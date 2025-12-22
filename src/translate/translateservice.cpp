#include "translateservice.h"

#include "mocktranslateprovider.h"
#include "mymemorytranslateprovider.h"

#include <QUuid>

TranslateService::TranslateService(QObject *parent) : QObject(parent)
{
    mock_ = new MockTranslateProvider(this);
    mymemory_ = new MyMemoryTranslateProvider(this);
    provider_ = mock_;

    for (auto *p : {mock_, mymemory_}) {
        connect(p, &TranslateProvider::finished, this, &TranslateService::finished);
        connect(p, &TranslateProvider::failed, this, &TranslateService::failed);
    }
}

TranslateService::~TranslateService() = default;

QStringList TranslateService::availableProviders() const
{
    return {mock_->name(), mymemory_->name()};
}

void TranslateService::setCurrentProvider(const QString &providerName)
{
    if (providerName == mymemory_->name()) {
        provider_ = mymemory_;
        return;
    }
    provider_ = mock_;
}

QString TranslateService::currentProvider() const
{
    return provider_ ? provider_->name() : QString();
}

QString TranslateService::translate(const TranslateRequest &request)
{
    const auto requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    provider_->translate(requestId, request);
    return requestId;
}

