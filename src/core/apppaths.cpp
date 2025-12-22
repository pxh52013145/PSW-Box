#include "apppaths.h"

#include <QDir>
#include <QStandardPaths>

QString AppPaths::appDataDir()
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base;
}

QString AppPaths::databaseFilePath()
{
    return QDir(appDataDir()).filePath("toolbox.sqlite3");
}

QString AppPaths::logFilePath()
{
    const auto logsDir = QDir(appDataDir()).filePath("logs");
    QDir().mkpath(logsDir);
    return QDir(logsDir).filePath("toolbox.log");
}

