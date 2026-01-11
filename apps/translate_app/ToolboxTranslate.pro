QT += core gui widgets network sql

CONFIG += c++17 utf8_source

TEMPLATE = app
TARGET = ToolboxTranslate

INCLUDEPATH += $$PWD/../../src
DEPENDPATH += $$PWD/../../src

DESTDIR = $$OUT_PWD/../bin

SOURCES += \
    main.cpp \
    translatemainwindow.cpp \
    ../../src/core/apppaths.cpp \
    ../../src/core/logging.cpp \
    ../../src/core/singleinstance.cpp \
    ../../src/translate/translatedatabase.cpp \
    ../../src/translate/mocktranslateprovider.cpp \
    ../../src/translate/mymemorytranslateprovider.cpp \
    ../../src/translate/translateservice.cpp \
    ../../src/translate/translationhistorymodel.cpp \
    ../../src/pages/translatorpage.cpp

HEADERS += \
    translatemainwindow.h \
    ../../src/core/apppaths.h \
    ../../src/core/logging.h \
    ../../src/core/singleinstance.h \
    ../../src/translate/translatedatabase.h \
    ../../src/translate/translateprovider.h \
    ../../src/translate/mocktranslateprovider.h \
    ../../src/translate/mymemorytranslateprovider.h \
    ../../src/translate/translateservice.h \
    ../../src/translate/translationhistorymodel.h \
    ../../src/pages/translatorpage.h
