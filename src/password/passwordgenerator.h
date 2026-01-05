#pragma once

#include <QString>

struct PasswordGeneratorOptions final
{
    int length = 16;
    bool useUpper = true;
    bool useLower = true;
    bool useDigits = true;
    bool useSymbols = true;
    bool excludeAmbiguous = true;
    bool requireEachSelectedType = true;
};

QString generatePassword(const PasswordGeneratorOptions &options, QString *errorOut = nullptr);

