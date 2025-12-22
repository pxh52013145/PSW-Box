#pragma once

#include <QSqlDatabase>

class Database final
{
public:
    static bool open();
    static QSqlDatabase db();

private:
    static bool ensureSchema(QSqlDatabase &database);
};

