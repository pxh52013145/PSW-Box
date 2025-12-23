#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSystemTrayIcon;

class TranslatorPage;
class PasswordManagerPage;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupTray();
    void ensureInfrastructure();

    QSystemTrayIcon *trayIcon_ = nullptr;
    TranslatorPage *translatorPage_ = nullptr;
    PasswordManagerPage *passwordManagerPage_ = nullptr;
    bool trayEnabled_ = true;
};

#endif
