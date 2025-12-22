#pragma once

#include "translateprovider.h"

class MockTranslateProvider final : public TranslateProvider
{
    Q_OBJECT

public:
    explicit MockTranslateProvider(QObject *parent = nullptr);
    QString name() const override;
    void translate(const QString &requestId, const TranslateRequest &request) override;
};

