#include "passwordfaviconservice.h"

#include "passworddatabase.h"

#include <QDateTime>
#include <QImage>
#include <QNetworkReply>
#include <QPixmap>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

#include <optional>

namespace {

constexpr qint64 kCacheTtlSecs = 14 * 86400;
constexpr qsizetype kMaxIconBytes = 256 * 1024;

std::optional<QIcon> decodeIcon(const QByteArray &bytes)
{
    QImage image;
    if (!image.loadFromData(bytes))
        return std::nullopt;

    const auto scaled = image.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(QPixmap::fromImage(scaled));
}

} // namespace

PasswordFaviconService::PasswordFaviconService(QObject *parent) : QObject(parent) {}

void PasswordFaviconService::setNetworkEnabled(bool enabled)
{
    networkEnabled_ = enabled;
}

QIcon PasswordFaviconService::iconForUrl(const QString &url)
{
    QString scheme;
    const auto host = hostForUrl(url, &scheme);
    if (host.isEmpty())
        return {};

    if (memory_.contains(host) && memory_.value(host).hasIcon) {
        const auto fetchedAt = memory_.value(host).fetchedAtSecs;
        const auto age = QDateTime::currentDateTime().toSecsSinceEpoch() - fetchedAt;
        if (age >= kCacheTtlSecs && networkEnabled_)
            ensureFetch(host, scheme);
        return memory_.value(host).icon;
    }

    CacheEntry entry;
    if (loadFromDatabase(host, entry) && entry.hasIcon) {
        memory_.insert(host, entry);
        const auto age = QDateTime::currentDateTime().toSecsSinceEpoch() - entry.fetchedAtSecs;
        if (age >= kCacheTtlSecs && networkEnabled_)
            ensureFetch(host, scheme);
        return entry.icon;
    }

    if (networkEnabled_)
        ensureFetch(host, scheme);
    return {};
}

QString PasswordFaviconService::hostForUrl(const QString &url, QString *schemeOut) const
{
    const auto trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return {};

    QUrl parsed(trimmed);
    if (parsed.scheme().isEmpty() && trimmed.contains('.') && !trimmed.contains("://"))
        parsed = QUrl(QString("https://%1").arg(trimmed));

    const auto scheme = parsed.scheme().toLower();
    if (schemeOut)
        *schemeOut = scheme;

    if (scheme != "http" && scheme != "https")
        return {};

    const auto host = parsed.host().trimmed().toLower();
    return host;
}

bool PasswordFaviconService::loadFromDatabase(const QString &host, CacheEntry &out)
{
    auto db = PasswordDatabase::db();
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare(R"sql(
        SELECT icon, fetched_at
        FROM favicon_cache
        WHERE host = ?
        LIMIT 1
    )sql");
    query.addBindValue(host);
    if (!query.exec() || !query.next())
        return false;

    const auto bytes = query.value(0).toByteArray();
    const auto fetchedAt = query.value(1).toLongLong();
    if (bytes.isEmpty())
        return false;

    const auto icon = decodeIcon(bytes);
    if (!icon.has_value())
        return false;

    out.icon = icon.value();
    out.fetchedAtSecs = fetchedAt;
    out.hasIcon = true;
    return true;
}

void PasswordFaviconService::saveToDatabase(const QString &host, const QByteArray &bytes, const QString &contentType, qint64 fetchedAtSecs)
{
    auto db = PasswordDatabase::db();
    if (!db.isOpen())
        return;

    QSqlQuery query(db);
    query.prepare(R"sql(
        INSERT OR REPLACE INTO favicon_cache(host, icon, content_type, fetched_at)
        VALUES(?, ?, ?, ?)
    )sql");
    query.addBindValue(host);
    query.addBindValue(bytes);
    query.addBindValue(contentType);
    query.addBindValue(fetchedAtSecs);
    query.exec();
}

void PasswordFaviconService::ensureFetch(const QString &host, const QString &scheme)
{
    if (host.isEmpty())
        return;
    if (!networkEnabled_)
        return;
    if (pending_.contains(host))
        return;

    pending_.insert(host);

    const auto s = (scheme == "http" || scheme == "https") ? scheme : QStringLiteral("https");
    const QUrl url(QString("%1://%2/favicon.ico").arg(s, host));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "ToolboxPassword/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, host]() {
        pending_.remove(host);

        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->readAll();
        const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const auto ok = (reply->error() == QNetworkReply::NoError) && status >= 200 && status < 300;
        reply->deleteLater();

        if (!ok)
            return;
        if (bytes.isEmpty() || bytes.size() > kMaxIconBytes)
            return;

        const auto icon = decodeIcon(bytes);
        if (!icon.has_value())
            return;

        CacheEntry entry;
        entry.icon = icon.value();
        entry.fetchedAtSecs = QDateTime::currentDateTime().toSecsSinceEpoch();
        entry.hasIcon = true;
        memory_.insert(host, entry);
        saveToDatabase(host, bytes, contentType, entry.fetchedAtSecs);
        emit iconUpdated(host);
    });
}
