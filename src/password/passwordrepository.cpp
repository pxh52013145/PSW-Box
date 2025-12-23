#include "passwordrepository.h"

#include "core/crypto.h"
#include "core/database.h"
#include "passwordvault.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

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

    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return items;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, title, username, url, category, created_at, updated_at
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
        entry.title = query.value(1).toString();
        entry.username = query.value(2).toString();
        entry.url = query.value(3).toString();
        entry.category = query.value(4).toString();
        entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(5).toLongLong());
        entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
        items.push_back(entry);
    }

    return items;
}

QStringList PasswordRepository::listCategories() const
{
    QStringList categories;

    auto database = Database::db();
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

    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    const auto key = vault_->masterKey();
    const auto passwordEnc = Crypto::seal(key, secrets.password.toUtf8());
    const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(key, secrets.notes.toUtf8());
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    const auto createdAt = normalizeTs(createdAtSecs, now);
    const auto updatedAt = normalizeTs(updatedAtSecs, createdAt);

    QSqlQuery query(database);
    query.prepare(R"sql(
        INSERT INTO password_entries(title, username, password_enc, url, category, notes_enc, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?)
    )sql");
    query.addBindValue(secrets.entry.title);
    query.addBindValue(secrets.entry.username);
    query.addBindValue(passwordEnc);
    query.addBindValue(secrets.entry.url);
    query.addBindValue(secrets.entry.category);
    query.addBindValue(notesEnc);
    query.addBindValue(createdAt);
    query.addBindValue(updatedAt);

    if (!query.exec()) {
        setError(QString("新增失败：%1").arg(query.lastError().text()));
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

    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    if (secrets.entry.id <= 0) {
        setError("无效的 id");
        return false;
    }

    const auto key = vault_->masterKey();
    const auto passwordEnc = Crypto::seal(key, secrets.password.toUtf8());
    const auto notesEnc = secrets.notes.trimmed().isEmpty() ? QByteArray() : Crypto::seal(key, secrets.notes.toUtf8());
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();

    QSqlQuery query(database);
    query.prepare(R"sql(
        UPDATE password_entries
        SET title = ?, username = ?, password_enc = ?, url = ?, category = ?, notes_enc = ?, updated_at = ?
        WHERE id = ?
    )sql");
    query.addBindValue(secrets.entry.title);
    query.addBindValue(secrets.entry.username);
    query.addBindValue(passwordEnc);
    query.addBindValue(secrets.entry.url);
    query.addBindValue(secrets.entry.category);
    query.addBindValue(notesEnc);
    query.addBindValue(now);
    query.addBindValue(secrets.entry.id);

    if (!query.exec()) {
        setError(QString("更新失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool PasswordRepository::deleteEntry(qint64 id)
{
    auto database = Database::db();
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

    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return std::nullopt;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, title, username, password_enc, url, category, notes_enc, created_at, updated_at
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
    out.entry.title = query.value(1).toString();
    out.entry.username = query.value(2).toString();
    const auto passwordEnc = query.value(3).toByteArray();
    out.entry.url = query.value(4).toString();
    out.entry.category = query.value(5).toString();
    const auto notesEnc = query.value(6).toByteArray();
    out.entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(7).toLongLong());
    out.entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(8).toLongLong());

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
