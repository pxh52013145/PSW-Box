#include "mainwindow.h"

#include "core/database.h"
#include "core/logging.h"
#include "pages/translatorpage.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    ensureInfrastructure();
    setupUi();
    setupTray();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!trayEnabled_) {
        event->accept();
        return;
    }

    hide();
    event->ignore();
}

void MainWindow::ensureInfrastructure()
{
    Logging::init();
    if (!Database::open()) {
        QMessageBox::critical(this, "数据库初始化失败", "无法打开 SQLite 数据库，请检查磁盘权限或路径。");
    }
}

void MainWindow::setupUi()
{
    setWindowTitle("Toolbox");
    resize(980, 720);

    auto *tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    translatorPage_ = new TranslatorPage(tabs);
    tabs->addTab(translatorPage_, "翻译");

    auto *placeholder = new QWidget(tabs);
    tabs->addTab(placeholder, "密码（开发中）");
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        trayEnabled_ = false;
        return;
    }

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setToolTip("Toolbox");
    trayIcon_->setIcon(QApplication::windowIcon());

    auto *menu = new QMenu(this);
    auto *actionShow = menu->addAction("打开主窗口");
    auto *actionQuit = menu->addAction("退出");

    connect(actionShow, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayIcon_->setContextMenu(menu);
    trayIcon_->show();

    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            raise();
            activateWindow();
        }
    });
}
