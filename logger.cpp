#include "logger.h"
#include <QDateTime>

// Static member initialization
Logger::Level Logger::s_minLevel = Logger::Debug;

void Logger::log(Level level, const QString& message)
{
    // Filter messages below minimum level
    if (level < s_minLevel) {
        return;
    }

    // In release builds, skip debug messages entirely
#ifdef QT_NO_DEBUG
    if (level == Debug) {
        return;
    }
#endif

    // Format log message with timestamp and level
    QString levelStr;
    switch (level) {
        case Debug: levelStr = "DEBUG"; break;
        case Info: levelStr = "INFO"; break;
        case Warning: levelStr = "WARNING"; break;
        case Error: levelStr = "ERROR"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString formattedMessage = QString("[%1] %2: %3")
                               .arg(timestamp, levelStr, message);

    // Output based on level
    switch (level) {
        case Debug:
        case Info:
            qDebug().noquote() << formattedMessage;
            break;
        case Warning:
            qWarning().noquote() << formattedMessage;
            break;
        case Error:
            qCritical().noquote() << formattedMessage;
            break;
    }
}

void Logger::setMinLevel(Level level)
{
    s_minLevel = level;
    LOG_INFO(QString("Logger minimum level set to: %1")
             .arg(level == Debug ? "Debug" :
                  level == Info ? "Info" :
                  level == Warning ? "Warning" : "Error"));
}

Logger::Level Logger::getMinLevel()
{
    return s_minLevel;
}
