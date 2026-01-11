QT += core testlib sql network

CONFIG += c++17 console utf8_source

TEMPLATE = app
TARGET = ToolboxPasswordIntegrationTests

INCLUDEPATH += $$PWD/../../src
DEPENDPATH += $$PWD/../../src

SOURCES += \
    tst_password_integration.cpp \
    ../../src/core/apppaths.cpp \
    ../../src/core/crypto.cpp \
    ../../src/password/passworddatabase.cpp \
    ../../src/password/passwordvault.cpp \
    ../../src/password/passwordrepository.cpp \
    ../../src/password/passwordcsv.cpp \
    ../../src/password/passwordcsvimportworker.cpp \
    ../../src/password/passwordstrength.cpp \
    ../../src/password/passwordgenerator.cpp \
    ../../src/password/passwordurl.cpp \
    ../../src/password/passwordgraph.cpp \
    ../../src/password/passwordwebloginmatcher.cpp \
    ../../src/password/passwordfaviconservice.cpp \
    ../../src/password/passwordhealthworker.cpp

HEADERS += \
    ../../src/core/apppaths.h \
    ../../src/core/crypto.h \
    ../../src/password/passworddatabase.h \
    ../../src/password/passwordentry.h \
    ../../src/password/passwordgroup.h \
    ../../src/password/passwordrepository.h \
    ../../src/password/passwordvault.h \
    ../../src/password/passwordcsv.h \
    ../../src/password/passwordcsvimportworker.h \
    ../../src/password/passwordstrength.h \
    ../../src/password/passwordgenerator.h \
    ../../src/password/passwordurl.h \
    ../../src/password/passwordgraph.h \
    ../../src/password/passwordwebloginmatcher.h \
    ../../src/password/passwordfaviconservice.h \
    ../../src/password/passwordhealth.h \
    ../../src/password/passwordhealthworker.h
