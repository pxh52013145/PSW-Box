#include "mocktranslateprovider.h"

#include <QTimer>

MockTranslateProvider::MockTranslateProvider(QObject *parent) : TranslateProvider(parent) {}

QString MockTranslateProvider::name() const
{
    return "Mock";
}

void MockTranslateProvider::translate(const QString &requestId, const TranslateRequest &request)
{
    QTimer::singleShot(150, this, [this, requestId, request]() {
        TranslateResult result;
        result.provider = name();
        result.detectedLang = request.sourceLang.isEmpty() ? "auto" : request.sourceLang;
        result.translatedText = QString("【Mock】%1").arg(request.text);
        emit finished(requestId, result);
    });
}

