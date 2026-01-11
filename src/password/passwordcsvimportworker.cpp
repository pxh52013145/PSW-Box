#include "passwordcsvimportworker.h"

#include "core/crypto.h"
#include "passwordcsv.h"
#include "passwordurl.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <optional>

namespace {

QString makeKey(const QString &title, const QString &username, const QString &url)
{
    const auto u = PasswordUrl::hostFromUrl(url);
    const auto user = username.trimmed().toLower();
    if (!u.isEmpty())
        return QString("%1\n%2").arg(u, user);
    return QString("%1\n%2").arg(title.trimmed().toLower(), user);
}

QString makeGroupKey(qint64 parentId, const QString &name)
{
    return QString("%1\n%2").arg(parentId).arg(name.trimmed().toLower());
}

bool upsertTag(QSqlDatabase &db, const QString &tag, qint64 &tagIdOut, QString &errorOut)
{
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();

    QSqlQuery insert(db);
    insert.prepare(R"sql(
        INSERT OR IGNORE INTO tags(name, created_at, updated_at)
        VALUES(?, ?, ?)
    )sql");
    insert.addBindValue(tag);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        errorOut = QString("写入 tags 失败：%1").arg(insert.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"sql(
        SELECT id FROM tags WHERE name = ? LIMIT 1
    )sql");
    query.addBindValue(tag);
    if (!query.exec() || !query.next()) {
        errorOut = QString("读取 tags 失败：%1").arg(query.lastError().text());
        return false;
    }

    tagIdOut = query.value(0).toLongLong();
    return true;
}

bool linkTags(QSqlDatabase &db, qint64 entryId, const QStringList &tags, QString &errorOut)
{
    if (tags.isEmpty())
        return true;

    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();

    for (const auto &tag : tags) {
        const auto trimmed = tag.trimmed();
        if (trimmed.isEmpty())
            continue;

        qint64 tagId = 0;
        if (!upsertTag(db, trimmed, tagId, errorOut))
            return false;

        QSqlQuery link(db);
        link.prepare(R"sql(
            INSERT OR IGNORE INTO entry_tags(entry_id, tag_id, created_at)
            VALUES(?, ?, ?)
        )sql");
        link.addBindValue(entryId);
        link.addBindValue(tagId);
        link.addBindValue(now);
        if (!link.exec()) {
            errorOut = QString("写入 entry_tags 失败：%1").arg(link.lastError().text());
            return false;
        }
    }

    return true;
}

bool replaceTags(QSqlDatabase &db, qint64 entryId, const QStringList &tags, QString &errorOut)
{
    QSqlQuery del(db);
    del.prepare("DELETE FROM entry_tags WHERE entry_id = ?");
    del.addBindValue(entryId);
    if (!del.exec()) {
        errorOut = QString("清理 entry_tags 失败：%1").arg(del.lastError().text());
        return false;
    }

    return linkTags(db, entryId, tags, errorOut);
}

std::optional<qint64> ensureGroup(QSqlDatabase &db,
                                 qint64 parentId,
                                 const QString &name,
                                 qint64 now,
                                 QHash<QString, qint64> &cache,
                                 QString &errorOut)
{
    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return parentId;

    if (parentId <= 0)
        parentId = 1;

    const auto cacheKey = makeGroupKey(parentId, trimmed);
    const auto it = cache.constFind(cacheKey);
    if (it != cache.constEnd())
        return it.value();

    QSqlQuery query(db);
    query.prepare(R"sql(
        SELECT id
        FROM groups
        WHERE parent_id = ?
          AND name = ? COLLATE NOCASE
        LIMIT 1
    )sql");
    query.addBindValue(parentId);
    query.addBindValue(trimmed);
    if (!query.exec()) {
        errorOut = QString("查询分组失败：%1").arg(query.lastError().text());
        return std::nullopt;
    }

    if (query.next()) {
        const auto id = query.value(0).toLongLong();
        cache.insert(cacheKey, id);
        return id;
    }

    QSqlQuery insert(db);
    insert.prepare(R"sql(
        INSERT INTO groups(parent_id, name, created_at, updated_at)
        VALUES(?, ?, ?, ?)
    )sql");
    insert.addBindValue(parentId);
    insert.addBindValue(trimmed);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        errorOut = QString("创建分组失败：%1").arg(insert.lastError().text());
        return std::nullopt;
    }

