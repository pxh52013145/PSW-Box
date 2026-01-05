#include "passwordhealthworker.h"

#include "core/crypto.h"
#include "passwordstrength.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUuid>

#include <optional>

namespace {

constexpr int kStaleDaysThreshold = 90;
constexpr qint64 kPwnedCacheTtlSecs = 30 * 86400;
constexpr int kPwnedTimeoutMs = 8000;
constexpr qsizetype kMaxPwnedBodyBytes = 2 * 1024 * 1024;

QByteArray sha1Hex(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().toUpper();
}

bool loadPwnedCache(QSqlDatabase &db, const QByteArray &prefix, QByteArray &bodyOut, qint64 &fetchedAtOut)
{
    QSqlQuery query(db);
    query.prepare(R"sql(
        SELECT body, fetched_at
        FROM pwned_prefix_cache
        WHERE prefix = ?
        LIMIT 1
    )sql");
    query.addBindValue(QString::fromLatin1(prefix));
    if (!query.exec() || !query.next())
        return false;

    bodyOut = query.value(0).toByteArray();
    fetchedAtOut = query.value(1).toLongLong();
    return !bodyOut.isEmpty();
}

void savePwnedCache(QSqlDatabase &db, const QByteArray &prefix, const QByteArray &body, qint64 fetchedAt)
{
    QSqlQuery query(db);
    query.prepare(R"sql(
        INSERT OR REPLACE INTO pwned_prefix_cache(prefix, body, fetched_at)
        VALUES(?, ?, ?)
    )sql");
    query.addBindValue(QString::fromLatin1(prefix));
    query.addBindValue(body);
    query.addBindValue(fetchedAt);
    query.exec();
}

std::optional<QByteArray> fetchPwnedRange(QNetworkAccessManager &net, const QByteArray &prefix, QString &errorOut)
{
    QNetworkRequest req(QUrl(QString("https://api.pwnedpasswords.com/range/%1").arg(QString::fromLatin1(prefix))));
    req.setHeader(QNetworkRequest::UserAgentHeader, "ToolboxPassword/1.0");
    req.setRawHeader("Add-Padding", "true");
    req.setRawHeader("Accept", "text/plain");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = net.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(kPwnedTimeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        errorOut = "泄露检查超时";
        reply->deleteLater();
        return std::nullopt;
    }

    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto bytes = reply->readAll();
    const auto ok = (reply->error() == QNetworkReply::NoError) && status >= 200 && status < 300;
    if (!ok) {
        errorOut = QString("泄露检查失败：%1").arg(reply->errorString());
        reply->deleteLater();
        return std::nullopt;
    }

    reply->deleteLater();

    if (bytes.isEmpty()) {
        errorOut = "泄露检查响应为空";
        return std::nullopt;
    }
    if (bytes.size() > kMaxPwnedBodyBytes) {
        errorOut = "泄露检查响应过大";
        return std::nullopt;
    }

    return bytes;
}

QHash<QByteArray, qint64> parsePwnedBody(const QByteArray &body)
{
    QHash<QByteArray, qint64> suffixCounts;

    const auto lines = body.split('\n');
    for (auto line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const auto colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const auto suffix = line.left(colon).trimmed().toUpper();
        bool ok = false;
        const auto count = line.mid(colon + 1).trimmed().toLongLong(&ok);
        if (!ok)
            continue;
        suffixCounts.insert(suffix, count);
    }

    return suffixCounts;
}

} // namespace

PasswordHealthWorker::PasswordHealthWorker(QString dbPath,
                                           QByteArray masterKey,
                                           bool enablePwnedCheck,
                                           bool allowNetwork,
                                           QObject *parent)
    : QObject(parent),
      dbPath_(std::move(dbPath)),
      masterKey_(std::move(masterKey)),
      enablePwnedCheck_(enablePwnedCheck),
      allowNetwork_(allowNetwork)
{
}

PasswordHealthWorker::~PasswordHealthWorker()
{
    Crypto::secureZero(masterKey_);
}

void PasswordHealthWorker::requestCancel()
{
    cancelRequested_.store(true);
}

