#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableView;

class TranslateService;
class TranslationHistoryModel;

class TranslatorPage final : public QWidget
{
    Q_OBJECT

public:
    explicit TranslatorPage(QWidget *parent = nullptr);

private:
    void setupUi();
    void wireSignals();
    void startTranslate();
    void setBusy(bool busy);

    QComboBox *providerCombo_ = nullptr;
    QComboBox *sourceLangCombo_ = nullptr;
    QComboBox *targetLangCombo_ = nullptr;
    QPlainTextEdit *sourceEdit_ = nullptr;
    QPlainTextEdit *targetEdit_ = nullptr;
    QPushButton *translateBtn_ = nullptr;
    QPushButton *clearBtn_ = nullptr;
    QPushButton *copyBtn_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QTableView *historyView_ = nullptr;

    TranslateService *service_ = nullptr;
    TranslationHistoryModel *historyModel_ = nullptr;
    QString pendingRequestId_;
};