    const auto id = insert.lastInsertId().toLongLong();
    cache.insert(cacheKey, id);
    return id;
}

std::optional<qint64> ensureGroupPath(QSqlDatabase &db,
                                     qint64 baseGroupId,
                                     const QString &path,
                                     qint64 now,
                                     QHash<QString, qint64> &cache,
                                     QString &errorOut)
{
    auto normalized = path;
    normalized.replace('\\', '/');
    const auto parts = normalized.split('/', Qt::SkipEmptyParts);

    qint64 parentId = baseGroupId > 0 ? baseGroupId : 1;
    for (const auto &part : parts) {
        const auto id = ensureGroup(db, parentId, part, now, cache, errorOut);
        if (!id.has_value())
            return std::nullopt;
        parentId = id.value();
    }

    return parentId;
}

} // namespace

PasswordCsvImportWorker::PasswordCsvImportWorker(QString csvPath,
                                                 QString dbPath,
                                                 QByteArray masterKey,
                                                 qint64 defaultGroupId,
                                                 QObject *parent)
    : QObject(parent),
      csvPath_(std::move(csvPath)),
      dbPath_(std::move(dbPath)),
      masterKey_(std::move(masterKey)),
      defaultGroupId_(defaultGroupId > 0 ? defaultGroupId : 1)
{
}

PasswordCsvImportWorker::~PasswordCsvImportWorker()
{
    Crypto::secureZero(masterKey_);
}

void PasswordCsvImportWorker::setOptions(PasswordCsvImportOptions options)
{
    options_ = options;
}

void PasswordCsvImportWorker::requestCancel()
{
    cancelRequested_.store(true);
}

