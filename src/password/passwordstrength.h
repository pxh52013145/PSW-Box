#pragma once

#include <QString>

struct PasswordStrength final
{
    int score = 0; // 0~100
    QString label;
};

PasswordStrength evaluatePasswordStrength(const QString &password);

