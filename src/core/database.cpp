#include "database.h"

#include "apppaths.h"

#include <QSqlError>
#include <QSqlQuery>

static constexpr auto kConnectionName = "toolbox_sqlite";

bool Database::open()
{
    auto database = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    database.setDatabaseName(AppPaths::databaseFilePath());

    if (!database.open())
        return false;

    return ensureSchema(database);
}

QSqlDatabase Database::db()
{
    return QSqlDatabase::database(kConnectionName);
}

bool Database::ensureSchema(QSqlDatabase &database)
{
    QSqlQuery query(database);

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
        CREATE TABLE IF NOT EXISTS password_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
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

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_category
        ON password_entries(category)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_password_entries_updated_at
        ON password_entries(updated_at DESC)
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE TABLE IF NOT EXISTS translation_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_text TEXT NOT NULL,
            translated_text TEXT NOT NULL,
            source_lang TEXT,
            target_lang TEXT,
            provider TEXT,
            created_at INTEGER NOT NULL
        )
    )sql"))
        return false;

    if (!query.exec(R"sql(
        CREATE INDEX IF NOT EXISTS idx_translation_history_created_at
        ON translation_history(created_at DESC)
    )sql"))
        return false;

    return true;
}
