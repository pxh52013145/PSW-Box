QT += core gui widgets network

CONFIG += c++17 utf8_source

TEMPLATE = app
TARGET = ToolboxLauncher

INCLUDEPATH += $$PWD/../../src
DEPENDPATH += $$PWD/../../src

DESTDIR = $$OUT_PWD/../bin

SOURCES += \
    main.cpp \
    launcherwindow.cpp \
    ../../src/core/apppaths.cpp \
    ../../src/core/logging.cpp \
    ../../src/core/singleinstance.cpp

HEADERS += \
    launcherwindow.h \
    ../../src/core/apppaths.h \
    ../../src/core/logging.h \
    ../../src/core/singleinstance.h
