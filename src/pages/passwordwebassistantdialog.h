#pragma once

#include <QDialog>

class PasswordRepository;
class PasswordVault;

class QLineEdit;
class QPushButton;

#ifdef TBX_HAS_WEBENGINE
class QWebChannel;
class QWebEngineView;

class PasswordWebAssistantDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordWebAssistantDialog(PasswordRepository *repo, PasswordVault *vault, QWidget *parent = nullptr);
    ~PasswordWebAssistantDialog() override;

private:
    void setupUi();
    void setupWebChannel();
    void injectScripts();
    void navigateToUrlText(const QString &urlText);
    void loadDemoPage();

    PasswordRepository *repo_ = nullptr;
    PasswordVault *vault_ = nullptr;

    QLineEdit *urlEdit_ = nullptr;
    QPushButton *goBtn_ = nullptr;
    QPushButton *demoBtn_ = nullptr;
    QWebEngineView *view_ = nullptr;

    QWebChannel *channel_ = nullptr;
};
#endif
