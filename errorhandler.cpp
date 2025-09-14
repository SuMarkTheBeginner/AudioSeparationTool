#include "errorhandler.h"
#include <QMessageBox>

/**
 * @brief Shows an error message to the user.
 * @param parent The parent widget for the message box.
 * @param title The title of the message box.
 * @param message The error message to display.
 */
void ErrorHandler::showError(QWidget* parent, const QString& title, const QString& message)
{
    QMessageBox::warning(parent, title, message);
}
