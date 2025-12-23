#pragma once

#include <QDateTime>
#include <QString>

struct PasswordEntry final
{
    qint64 id = 0;
    QString title;
    QString username;
    QString url;
    QString category;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct PasswordEntrySecrets final
{
    PasswordEntry entry;
    QString password;
    QString notes;
};

