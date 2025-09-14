#ifndef WIDECHECKBOX_H
#define WIDECHECKBOX_H

#include <QCheckBox>
#include <QMouseEvent>
#include <QKeyEvent>

/**
 * @brief A custom QCheckBox with enhanced hit area.
 *
 * This class provides a wide checkbox widget by inheriting from QCheckBox
 * and overriding mouse and keyboard event handlers to make the entire widget
 * area clickable, not just the small checkbox itself.
 */
class WideCheckBox : public QCheckBox {
    Q_OBJECT

public:
    /**
     * @brief Constructs the WideCheckBox.
     * @param parent The parent widget (default is nullptr).
     */
    explicit WideCheckBox(QWidget* parent = nullptr);

    /**
     * @brief Constructs the WideCheckBox with text.
     * @param text The text to display next to the checkbox.
     * @param parent The parent widget (default is nullptr).
     */
    explicit WideCheckBox(const QString& text, QWidget* parent = nullptr);

protected:
    /**
     * @brief Handles mouse press events.
     * @param event The mouse event.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse double-click events.
     * @param event The mouse event.
     */
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /**
     * @brief Handles key press events.
     * @param event The key event.
     */
    void keyPressEvent(QKeyEvent* event) override;
};

#endif // WIDECHECKBOX_H

