#pragma once

#include "passwordentry.h"

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

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

PasswordCsvParseResult parsePasswordCsv(const QByteArray &csvData, qint64 defaultGroupId = 1);

QByteArray exportPasswordCsv(const QVector<PasswordEntrySecrets> &entries);

