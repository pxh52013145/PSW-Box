#pragma once

#include "passwordentry.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class PasswordVault;

class PasswordRepository final
{
public:
    explicit PasswordRepository(PasswordVault *vault);

    QString lastError() const;

    QVector<PasswordEntry> listEntries() const;
    QStringList listCategories() const;

    bool addEntry(const PasswordEntrySecrets &secrets);
    bool addEntryWithTimestamps(const PasswordEntrySecrets &secrets, qint64 createdAtSecs, qint64 updatedAtSecs);
    bool updateEntry(const PasswordEntrySecrets &secrets);
    bool deleteEntry(qint64 id);

    std::optional<PasswordEntrySecrets> loadEntry(qint64 id) const;

private:
    void setError(const QString &error) const;

    PasswordVault *vault_ = nullptr;
    mutable QString lastError_;
};
