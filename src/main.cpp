#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("CourseDesign");
    QCoreApplication::setApplicationName("Toolbox");

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
