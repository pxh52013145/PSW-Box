#include "passwordurl.h"

#include <QUrl>

namespace PasswordUrl {

QString normalizeHost(const QString &host)
{
    auto out = host.trimmed().toLower();
    if (out.startsWith("www."))
        out = out.mid(4);
    return out;
}

QString hostFromUrl(const QString &urlText)
{
    const auto trimmed = urlText.trimmed();
    if (trimmed.isEmpty())
        return {};

    QUrl u(trimmed);
    if (u.scheme().isEmpty() && trimmed.contains('.') && !trimmed.contains("://"))
        u = QUrl(QString("https://%1").arg(trimmed));

    const auto host = u.host().trimmed();
    if (host.isEmpty())
        return {};

    return normalizeHost(host);
}

bool hostsEqual(const QString &a, const QString &b)
{
    return normalizeHost(a) == normalizeHost(b);
}

bool urlMatchesHost(const QString &urlText, const QString &host)
{
    const auto h = hostFromUrl(urlText);
    if (h.isEmpty())
        return false;
    return hostsEqual(h, host);
}

} // namespace PasswordUrl

