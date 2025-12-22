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

