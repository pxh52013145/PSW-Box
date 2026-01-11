#pragma once

#include <QString>

namespace PasswordUrl {

QString normalizeHost(const QString &host);
QString hostFromUrl(const QString &urlText);
bool hostsEqual(const QString &a, const QString &b);
bool urlMatchesHost(const QString &urlText, const QString &host);

} // namespace PasswordUrl

