#pragma once

#include <QObject>
#include <QString>

struct TranslateRequest final
{
    QString text;
    QString sourceLang;
    QString targetLang;
};

struct TranslateResult final
{
    QString translatedText;
    QString detectedLang;
    QString provider;
};

class TranslateProvider : public QObject
{
    Q_OBJECT

public:
    explicit TranslateProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~TranslateProvider() override = default;

    virtual QString name() const = 0;
    virtual void translate(const QString &requestId, const TranslateRequest &request) = 0;

signals:
    void finished(const QString &requestId, const TranslateResult &result);
    void failed(const QString &requestId, const QString &error);
};

