#include "passwordrepository.h"

#include "core/crypto.h"
#include "passworddatabase.h"
#include "passwordvault.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>

namespace {

bool isVaultAvailable(PasswordVault *vault)
{
    return vault && vault->isUnlocked();
}

qint64 normalizeTs(qint64 ts, qint64 fallback)
{
    if (ts <= 0)
        return fallback;
    return ts;
}

QStringList normalizeTags(const QStringList &tags)
{
    QStringList out;
    QSet<QString> seen;

    for (const auto &tag : tags) {
        const auto trimmed = tag.trimmed();
        if (trimmed.isEmpty())
            continue;

        const auto key = trimmed.toLower();
        if (seen.contains(key))
            continue;

        seen.insert(key);
        out.push_back(trimmed);
    }

    return out;
}

bool replaceEntryTags(QSqlDatabase &database, qint64 entryId, const QStringList &tags, QString &errorOut)
{
    QSqlQuery del(database);
    del.prepare(R"sql(
        DELETE FROM entry_tags WHERE entry_id = ?
    )sql");
    del.addBindValue(entryId);
    if (!del.exec()) {
        errorOut = QString("清空标签关联失败：%1").arg(del.lastError().text());
        return false;
    }

    if (tags.isEmpty())
        return true;

    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();

    for (const auto &tag : tags) {
        QSqlQuery insertTag(database);
        insertTag.prepare(R"sql(
            INSERT OR IGNORE INTO tags(name, created_at, updated_at)
            VALUES(?, ?, ?)
        )sql");
        insertTag.addBindValue(tag);
        insertTag.addBindValue(now);
        insertTag.addBindValue(now);
        if (!insertTag.exec()) {
            errorOut = QString("创建标签失败：%1").arg(insertTag.lastError().text());
            return false;
        }

        QSqlQuery queryTagId(database);
        queryTagId.prepare(R"sql(
            SELECT id
            FROM tags
            WHERE name = ?
            LIMIT 1
        )sql");
        queryTagId.addBindValue(tag);
        if (!queryTagId.exec() || !queryTagId.next()) {
            errorOut = QString("读取标签失败：%1").arg(queryTagId.lastError().text());
            return false;
        }

        const auto tagId = queryTagId.value(0).toLongLong();

        QSqlQuery link(database);
        link.prepare(R"sql(
            INSERT OR IGNORE INTO entry_tags(entry_id, tag_id, created_at)
            VALUES(?, ?, ?)
        )sql");
        link.addBindValue(entryId);
        link.addBindValue(tagId);
        link.addBindValue(now);
        if (!link.exec()) {
            errorOut = QString("关联标签失败：%1").arg(link.lastError().text());
            return false;
        }
    }

    return true;
}

} // namespace

PasswordRepository::PasswordRepository(PasswordVault *vault) : vault_(vault) {}

QString PasswordRepository::lastError() const
{
    return lastError_;
}

void PasswordRepository::setError(const QString &error) const
{
    lastError_ = error;
}

QVector<PasswordEntry> PasswordRepository::listEntries() const
{
    QVector<PasswordEntry> items;

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return items;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, group_id, entry_type, title, username, url, category, created_at, updated_at
        FROM password_entries
        ORDER BY updated_at DESC
    )sql");

    if (!query.exec()) {
        setError(QString("查询失败：%1").arg(query.lastError().text()));
        return items;
    }

    while (query.next()) {
        PasswordEntry entry;
        entry.id = query.value(0).toLongLong();
        entry.groupId = query.value(1).toLongLong();
        entry.type = passwordEntryTypeFromInt(query.value(2).toInt());
        entry.title = query.value(3).toString();
        entry.username = query.value(4).toString();
        entry.url = query.value(5).toString();
        entry.category = query.value(6).toString();
        entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(7).toLongLong());
        entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(8).toLongLong());
        items.push_back(entry);
    }

    return items;
}

QStringList PasswordRepository::listCategories() const
{
    QStringList categories;

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return categories;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT DISTINCT category
        FROM password_entries
        WHERE category IS NOT NULL AND category <> ''
        ORDER BY category ASC
    )sql");

    if (!query.exec()) {
        setError(QString("查询分类失败：%1").arg(query.lastError().text()));
        return categories;
    }

    while (query.next())
        categories.push_back(query.value(0).toString());

    return categories;
}

QVector<PasswordGroup> PasswordRepository::listGroups() const
{
    QVector<PasswordGroup> groups;

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return groups;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, parent_id, name
        FROM groups
        ORDER BY name COLLATE NOCASE ASC
    )sql");

    if (!query.exec()) {
        setError(QString("查询分组失败：%1").arg(query.lastError().text()));
        return groups;
    }

    while (query.next()) {
        PasswordGroup group;
        group.id = query.value(0).toLongLong();
        group.parentId = query.value(1).isNull() ? 0 : query.value(1).toLongLong();
        group.name = query.value(2).toString();
        groups.push_back(group);
    }

    return groups;
}

