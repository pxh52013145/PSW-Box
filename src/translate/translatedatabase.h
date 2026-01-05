#pragma once

#include <QSqlDatabase>

class TranslateDatabase final
{
public:
    static bool open();
    static QSqlDatabase db();

private:
    static bool ensureSchema(QSqlDatabase &database);
};

