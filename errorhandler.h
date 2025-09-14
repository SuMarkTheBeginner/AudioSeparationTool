#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QString>
#include <QWidget>

/**
 * @brief Utility class for centralized error handling and user notifications.
 */
class ErrorHandler
{
public:
    /**
     * @brief Shows an error message to the user.
     * @param parent The parent widget for the message box.
     * @param title The title of the message box.
     * @param message The error message to display.
     */
    static void showError(QWidget* parent, const QString& title, const QString& message);
};

#endif // ERRORHANDLER_H
