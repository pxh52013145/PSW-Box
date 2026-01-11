#include "passwordwebloginmatcher.h"

#include "passwordurl.h"

namespace PasswordWebLoginMatcher {

MatchResult match(const QVector<PasswordEntry> &entries, const QString &pageUrl, const QString &username)
{
    MatchResult result;

    result.host = PasswordUrl::hostFromUrl(pageUrl);
    if (result.host.isEmpty())
        return result;

    const auto user = username.trimmed();

    for (const auto &e : entries) {
        if (e.type != PasswordEntryType::WebLogin)
            continue;
        if (!PasswordUrl::urlMatchesHost(e.url, result.host))
            continue;

        result.hostEntries.push_back(e);

        const auto entryUser = e.username.trimmed();
        if (!user.isEmpty()) {
            if (entryUser.compare(user, Qt::CaseInsensitive) == 0)
                result.userEntries.push_back(e);
        } else {
            if (entryUser.isEmpty())
                result.userEntries.push_back(e);
        }
    }

    return result;
}

} // namespace PasswordWebLoginMatcher

