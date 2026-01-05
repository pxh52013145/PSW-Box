#include "passwordstrength.h"

#include <QSet>

namespace {

bool isAllSameChar(const QString &s)
{
    if (s.size() < 4)
        return false;

    const auto ch = s.at(0);
    for (int i = 1; i < s.size(); ++i) {
        if (s.at(i) != ch)
            return false;
    }
    return true;
}

bool isSequentialChars(const QString &s)
{
    if (s.size() < 4)
        return false;

    bool inc = true;
    bool dec = true;
    for (int i = 1; i < s.size(); ++i) {
        const auto prev = s.at(i - 1).unicode();
        const auto cur = s.at(i).unicode();
        if (cur != prev + 1)
            inc = false;
        if (cur != prev - 1)
            dec = false;
        if (!inc && !dec)
            return false;
    }
    return inc || dec;
}

bool isCommonWeakPassword(const QString &password)
{
    static const QSet<QString> kCommonWeak = {
        "123456",
        "12345678",
        "123456789",
        "111111",
        "000000",
        "password",
        "qwerty",
        "abc123",
        "admin",
        "letmein",
        "iloveyou",
    };

    const auto p = password.trimmed().toLower();
    if (p.isEmpty())
        return true;

    if (kCommonWeak.contains(p))
        return true;

    bool allDigits = true;
    for (const auto &ch : p) {
        if (!ch.isDigit()) {
            allDigits = false;
            break;
        }
    }
    if (allDigits && p.size() <= 10)
        return true;

    return false;
}

QString strengthLabelForScore(int score)
{
    if (score < 20)
        return "极弱";
    if (score < 40)
        return "弱";
    if (score < 60)
        return "一般";
    if (score < 80)
        return "强";
    return "很强";
}

} // namespace

PasswordStrength evaluatePasswordStrength(const QString &password)
{
    if (password.isEmpty())
        return {0, "极弱"};

    if (isCommonWeakPassword(password))
        return {0, "极弱"};

    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSymbol = false;
    int uniqueChars = 0;

    {
        QSet<QChar> uniq;
        for (const auto &ch : password) {
            if (ch.isLower())
                hasLower = true;
            else if (ch.isUpper())
                hasUpper = true;
            else if (ch.isDigit())
                hasDigit = true;
            else
                hasSymbol = true;
            uniq.insert(ch);
        }
        uniqueChars = uniq.size();
    }

    const int length = password.size();
    const int classes = (hasLower ? 1 : 0) + (hasUpper ? 1 : 0) + (hasDigit ? 1 : 0) + (hasSymbol ? 1 : 0);

    int score = 0;

    score += qMin(length * 4, 40);
    score += classes * 10;
    if (classes >= 3)
        score += 10;

    if (length >= 16)
        score += 15;
    else if (length >= 12)
        score += 10;
    else if (length >= 8)
        score += 5;

    if (uniqueChars <= qMax(2, length / 4))
        score -= 15;

    if (isAllSameChar(password))
        score -= 30;
    if (isSequentialChars(password))
        score -= 20;

    if (length < 8)
        score = qMin(score, 25);

    score = qBound(0, score, 100);
    return {score, strengthLabelForScore(score)};
}

