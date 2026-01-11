QT += core gui widgets network sql

qtHaveModule(webenginewidgets): qtHaveModule(webchannel): qtHaveModule(positioning) {
    QT += webenginewidgets webchannel
    DEFINES += TBX_HAS_WEBENGINE

    SOURCES += \
        ../../src/pages/passwordwebassistantdialog.cpp

    HEADERS += \
        ../../src/pages/passwordwebassistantdialog.h
} else {
    message("Qt WebEngine/WebChannel/Positioning not found: Web Assistant disabled")
}

CONFIG += c++17 utf8_source

TEMPLATE = app
TARGET = ToolboxPassword

INCLUDEPATH += $$PWD/../../src
DEPENDPATH += $$PWD/../../src

DESTDIR = $$OUT_PWD/../bin

SOURCES += \
    main.cpp \
    passwordmainwindow.cpp \
    ../../src/core/apppaths.cpp \
    ../../src/core/logging.cpp \
    ../../src/core/crypto.cpp \
    ../../src/core/singleinstance.cpp \
    ../../src/password/passworddatabase.cpp \
    ../../src/password/passwordvault.cpp \
    ../../src/password/passwordrepository.cpp \
    ../../src/password/passwordentrymodel.cpp \
    ../../src/password/passwordcsv.cpp \
    ../../src/password/passwordcsvimportworker.cpp \
    ../../src/password/passwordgenerator.cpp \
    ../../src/password/passwordstrength.cpp \
    ../../src/password/passwordurl.cpp \
    ../../src/password/passwordgraph.cpp \
    ../../src/password/passwordwebloginmatcher.cpp \
    ../../src/password/passwordfaviconservice.cpp \
    ../../src/password/passwordhealthworker.cpp \
    ../../src/password/passwordhealthmodel.cpp \
    ../../src/password/passwordgroupmodel.cpp \
    ../../src/pages/passwordmanagerpage.cpp \
    ../../src/pages/passwordcsvimportdialog.cpp \
    ../../src/pages/passwordentrydialog.cpp \
    ../../src/pages/passwordgraphdialog.cpp \
    ../../src/pages/passwordgeneratordialog.cpp \
    ../../src/pages/passwordhealthdialog.cpp

HEADERS += \
    passwordmainwindow.h \
    ../../src/core/apppaths.h \
    ../../src/core/logging.h \
    ../../src/core/crypto.h \
    ../../src/core/singleinstance.h \
    ../../src/password/passworddatabase.h \
    ../../src/password/passwordvault.h \
    ../../src/password/passwordentry.h \
    ../../src/password/passwordcsv.h \
    ../../src/password/passwordcsvimportworker.h \
    ../../src/password/passwordgenerator.h \
    ../../src/password/passwordstrength.h \
    ../../src/password/passwordurl.h \
    ../../src/password/passwordgraph.h \
    ../../src/password/passwordwebloginmatcher.h \
    ../../src/password/passwordfaviconservice.h \
    ../../src/password/passwordhealth.h \
    ../../src/password/passwordhealthworker.h \
    ../../src/password/passwordhealthmodel.h \
    ../../src/password/passwordgroup.h \
    ../../src/password/passwordrepository.h \
    ../../src/password/passwordentrymodel.h \
    ../../src/password/passwordgroupmodel.h \
    ../../src/pages/passwordmanagerpage.h \
    ../../src/pages/passwordcsvimportdialog.h \
    ../../src/pages/passwordentrydialog.h \
    ../../src/pages/passwordgraphdialog.h \
    ../../src/pages/passwordgeneratordialog.h \
    ../../src/pages/passwordhealthdialog.h
