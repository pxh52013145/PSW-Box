#pragma once

#include "translateprovider.h"

#include <QObject>

class TranslateService final : public QObject
{
    Q_OBJECT

public:
    explicit TranslateService(QObject *parent = nullptr);
    ~TranslateService() override;

    QStringList availableProviders() const;
    void setCurrentProvider(const QString &providerName);
    QString currentProvider() const;

    QString translate(const TranslateRequest &request);

signals:
    void finished(const QString &requestId, const TranslateResult &result);
    void failed(const QString &requestId, const QString &error);

private:
    TranslateProvider *provider_ = nullptr;
    TranslateProvider *mock_ = nullptr;
    TranslateProvider *mymemory_ = nullptr;
};

