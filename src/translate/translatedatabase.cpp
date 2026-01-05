#include "translatedatabase.h"

#include "core/apppaths.h"

#include <QDir>
#include <QSqlQuery>

static constexpr auto kConnectionName = "toolbox_translate_sqlite";

bool TranslateDatabase::open()
{
    QSqlDatabase database;
    if (QSqlDatabase::contains(kConnectionName)) {
        database = QSqlDatabase::database(kConnectionName);
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("translate.sqlite3");
        database.setDatabaseName(dbPath);
    }

    if (!database.isOpen() && !database.open())
        return false;

    return ensureSchema(database);
}

QSqlDatabase TranslateDatabase::db()
{
    return QSqlDatabase::database(kConnectionName);
}

bool TranslateDatabase::ensureSchema(QSqlDatabase &database)
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

