#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QDebug>

/**
 * @brief Centralized logging utility to replace scattered qDebug() calls.
 *
 * Provides consistent logging levels and conditional compilation support
 * for removing debug logs in release builds.
 */
class Logger
{
public:
    /**
     * @brief Logging levels from most verbose to least.
     */
    enum Level {
        Debug,      ///< Detailed debugging information
        Info,       ///< General information messages
        Warning,    ///< Warning messages for potential issues
        Error       ///< Error messages for failures
    };

    /**
     * @brief Log a message at the specified level.
     * @param level The logging level for this message.
     * @param message The message to log.
     */
    static void log(Level level, const QString& message);

    /**
     * @brief Set the minimum logging level.
     *
     * Messages below this level will be filtered out.
     * @param level The minimum level to log.
     */
    static void setMinLevel(Level level);

    /**
     * @brief Get the current minimum logging level.
     * @return The current minimum logging level.
     */
    static Level getMinLevel();

private:
    static Level s_minLevel;
};

// Convenience macros for easy usage
#define LOG_DEBUG(msg) Logger::log(Logger::Debug, msg)
#define LOG_INFO(msg) Logger::log(Logger::Info, msg)
#define LOG_WARNING(msg) Logger::log(Logger::Warning, msg)
#define LOG_ERROR(msg) Logger::log(Logger::Error, msg)

#endif // LOGGER_H
