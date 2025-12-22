#pragma once

#include "translateprovider.h"

class QNetworkAccessManager;

class MyMemoryTranslateProvider final : public TranslateProvider
{
    Q_OBJECT

public:
    explicit MyMemoryTranslateProvider(QObject *parent = nullptr);
    ~MyMemoryTranslateProvider() override;

    QString name() const override;
    void translate(const QString &requestId, const TranslateRequest &request) override;

private:
    QNetworkAccessManager *network_ = nullptr;
};