QStringList PasswordRepository::listAllTags() const
{
    QStringList tags;

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return tags;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT name
        FROM tags
        ORDER BY name COLLATE NOCASE ASC
    )sql");

    if (!query.exec()) {
        setError(QString("查询标签失败：%1").arg(query.lastError().text()));
        return tags;
    }

    while (query.next())
        tags.push_back(query.value(0).toString());

    return tags;
}

std::optional<qint64> PasswordRepository::createGroup(qint64 parentId, const QString &name)
{
    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return std::nullopt;
    }

    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setError("分组名不能为空");
        return std::nullopt;
    }

    if (parentId <= 0)
        parentId = 1;

    QSqlQuery query(database);
    query.prepare(R"sql(
        INSERT INTO groups(parent_id, name, created_at, updated_at)
        VALUES(?, ?, ?, ?)
    )sql");
    query.addBindValue(parentId);
    query.addBindValue(trimmed);
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.addBindValue(now);
    query.addBindValue(now);

    if (!query.exec()) {
        setError(QString("新增分组失败：%1").arg(query.lastError().text()));
        return std::nullopt;
    }

    return query.lastInsertId().toLongLong();
}

bool PasswordRepository::renameGroup(qint64 groupId, const QString &name)
{
    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (groupId <= 0) {
        setError("无效的分组 id");
        return false;
    }

    if (groupId == 1) {
        setError("根分组不允许重命名");
        return false;
    }

    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setError("分组名不能为空");
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        UPDATE groups
        SET name = ?, updated_at = ?
        WHERE id = ?
    )sql");
    query.addBindValue(trimmed);
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
    query.addBindValue(groupId);

    if (!query.exec()) {
        setError(QString("重命名失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::deleteGroup(qint64 groupId)
{
    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (groupId <= 0) {
        setError("无效的分组 id");
        return false;
    }

    if (groupId == 1) {
        setError("根分组不允许删除");
        return false;
    }

    QSqlQuery check(database);
    check.prepare(R"sql(
        SELECT
            (SELECT COUNT(1) FROM groups WHERE parent_id = ?) AS child_count,
            (SELECT COUNT(1) FROM password_entries WHERE group_id = ?) AS entry_count
    )sql");
    check.addBindValue(groupId);
    check.addBindValue(groupId);
    if (!check.exec() || !check.next()) {
        setError(QString("检查分组失败：%1").arg(check.lastError().text()));
        return false;
    }

    const auto childCount = check.value(0).toInt();
    const auto entryCount = check.value(1).toInt();
    if (childCount > 0) {
        setError("该分组下存在子分组，无法删除");
        return false;
    }
    if (entryCount > 0) {
        setError("该分组下存在条目，无法删除");
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        DELETE FROM groups WHERE id = ?
    )sql");
    query.addBindValue(groupId);
    if (!query.exec()) {
        setError(QString("删除分组失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::addEntry(const PasswordEntrySecrets &secrets)
{
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    return addEntryWithTimestamps(secrets, now, now);
}

bool PasswordRepository::addEntryWithTimestamps(const PasswordEntrySecrets &secrets, qint64 createdAtSecs, qint64 updatedAtSecs)
{
    if (!isVaultAvailable(vault_)) {
        setError("Vault 未解锁");
        return false;
    }

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (!database.transaction()) {
        setError(QString("开启事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    const auto key = vault_->masterKey();
    const auto passwordEnc = Crypto::seal(key, secrets.password.toUtf8());
    const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(key, secrets.notes.toUtf8());
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    const auto createdAt = normalizeTs(createdAtSecs, now);
    const auto updatedAt = normalizeTs(updatedAtSecs, createdAt);
    const auto groupId = secrets.entry.groupId > 0 ? secrets.entry.groupId : 1;
    const auto entryType = static_cast<int>(secrets.entry.type);

    QSqlQuery query(database);
    query.prepare(R"sql(
        INSERT INTO password_entries(group_id, entry_type, title, username, password_enc, url, category, notes_enc, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");
    query.addBindValue(groupId);
    query.addBindValue(entryType);
    query.addBindValue(secrets.entry.title);
    query.addBindValue(secrets.entry.username);
    query.addBindValue(passwordEnc);
    query.addBindValue(secrets.entry.url);
    query.addBindValue(secrets.entry.category);
    query.addBindValue(notesEnc);
    query.addBindValue(createdAt);
    query.addBindValue(updatedAt);

    if (!query.exec()) {
        database.rollback();
        setError(QString("新增失败：%1").arg(query.lastError().text()));
        return false;
    }

    const auto entryId = query.lastInsertId().toLongLong();
    QString tagsError;
    if (!replaceEntryTags(database, entryId, normalizeTags(secrets.entry.tags), tagsError)) {
        database.rollback();
        setError(tagsError);
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        setError(QString("提交事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::updateEntry(const PasswordEntrySecrets &secrets)
{
    if (!isVaultAvailable(vault_)) {
        setError("Vault 未解锁");
        return false;
    }

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (secrets.entry.id <= 0) {
        setError("无效的 id");
        return false;
    }

    if (!database.transaction()) {
        setError(QString("开启事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    const auto key = vault_->masterKey();
    const auto passwordEnc = Crypto::seal(key, secrets.password.toUtf8());
    const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(key, secrets.notes.toUtf8());
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    const auto groupId = secrets.entry.groupId > 0 ? secrets.entry.groupId : 1;
    const auto entryType = static_cast<int>(secrets.entry.type);

    QSqlQuery query(database);
    query.prepare(R"sql(
        UPDATE password_entries
        SET group_id = ?, entry_type = ?, title = ?, username = ?, password_enc = ?, url = ?, category = ?, notes_enc = ?, updated_at = ?
        WHERE id = ?
    )sql");
    query.addBindValue(groupId);
    query.addBindValue(entryType);
    query.addBindValue(secrets.entry.title);
    query.addBindValue(secrets.entry.username);
    query.addBindValue(passwordEnc);
    query.addBindValue(secrets.entry.url);
    query.addBindValue(secrets.entry.category);
    query.addBindValue(notesEnc);
    query.addBindValue(now);
    query.addBindValue(secrets.entry.id);

    if (!query.exec()) {
        database.rollback();
        setError(QString("更新失败：%1").arg(query.lastError().text()));
        return false;
    }

    QString tagsError;
    if (!replaceEntryTags(database, secrets.entry.id, normalizeTags(secrets.entry.tags), tagsError)) {
        database.rollback();
        setError(tagsError);
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        setError(QString("提交事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::moveEntryToGroup(qint64 entryId, qint64 groupId)
{
    if (!isVaultAvailable(vault_)) {
        setError("Vault 未解锁");
        return false;
    }

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (entryId <= 0 || groupId <= 0) {
        setError("无效的参数");
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        UPDATE password_entries
        SET group_id = ?, updated_at = ?
        WHERE id = ?
    )sql");
    query.addBindValue(groupId);
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
    query.addBindValue(entryId);

    if (!query.exec()) {
        setError(QString("移动失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::deleteEntry(qint64 id)
{
    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        DELETE FROM password_entries WHERE id = ?
    )sql");
    query.addBindValue(id);

    if (!query.exec()) {
        setError(QString("删除失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

std::optional<PasswordEntrySecrets> PasswordRepository::loadEntry(qint64 id) const
{
    if (!isVaultAvailable(vault_)) {
        setError("Vault 未解锁");
        return std::nullopt;
    }

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return std::nullopt;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, group_id, entry_type, title, username, password_enc, url, category, notes_enc, created_at, updated_at
        FROM password_entries
        WHERE id = ?
        LIMIT 1
    )sql");
    query.addBindValue(id);

    if (!query.exec()) {
        setError(QString("查询失败：%1").arg(query.lastError().text()));
        return std::nullopt;
    }

    if (!query.next()) {
        setError("未找到条目");
        return std::nullopt;
    }

    PasswordEntrySecrets out;
    out.entry.id = query.value(0).toLongLong();
    out.entry.groupId = query.value(1).toLongLong();
    out.entry.type = passwordEntryTypeFromInt(query.value(2).toInt());
    out.entry.title = query.value(3).toString();
    out.entry.username = query.value(4).toString();
    const auto passwordEnc = query.value(5).toByteArray();
    out.entry.url = query.value(6).toString();
    out.entry.category = query.value(7).toString();
    const auto notesEnc = query.value(8).toByteArray();
    out.entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(9).toLongLong());
    out.entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(10).toLongLong());

    QSqlQuery tagQuery(database);
    tagQuery.prepare(R"sql(
        SELECT t.name
        FROM tags t
        INNER JOIN entry_tags et ON et.tag_id = t.id
        WHERE et.entry_id = ?
        ORDER BY t.name COLLATE NOCASE ASC
    )sql");
    tagQuery.addBindValue(id);
    if (!tagQuery.exec()) {
        setError(QString("读取标签失败：%1").arg(tagQuery.lastError().text()));
        return std::nullopt;
    }
    while (tagQuery.next())
        out.entry.tags.push_back(tagQuery.value(0).toString());

    const auto key = vault_->masterKey();
    const auto passwordPlain = Crypto::open(key, passwordEnc);
    if (!passwordPlain.has_value()) {
        setError("解密失败：密码数据损坏或主密码不匹配");
        return std::nullopt;
    }
    out.password = QString::fromUtf8(passwordPlain.value());

    if (!notesEnc.isEmpty()) {
        const auto notesPlain = Crypto::open(key, notesEnc);
        if (!notesPlain.has_value()) {
            setError("解密失败：备注数据损坏或主密码不匹配");
            return std::nullopt;
        }
        out.notes = QString::fromUtf8(notesPlain.value());
    }

    return out;
}
