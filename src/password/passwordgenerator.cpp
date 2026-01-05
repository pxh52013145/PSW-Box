#include "passwordgenerator.h"

#include <QStringList>
#include <QRandomGenerator>

#include <algorithm>

namespace {

bool isAmbiguous(QChar ch)
{
    static const QString kAmbiguous = "O0oIl1";
    return kAmbiguous.contains(ch);
}

QString filterAmbiguous(const QString &chars, bool exclude)
{
    if (!exclude)
        return chars;

    QString out;
    out.reserve(chars.size());
    for (const auto &ch : chars) {
        if (!isAmbiguous(ch))
            out.append(ch);
    }
    return out;
}

QChar randomChar(const QString &chars, QRandomGenerator *rng)
{
    const auto idx = rng->bounded(chars.size());
    return chars.at(idx);
}

void shuffle(QString &s, QRandomGenerator *rng)
{
    for (int i = s.size() - 1; i > 0; --i) {
        const auto j = rng->bounded(i + 1);
        if (i != j) {
            const auto tmp = s.at(i);
            s[i] = s.at(j);
            s[j] = tmp;
        }
    }
}

} // namespace

QString generatePassword(const PasswordGeneratorOptions &options, QString *errorOut)
{
    const auto fail = [&](const QString &msg) {
        if (errorOut)
            *errorOut = msg;
        return QString();
    };

    const int length = options.length;
    if (length <= 0)
        return fail("长度无效");

    const auto upper = filterAmbiguous("ABCDEFGHIJKLMNOPQRSTUVWXYZ", options.excludeAmbiguous);
    const auto lower = filterAmbiguous("abcdefghijklmnopqrstuvwxyz", options.excludeAmbiguous);
    const auto digits = filterAmbiguous("0123456789", options.excludeAmbiguous);
    const auto symbols = filterAmbiguous("!@#$%^&*()-_=+[]{};:,.?/\\|~", options.excludeAmbiguous);

    QStringList pools;
    if (options.useUpper)
        pools.push_back(upper);
    if (options.useLower)
        pools.push_back(lower);
    if (options.useDigits)
        pools.push_back(digits);
    if (options.useSymbols)
        pools.push_back(symbols);

    pools.erase(std::remove_if(pools.begin(),
                               pools.end(),
                               [](const QString &pool) { return pool.isEmpty(); }),
                pools.end());

    if (pools.isEmpty())
        return fail("请至少选择一种字符类型");

    if (options.requireEachSelectedType && length < pools.size())
        return fail("长度必须 ≥ 已选择的字符类型数量");

    QString all;
    for (const auto &pool : pools)
        all.append(pool);

    auto *rng = QRandomGenerator::system();
    QString out;
    out.reserve(length);

    if (options.requireEachSelectedType) {
        for (const auto &pool : pools)
            out.append(randomChar(pool, rng));
    }

    while (out.size() < length)
        out.append(randomChar(all, rng));

    shuffle(out, rng);
    return out;
}
