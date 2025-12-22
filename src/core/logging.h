#pragma once

#include <QtGlobal>

class QMessageLogContext;
class QString;

class Logging final
{
public:
    static void init();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
};
