#include "passworddatabase.h"

#include "core/apppaths.h"

#include <QDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlRecord>

static constexpr auto kConnectionName = "toolbox_password_sqlite";

namespace {

bool hasColumn(QSqlDatabase &database, const QString &table, const QString &column)
{
    QSqlQuery query(database);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(table)))
        return false;

    const auto nameIdx = query.record().indexOf("name");
    while (query.next()) {
        if (query.value(nameIdx).toString() == column)
            return true;
    }
    return false;
}

} // namespace

bool PasswordDatabase::open()
{
    QSqlDatabase database;
    if (QSqlDatabase::contains(kConnectionName)) {
        database = QSqlDatabase::database(kConnectionName);
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
        database.setDatabaseName(dbPath);
    }

    if (!database.isOpen() && !database.open())
        return false;

    return ensureSchema(database);
}

QSqlDatabase PasswordDatabase::db()
{
    return QSqlDatabase::database(kConnectionName);
}

bool PasswordDatabase::ensureSchema(QSqlDatabase &database)
{
    QSqlQuery query(database);

    if (!query.exec("PRAGMA foreign_keys = ON"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS vault_meta (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            kdf_salt BLOB NOT NULL,
            kdf_iterations INTEGER NOT NULL,
            verifier BLOB NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS groups (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_id INTEGER,
            name TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            UNIQUE(parent_id, name)
        )
    )sql"))
        return false;

    {
        const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
        query.prepare(R"sql(
            INSERT OR IGNORE INTO groups(id, parent_id, name, created_at, updated_at)
            VALUES(1, NULL, '全部', ?, ?)
        )sql");
        query.addBindValue(now);
        query.addBindValue(now);
        if (!query.exec())
            return false;
    }

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS password_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id INTEGER NOT NULL DEFAULT 1,
            entry_type INTEGER NOT NULL DEFAULT 0,
            title TEXT NOT NULL,
            username TEXT,
            password_enc BLOB NOT NULL,
            url TEXT,
            category TEXT,
            notes_enc BLOB,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!hasColumn(database, "password_entries", "group_id")) {
        if (!query.exec("ALTER TABLE password_entries ADD COLUMN group_id INTEGER NOT NULL DEFAULT 1"))
            return false;
    }

    if (!hasColumn(database, "password_entries", "entry_type")) {
        if (!query.exec("ALTER TABLE password_entries ADD COLUMN entry_type INTEGER NOT NULL DEFAULT 0"))
            return false;
    }

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_category
        ON password_entries(category)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_group_id
        ON password_entries(group_id)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_entry_type
        ON password_entries(entry_type)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_updated_at
        ON password_entries(updated_at DESC)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS entry_tags (
            entry_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            PRIMARY KEY(entry_id, tag_id),
            FOREIGN KEY(entry_id) REFERENCES password_entries(id) ON DELETE CASCADE,
            FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_entry_tags_tag_id
        ON entry_tags(tag_id)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS favicon_cache (
            host TEXT PRIMARY KEY,
            icon BLOB NOT NULL,
            content_type TEXT,
            fetched_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_favicon_cache_fetched_at
        ON favicon_cache(fetched_at DESC)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS pwned_prefix_cache (
            prefix TEXT PRIMARY KEY,
            body BLOB NOT NULL,
            fetched_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_pwned_prefix_cache_fetched_at
        ON pwned_prefix_cache(fetched_at DESC)
    )sql"))
        return false;

    return true;
}
