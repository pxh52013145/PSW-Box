#include "passwordmainwindow.h"

#include "core/singleinstance.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>

static void applyPasswordTheme(QApplication &app)
{
    QApplication::setStyle("Fusion");

    QFile qssFile(":/tbx/themes/password_dark.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const auto qss = QString::fromUtf8(qssFile.readAll());
    app.setStyleSheet(qss);
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("CourseDesign");
    QCoreApplication::setApplicationName("ToolboxPassword");

    QApplication app(argc, argv);
    applyPasswordTheme(app);

    SingleInstance instance("CourseDesign.Toolbox.Password");
    if (!instance.tryRunAsPrimary()) {
        if (!instance.lastError().isEmpty())
            QMessageBox::critical(nullptr, "启动失败", instance.lastError());
        return 0;
    }

    PasswordMainWindow window;
    QObject::connect(&instance, &SingleInstance::messageReceived, &window, [&window](const QByteArray &) {
        window.activateFromMessage();
    });

    if (!window.isReady())
        return 1;

    window.show();
    return app.exec();
}
