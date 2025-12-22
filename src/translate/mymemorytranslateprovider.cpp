#include "mymemorytranslateprovider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

MyMemoryTranslateProvider::MyMemoryTranslateProvider(QObject *parent) : TranslateProvider(parent)
{
    network_ = new QNetworkAccessManager(this);
}

MyMemoryTranslateProvider::~MyMemoryTranslateProvider() = default;

QString MyMemoryTranslateProvider::name() const
{
    return "MyMemory";
}

void MyMemoryTranslateProvider::translate(const QString &requestId, const TranslateRequest &request)
{
    auto url = QUrl("https://api.mymemory.translated.net/get");
    QUrlQuery query;
    query.addQueryItem("q", request.text);

    const auto source = request.sourceLang.isEmpty() ? "auto" : request.sourceLang;
    const auto target = request.targetLang.isEmpty() ? "zh-CN" : request.targetLang;
    query.addQueryItem("langpair", source + "|" + target);
    url.setQuery(query);

    QNetworkRequest netRequest(url);
    netRequest.setHeader(QNetworkRequest::UserAgentHeader, "Toolbox/1.0");

    auto *reply = network_->get(netRequest);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit failed(requestId, reply->errorString());
            return;
        }

        const auto bytes = reply->readAll();
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isObject()) {
            emit failed(requestId, "解析翻译响应失败（非 JSON 对象）");
            return;
        }

        const auto root = doc.object();
        const auto responseData = root.value("responseData").toObject();
        const auto translated = responseData.value("translatedText").toString();
        if (translated.trimmed().isEmpty()) {
            emit failed(requestId, "翻译结果为空");
            return;
        }

        TranslateResult result;
        result.provider = name();
        result.translatedText = translated;
        result.detectedLang = root.value("matches").isArray() ? "auto" : "auto";
        emit finished(requestId, result);
    });
}

