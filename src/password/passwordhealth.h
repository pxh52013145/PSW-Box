#pragma once

#include <QtGlobal>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct PasswordHealthItem final
{
    qint64 entryId = 0;
    qint64 groupId = 1;
    QString title;
    QString username;
    QString url;
    QString category;
    QStringList tags;
    qint64 updatedAtSecs = 0;

    int strengthScore = 0;
    bool weak = false;

    bool reused = false;
    int reuseCount = 0;

    bool stale = false;
    int daysSinceUpdate = 0;

    bool corrupted = false;

    bool pwned = false;
    bool pwnedChecked = false;
    qint64 pwnedCount = 0;

    bool hasIssues() const { return weak || reused || stale || corrupted || pwned; }

    QString issuesText() const
    {
        QStringList issues;
        if (corrupted)
            issues.push_back("损坏");
        if (weak)
            issues.push_back(QString("弱密码(%1)").arg(strengthScore));
        if (reused)
            issues.push_back(QString("重复(%1)").arg(reuseCount));
        if (stale)
            issues.push_back(QString("久未更新(%1天)").arg(daysSinceUpdate));
        if (pwned)
            issues.push_back(pwnedCount > 0 ? QString("已泄露(%1)").arg(pwnedCount) : QString("已泄露"));
        return issues.join("；");
    }
};

Q_DECLARE_METATYPE(PasswordHealthItem)
Q_DECLARE_METATYPE(QVector<PasswordHealthItem>)
