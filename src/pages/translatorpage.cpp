#include "translatorpage.h"

#include "translate/translateservice.h"
#include "translate/translationhistorymodel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

TranslatorPage::TranslatorPage(QWidget *parent) : QWidget(parent)
{
    service_ = new TranslateService(this);
    historyModel_ = new TranslationHistoryModel(this);

    setupUi();
    wireSignals();
}

void TranslatorPage::setupUi()
{
    auto *root = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();
    providerCombo_ = new QComboBox(this);
    providerCombo_->addItems(service_->availableProviders());

    sourceLangCombo_ = new QComboBox(this);
    sourceLangCombo_->addItems({"auto", "zh-CN", "en"});
    targetLangCombo_ = new QComboBox(this);
    targetLangCombo_->addItems({"zh-CN", "en"});

    translateBtn_ = new QPushButton("翻译", this);
    clearBtn_ = new QPushButton("清空", this);
    copyBtn_ = new QPushButton("复制译文", this);

    topRow->addWidget(new QLabel("引擎：", this));
    topRow->addWidget(providerCombo_);
    topRow->addSpacing(12);
    topRow->addWidget(new QLabel("源：", this));
    topRow->addWidget(sourceLangCombo_);
    topRow->addWidget(new QLabel("目标：", this));
    topRow->addWidget(targetLangCombo_);
    topRow->addStretch(1);
    topRow->addWidget(translateBtn_);
    topRow->addWidget(clearBtn_);
    topRow->addWidget(copyBtn_);

    root->addLayout(topRow);

    auto *editRow = new QHBoxLayout();
    sourceEdit_ = new QPlainTextEdit(this);
    sourceEdit_->setPlaceholderText("输入要翻译的文本…");

    targetEdit_ = new QPlainTextEdit(this);
    targetEdit_->setReadOnly(true);
    targetEdit_->setPlaceholderText("译文输出…");

    editRow->addWidget(sourceEdit_, 1);
    editRow->addWidget(targetEdit_, 1);
    root->addLayout(editRow, 2);

    statusLabel_ = new QLabel(this);
    statusLabel_->setText("就绪");
    root->addWidget(statusLabel_);

    root->addWidget(new QLabel("历史记录（最近 200 条）：", this));
    historyView_ = new QTableView(this);
    historyView_->setModel(historyModel_);
    historyView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyView_->setSelectionMode(QAbstractItemView::SingleSelection);
    historyView_->horizontalHeader()->setStretchLastSection(true);
    historyView_->verticalHeader()->setVisible(false);
    historyView_->setWordWrap(false);
    root->addWidget(historyView_, 1);
}

void TranslatorPage::wireSignals()
{
    connect(translateBtn_, &QPushButton::clicked, this, &TranslatorPage::startTranslate);
    connect(clearBtn_, &QPushButton::clicked, this, [this]() {
        sourceEdit_->clear();
        targetEdit_->clear();
        statusLabel_->setText("已清空");
    });
    connect(copyBtn_, &QPushButton::clicked, this, [this]() {
        const auto text = targetEdit_->toPlainText();
        if (text.trimmed().isEmpty())
            return;
        QApplication::clipboard()->setText(text);
        statusLabel_->setText("译文已复制到剪贴板");
    });

    connect(providerCombo_, &QComboBox::currentTextChanged, this, [this](const QString &provider) {
        service_->setCurrentProvider(provider);
    });
    service_->setCurrentProvider(providerCombo_->currentText());

    connect(service_, &TranslateService::finished, this, [this](const QString &requestId, const TranslateResult &result) {
        if (requestId != pendingRequestId_)
            return;

        targetEdit_->setPlainText(result.translatedText);
        statusLabel_->setText(QString("完成（%1）").arg(result.provider));
        setBusy(false);

        TranslationHistoryItem item;
        item.sourceText = sourceEdit_->toPlainText();
        item.translatedText = result.translatedText;
        item.sourceLang = sourceLangCombo_->currentText();
        item.targetLang = targetLangCombo_->currentText();
        item.provider = result.provider;
        item.createdAt = QDateTime::currentDateTime();
        historyModel_->addEntry(item);
    });

    connect(service_, &TranslateService::failed, this, [this](const QString &requestId, const QString &error) {
        if (requestId != pendingRequestId_)
            return;
        statusLabel_->setText(QString("失败：%1").arg(error));
        setBusy(false);
    });

    connect(historyView_, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto item = historyModel_->itemAt(index.row());
        if (item.sourceText.isEmpty())
            return;
        sourceEdit_->setPlainText(item.sourceText);
        targetEdit_->setPlainText(item.translatedText);
        statusLabel_->setText("已从历史记录载入");
    });
}

void TranslatorPage::startTranslate()
{
    const auto text = sourceEdit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        statusLabel_->setText("请输入要翻译的文本");
        return;
    }

    TranslateRequest request;
    request.text = text;
    request.sourceLang = sourceLangCombo_->currentText();
    request.targetLang = targetLangCombo_->currentText();

    targetEdit_->clear();
    statusLabel_->setText("翻译中…");
    setBusy(true);
    pendingRequestId_ = service_->translate(request);
}

void TranslatorPage::setBusy(bool busy)
{
    translateBtn_->setEnabled(!busy);
    providerCombo_->setEnabled(!busy);
    sourceLangCombo_->setEnabled(!busy);
    targetLangCombo_->setEnabled(!busy);
}