void PasswordHealthWorker::run()
{
    const auto connectionName = QString("toolbox_password_health_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVector<PasswordHealthItem> items;
    QVector<QByteArray> passwordHashes;
    QVector<QByteArray> sha1Hexes;
    QString error;
    bool ok = true;

    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(dbPath_);
        if (!db.open()) {
            error = QString("打开数据库失败：%1").arg(db.lastError().text());
            ok = false;
        }

        int total = 0;
        if (ok) {
            QSqlQuery countQuery(db);
            if (!countQuery.exec("SELECT COUNT(1) FROM password_entries") || !countQuery.next()) {
                error = QString("统计条目失败：%1").arg(countQuery.lastError().text());
                ok = false;
            } else {
                total = countQuery.value(0).toInt();
            }
        }

        if (ok)
            emit progressRangeChanged(0, total);

        QSqlQuery query(db);
        query.prepare(R"sql(
            SELECT
                e.id,
                e.group_id,
                e.title,
                e.username,
                e.url,
                e.category,
                e.updated_at,
                e.password_enc,
                GROUP_CONCAT(t.name, ',') AS tags_csv
            FROM password_entries e
            LEFT JOIN entry_tags et ON et.entry_id = e.id
            LEFT JOIN tags t ON t.id = et.tag_id
            GROUP BY e.id
            ORDER BY e.updated_at DESC
        )sql");

        if (ok && !query.exec()) {
            error = QString("读取条目失败：%1").arg(query.lastError().text());
            ok = false;
        }

        const auto nowSecs = QDateTime::currentDateTime().toSecsSinceEpoch();
        int index = 0;
        while (ok && query.next()) {
            if (cancelRequested_.load())
                break;

            PasswordHealthItem item;
            item.entryId = query.value(0).toLongLong();
            item.groupId = query.value(1).toLongLong();
            item.title = query.value(2).toString();
            item.username = query.value(3).toString();
            item.url = query.value(4).toString();
            item.category = query.value(5).toString();
            item.updatedAtSecs = query.value(6).toLongLong();
            const auto passwordEnc = query.value(7).toByteArray();
            const auto tagsCsv = query.value(8).toString();
            if (!tagsCsv.trimmed().isEmpty()) {
                for (const auto &t : tagsCsv.split(',', Qt::SkipEmptyParts))
                    item.tags.push_back(t.trimmed());
            }

            const auto ageDays = item.updatedAtSecs > 0 ? static_cast<int>((nowSecs - item.updatedAtSecs) / 86400) : 0;
            item.daysSinceUpdate = qMax(0, ageDays);
            item.stale = item.daysSinceUpdate >= kStaleDaysThreshold;

            QByteArray hash;
            const auto plain = Crypto::open(masterKey_, passwordEnc);
            if (!plain.has_value()) {
                item.corrupted = true;
                item.strengthScore = 0;
                item.weak = true;
            } else {
                const auto pwd = QString::fromUtf8(plain.value());
                const auto strength = evaluatePasswordStrength(pwd);
                item.strengthScore = strength.score;
                item.weak = strength.score < 40;
                hash = Crypto::sha256(plain.value());
                sha1Hexes.push_back(sha1Hex(plain.value()));
            }

            items.push_back(item);
            passwordHashes.push_back(hash);
            if (!plain.has_value())
                sha1Hexes.push_back({});

            index++;
            emit progressValueChanged(index);
        }

        if (enablePwnedCheck_ && ok) {
            QHash<QByteArray, QVector<int>> prefixToIndices;
            for (int i = 0; i < sha1Hexes.size() && i < items.size(); ++i) {
                const auto &hex = sha1Hexes.at(i);
                if (hex.size() != 40)
                    continue;
                const auto prefix = hex.left(5);
                prefixToIndices[prefix].push_back(i);
            }

            if (!prefixToIndices.isEmpty()) {
                emit progressRangeChanged(0, index + prefixToIndices.size());

                QNetworkAccessManager net;
                QHash<QByteArray, QByteArray> bodyCache;

                qint64 pwnedCacheErrors = 0;
                qint64 pwnedNetworkErrors = 0;
                int prefixDone = 0;

                for (auto it = prefixToIndices.begin(); it != prefixToIndices.end(); ++it) {
                    if (cancelRequested_.load())
                        break;

                    const auto prefix = it.key();
                    QByteArray body;
                    qint64 fetchedAt = 0;
                    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();

                    bool haveBody = false;
                    if (bodyCache.contains(prefix)) {
                        body = bodyCache.value(prefix);
                        haveBody = !body.isEmpty();
                    } else {
                        if (loadPwnedCache(db, prefix, body, fetchedAt)) {
                            const auto age = now - fetchedAt;
                            if (age >= 0 && age <= kPwnedCacheTtlSecs) {
                                bodyCache.insert(prefix, body);
                                haveBody = true;
                            }
                        }

                        if (!haveBody && allowNetwork_) {
                            QString fetchError;
                            const auto fetched = fetchPwnedRange(net, prefix, fetchError);
                            if (fetched.has_value()) {
                                body = fetched.value();
                                bodyCache.insert(prefix, body);
                                savePwnedCache(db, prefix, body, now);
                                haveBody = true;
                            } else {
                                pwnedNetworkErrors++;
                            }
                        } else if (!haveBody) {
                            pwnedCacheErrors++;
                        }
                    }

                    if (haveBody) {
                        const auto suffixCounts = parsePwnedBody(body);
                        for (const auto idx : it.value()) {
                            if (idx < 0 || idx >= items.size())
                                continue;
                            const auto suffix = sha1Hexes.at(idx).mid(5).toUpper();
                            items[idx].pwnedChecked = true;
                            const auto count = suffixCounts.value(suffix, 0);
                            if (count > 0) {
                                items[idx].pwned = true;
                                items[idx].pwnedCount = count;
                            }
                        }
                    }

                    prefixDone++;
                    emit progressValueChanged(index + prefixDone);
                }

                Q_UNUSED(pwnedCacheErrors);
                Q_UNUSED(pwnedNetworkErrors);
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    if (!ok) {
        emit failed(error.isEmpty() ? "扫描失败" : error);
        return;
    }

    QHash<QByteArray, int> counts;
    for (const auto &h : passwordHashes) {
        if (h.isEmpty())
            continue;
        counts[h] = counts.value(h, 0) + 1;
    }

    for (int i = 0; i < items.size() && i < passwordHashes.size(); ++i) {
        const auto &h = passwordHashes.at(i);
        if (h.isEmpty())
            continue;
        const auto count = counts.value(h, 0);
        if (count > 1) {
            items[i].reused = true;
            items[i].reuseCount = count;
        }
    }

    emit finished(items);
}
