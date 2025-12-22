#include "logging.h"

#include "apppaths.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QStringConverter>
#include <QTextStream>

static QMutex g_logMutex;
static bool g_initialized = false;

void Logging::init()
{
    if (g_initialized)
        return;
    g_initialized = true;
    qInstallMessageHandler(&Logging::messageHandler);
}

static QString levelToString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    default:
        return "LOG";
    }
}

void Logging::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&g_logMutex);

    QFile file(AppPaths::logFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const auto ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    out << ts << " [" << levelToString(type) << "] " << msg;
    if (context.file && context.line > 0)
        out << " (" << context.file << ":" << context.line << ")";
    out << "\n";

    if (type == QtFatalMsg)
        abort();
}
