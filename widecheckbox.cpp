#include "widecheckbox.h"
#include <QMouseEvent>

/**
 * @brief Constructs the WideCheckBox.
 *
 * This constructor initializes the WideCheckBox with the given parent.
 * It sets the focus policy to allow keyboard focus and enables mouse tracking.
 *
 * @param parent The parent widget (default is nullptr).
 */
WideCheckBox::WideCheckBox(QWidget* parent)
    : QCheckBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

/**
 * @brief Constructs the WideCheckBox with text.
 *
 * This constructor initializes the WideCheckBox with the given text and parent.
 * It sets the focus policy to allow keyboard focus and enables mouse tracking.
 *
 * @param text The text to display next to the checkbox.
 * @param parent The parent widget (default is nullptr).
 */
WideCheckBox::WideCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

/**
 * @brief Handles mouse press events.
 *
 * This method overrides the mouse press event to toggle the checkbox
 * when the user clicks anywhere within the widget's area, not just on the checkbox itself.
 *
 * @param event The mouse event.
 */
void WideCheckBox::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggle();
        event->accept();
        return;
    }
    QCheckBox::mousePressEvent(event);
}

/**
 * @brief Handles mouse double-click events.
 *
 * This method overrides the mouse double-click event to prevent
 * the default double-click behavior and treat it as a single click.
 *
 * @param event The mouse event.
 */
void WideCheckBox::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggle();
        event->accept();
        return;
    }
    QCheckBox::mouseDoubleClickEvent(event);
}

/**
 * @brief Handles key press events.
 *
 * This method overrides the key press event to allow toggling the checkbox
 * with the spacebar or enter key.
 *
 * @param event The key event.
 */
void WideCheckBox::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        setChecked(!isChecked());
        emit clicked(isChecked());
    } else {
        QCheckBox::keyPressEvent(event);
    }
}
