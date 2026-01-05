#include "passwordcsvimportworker.h"

#include "core/crypto.h"
#include "passwordcsv.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {

QString makeKey(const QString &title, const QString &username, const QString &url)
{
    const auto u = url.trimmed().toLower();
    const auto user = username.trimmed().toLower();
    if (!u.isEmpty())
        return QString("%1\n%2").arg(u, user);
    return QString("%1\n%2").arg(title.trimmed().toLower(), user);
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
    int imported = 0;
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

        QSet<QString> existing;
        if (ok) {
            QSqlQuery query(db);
            query.prepare(R"sql(
                SELECT title, username, url
                FROM password_entries
            )sql");
            if (!query.exec()) {
                error = QString("读取现有条目失败：%1").arg(query.lastError().text());
                ok = false;
            } else {
                while (query.next()) {
                    existing.insert(makeKey(query.value(0).toString(), query.value(1).toString(), query.value(2).toString()));
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

        for (int i = 0; ok && i < parse.entries.size(); ++i) {
            if (cancelRequested_.load()) {
                error = "导入已取消";
                ok = false;
                break;
            }

            const auto &secrets = parse.entries.at(i);
            const auto key = makeKey(secrets.entry.title, secrets.entry.username, secrets.entry.url);
            if (existing.contains(key)) {
                skippedDup++;
                emit progressValueChanged(i + 1);
                continue;
            }

            if (secrets.entry.title.trimmed().isEmpty() || secrets.password.isEmpty()) {
                skippedInvalid++;
                emit progressValueChanged(i + 1);
                continue;
            }

            existing.insert(key);

            const auto passwordEnc = Crypto::seal(masterKey_, secrets.password.toUtf8());
            const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(masterKey_, secrets.notes.toUtf8());

            QSqlQuery insert(db);
            insert.prepare(R"sql(
                INSERT INTO password_entries(group_id, title, username, password_enc, url, category, notes_enc, created_at, updated_at)
                VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
            )sql");
            insert.addBindValue(secrets.entry.groupId > 0 ? secrets.entry.groupId : defaultGroupId_);
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
            QString tagError;
            if (!linkTags(db, entryId, secrets.entry.tags, tagError)) {
                error = tagError;
                ok = false;
                break;
            }

            imported++;
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

    emit finished(imported, skippedDup, skippedInvalid, parse.warnings);
}
