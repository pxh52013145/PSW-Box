QT       += core gui widgets network sql

CONFIG += c++17

INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/pages/translatorpage.cpp \
    src/translate/mocktranslateprovider.cpp \
    src/translate/mymemorytranslateprovider.cpp \
    src/translate/translateservice.cpp \
    src/translate/translationhistorymodel.cpp \
    src/core/apppaths.cpp \
    src/core/database.cpp \
    src/core/logging.cpp \
    src/core/crypto.cpp \
    src/password/passwordvault.cpp \
    src/password/passwordrepository.cpp \
    src/password/passwordentrymodel.cpp

HEADERS += \
    src/mainwindow.h \
    src/pages/translatorpage.h \
    src/translate/translateprovider.h \
    src/translate/mocktranslateprovider.h \
    src/translate/mymemorytranslateprovider.h \
    src/translate/translateservice.h \
    src/translate/translationhistorymodel.h \
    src/core/apppaths.h \
    src/core/database.h \
    src/core/logging.h \
    src/core/crypto.h \
    src/password/passwordvault.h \
    src/password/passwordentry.h \
    src/password/passwordrepository.h \
    src/password/passwordentrymodel.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
