#pragma once

#include <QString>

class AppPaths final
{
public:
    static QString appDataDir();
    static QString databaseFilePath();
    static QString logFilePath();
};

