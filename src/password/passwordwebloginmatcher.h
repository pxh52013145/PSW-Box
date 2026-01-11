#pragma once

#include "passwordentry.h"

#include <QString>
#include <QVector>

namespace PasswordWebLoginMatcher {

struct MatchResult final
{
    QString host;
    QVector<PasswordEntry> hostEntries;
    QVector<PasswordEntry> userEntries;
};

MatchResult match(const QVector<PasswordEntry> &entries, const QString &pageUrl, const QString &username);

} // namespace PasswordWebLoginMatcher

