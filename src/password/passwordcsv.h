#pragma once

#include "passwordentry.h"

#include <QtGlobal>
#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

enum class PasswordCsvFormat : int
{
    Unknown = 0,
    Toolbox = 1,
    Chrome = 2,
    KeePassXC = 3,
};

struct PasswordCsvInfo final
{
    PasswordCsvFormat format = PasswordCsvFormat::Unknown;
    QChar delimiter = ',';
    QStringList header;
    bool hasCategoryLikeColumn = false;
};

struct PasswordCsvParseResult final
{
    QVector<PasswordEntrySecrets> entries;
    int totalRows = 0;
    int skippedInvalid = 0;
    int skippedEmpty = 0;
    QStringList warnings;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

PasswordCsvInfo detectPasswordCsv(const QByteArray &csvData, QString *errorOut = nullptr);

PasswordCsvParseResult parsePasswordCsv(const QByteArray &csvData, qint64 defaultGroupId = 1);

QByteArray exportPasswordCsv(const QVector<PasswordEntrySecrets> &entries);