void PasswordCsvImportWorker::run()
{
    QFile in(csvPath_);
    if (!in.open(QIODevice::ReadOnly)) {
        emit failed("无法读取 CSV 文件");
        return;
    }

    const auto parse = parsePasswordCsv(in.readAll(), defaultGroupId_);
    if (!parse.ok()) {
        emit failed(parse.error);
        return;
    }

    emit progressRangeChanged(0, parse.entries.size());

    const auto connectionName = QString("toolbox_password_csv_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    int inserted = 0;
    int updated = 0;
    int skippedDup = 0;
    int skippedInvalid = parse.skippedInvalid + parse.skippedEmpty;
    QString error;
    bool ok = true;

    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(dbPath_);
        if (!db.open()) {
            error = QString("打开数据库失败：%1").arg(db.lastError().text());
            ok = false;
        }

        if (ok) {
            QSqlQuery pragma(db);
            pragma.exec("PRAGMA foreign_keys = ON");
        }

        QHash<QString, qint64> existingIds;
        if (ok) {
            QSqlQuery query(db);
            query.prepare(R"sql(
                SELECT id, title, username, url
                FROM password_entries
            )sql");
            if (!query.exec()) {
                error = QString("读取现有条目失败：%1").arg(query.lastError().text());
                ok = false;
            } else {
                while (query.next()) {
                    const auto id = query.value(0).toLongLong();
                    const auto key = makeKey(query.value(1).toString(), query.value(2).toString(), query.value(3).toString());
                    if (!key.isEmpty() && !existingIds.contains(key))
                        existingIds.insert(key, id);
                }
            }
        }

        bool transactionStarted = false;
        if (ok) {
            if (!db.transaction()) {
                error = QString("开启事务失败：%1").arg(db.lastError().text());
                ok = false;
            } else {
                transactionStarted = true;
            }
        }

        const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
        QHash<QString, qint64> groupCache;

        for (int i = 0; ok && i < parse.entries.size(); ++i) {
            if (cancelRequested_.load()) {
                error = "导入已取消";
                ok = false;
                break;
            }

            auto secrets = parse.entries.at(i);
            if (secrets.entry.title.trimmed().isEmpty() || secrets.password.isEmpty()) {
                skippedInvalid++;
                emit progressValueChanged(i + 1);
                continue;
            }

            auto groupId = secrets.entry.groupId > 0 ? secrets.entry.groupId : defaultGroupId_;
            if (options_.createGroupsFromCategoryPath && !secrets.entry.category.trimmed().isEmpty()) {
                const auto ensured =
                    ensureGroupPath(db, groupId, secrets.entry.category.trimmed(), now, groupCache, error);
                if (!ensured.has_value()) {
                    ok = false;
                    break;
                }
                groupId = ensured.value();
            }

            const auto key = makeKey(secrets.entry.title, secrets.entry.username, secrets.entry.url);
            const auto existingIt = existingIds.constFind(key);
            const bool exists = existingIt != existingIds.constEnd();

            if (exists && options_.duplicatePolicy == PasswordCsvDuplicatePolicy::Skip) {
                skippedDup++;
                emit progressValueChanged(i + 1);
                continue;
            }

            const auto passwordEnc = Crypto::seal(masterKey_, secrets.password.toUtf8());
            const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(masterKey_, secrets.notes.toUtf8());

            if (exists && options_.duplicatePolicy == PasswordCsvDuplicatePolicy::Update) {
                const auto entryId = existingIt.value();

                QString existingUrl;
                QString existingCategory;
                {
                    QSqlQuery q(db);
                    q.prepare("SELECT url, category FROM password_entries WHERE id = ? LIMIT 1");
                    q.addBindValue(entryId);
                    if (!q.exec() || !q.next()) {
                        error = QString("读取重复条目失败：%1").arg(q.lastError().text());
                        ok = false;
                        break;
                    }
                    existingUrl = q.value(0).toString();
                    existingCategory = q.value(1).toString();
                }

                const auto finalUrl = existingUrl.trimmed().isEmpty() ? secrets.entry.url : existingUrl;
                const auto finalCategory = existingCategory.trimmed().isEmpty() ? secrets.entry.category : existingCategory;

                QSqlQuery upd(db);
                upd.prepare(R"sql(
                    UPDATE password_entries
                    SET group_id = ?,
                        entry_type = ?,
                        password_enc = ?,
                        url = ?,
                        category = ?,
                        notes_enc = ?,
                        updated_at = ?
                    WHERE id = ?
                )sql");
                upd.addBindValue(groupId);
                upd.addBindValue(static_cast<int>(options_.defaultEntryType));
                upd.addBindValue(passwordEnc);
                upd.addBindValue(finalUrl);
                upd.addBindValue(finalCategory);
                upd.addBindValue(notesEnc);
                upd.addBindValue(now);
                upd.addBindValue(entryId);

                if (!upd.exec()) {
                    error = QString("更新重复条目失败：%1").arg(upd.lastError().text());
                    ok = false;
                    break;
                }

                QString tagError;
                if (!replaceTags(db, entryId, secrets.entry.tags, tagError)) {
                    error = tagError;
                    ok = false;
                    break;
                }

                updated++;
            } else {
                QSqlQuery insert(db);
                insert.prepare(R"sql(
                    INSERT INTO password_entries(group_id, entry_type, title, username, password_enc, url, category, notes_enc, created_at, updated_at)
                    VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                )sql");
                insert.addBindValue(groupId);
                insert.addBindValue(static_cast<int>(options_.defaultEntryType));
                insert.addBindValue(secrets.entry.title);
                insert.addBindValue(secrets.entry.username);
                insert.addBindValue(passwordEnc);
                insert.addBindValue(secrets.entry.url);
                insert.addBindValue(secrets.entry.category);
                insert.addBindValue(notesEnc);
                insert.addBindValue(now);
                insert.addBindValue(now);

                if (!insert.exec()) {
                    error = QString("写入条目失败：%1").arg(insert.lastError().text());
                    ok = false;
                    break;
                }

                const auto entryId = insert.lastInsertId().toLongLong();
                if (!key.isEmpty() && !existingIds.contains(key))
                    existingIds.insert(key, entryId);

                QString tagError;
                if (!linkTags(db, entryId, secrets.entry.tags, tagError)) {
                    error = tagError;
                    ok = false;
                    break;
                }

                inserted++;
            }
            emit progressValueChanged(i + 1);
        }

        if (transactionStarted) {
            if (ok) {
                if (!db.commit()) {
                    db.rollback();
                    error = QString("提交事务失败：%1").arg(db.lastError().text());
                    ok = false;
                }
            } else {
                db.rollback();
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    if (!ok) {
        emit failed(error.isEmpty() ? "导入失败" : error);
        return;
    }

    emit finished(inserted, updated, skippedDup, skippedInvalid, parse.warnings);
}
