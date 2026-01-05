#pragma once

#include <QHash>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>

class PasswordFaviconService final : public QObject
{
    Q_OBJECT

public:
    explicit PasswordFaviconService(QObject *parent = nullptr);

    void setNetworkEnabled(bool enabled);
    bool isNetworkEnabled() const { return networkEnabled_; }

    QIcon iconForUrl(const QString &url);

signals:
    void iconUpdated(const QString &host);

private:
    struct CacheEntry final
    {
        QIcon icon;
        qint64 fetchedAtSecs = 0;
        bool hasIcon = false;
    };

    QString hostForUrl(const QString &url, QString *schemeOut = nullptr) const;
    bool loadFromDatabase(const QString &host, CacheEntry &out);
    void saveToDatabase(const QString &host, const QByteArray &bytes, const QString &contentType, qint64 fetchedAtSecs);

    void ensureFetch(const QString &host, const QString &scheme);

    QNetworkAccessManager net_;
    bool networkEnabled_ = true;
    QHash<QString, CacheEntry> memory_;
    QSet<QString> pending_;
};

