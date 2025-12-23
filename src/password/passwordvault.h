#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <optional>

class PasswordVault final : public QObject
{
    Q_OBJECT

public:
    explicit PasswordVault(QObject *parent = nullptr);

    bool isInitialized() const;
    bool isUnlocked() const;
    QString lastError() const;

    bool reloadMetadata();
    bool createVault(const QString &masterPassword);
    bool unlock(const QString &masterPassword);
    void lock();
    bool changeMasterPassword(const QString &newMasterPassword);

    QByteArray masterKey() const;

signals:
    void stateChanged();

private:
    struct Meta final
    {
        QByteArray salt;
        int iterations = 0;
        QByteArray verifier;
    };

    static Meta defaultMeta();
    static QByteArray computeVerifier(const QByteArray &masterKey);

    void setError(const QString &error);
    std::optional<Meta> readMeta();
    bool writeMeta(const Meta &meta);

    std::optional<Meta> meta_;
    QByteArray masterKey_;
    QString lastError_;
};
