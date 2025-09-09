#include "filewidget.h"
#include <QFileInfo>
#include <QSignalBlocker>
#include <QEvent>
#include <QToolButton>

#include "widecheckbox.h"

/**
 * @brief Constructs the FileWidget.
 *
 * Sets the frame style and shadow, stores the file path, and sets up the UI.
 *
 * @param filePath The path to the WAV file.
 * @param parent The parent widget (default is nullptr).
 */
FileWidget::FileWidget(const QString& filePath, QWidget* parent)
    : QFrame(parent), m_filePath(filePath)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setupUI();
}

/**
 * @brief Sets up the user interface components.
 *
 * Creates the main vertical layout, header widget with checkbox and file name label,
 * and full path label. Installs event filter on headerWidget for click handling.
 */
void FileWidget::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(2);

    // Header widget containing arrow, checkbox, file name, and remove button
    headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    // Checkbox
    m_checkBox = new WideCheckBox(headerWidget);
    m_checkBox->setChecked(true);
    headerLayout->addWidget(m_checkBox);

    // File name label
    QFileInfo fi(m_filePath);
    fileNameLabel = new QLabel(fi.fileName(), headerWidget);
    headerLayout->addWidget(fileNameLabel, 1);

    // Remove button ("X")
    QToolButton* removeButton = new QToolButton(headerWidget);
    removeButton->setText("✕"); // 或者用 "X"、垃圾桶圖示
    removeButton->setToolTip("移除此檔案");
    removeButton->setFixedSize(20, 20);
    headerLayout->addWidget(removeButton);

    mainLayout->addWidget(headerWidget);

    // Full path label
    filePathLabel = new QLabel(m_filePath, this);
    filePathLabel->setStyleSheet("color: gray; font-size: 10px; padding-left: 4px;");
    mainLayout->addWidget(filePathLabel);

    // Install event filter for checkbox toggle
    headerWidget->installEventFilter(this);

    // --- Connect remove button ---
    connect(removeButton, &QToolButton::clicked, this, [this]() {
        emit fileRemoved(m_filePath);  // 自訂一個 signal 給外部 MainWindow
        this->deleteLater();           // 自己刪掉自己
    });
}

/**
 * @brief Event filter to handle mouse events on the header widget.
 *
 * Toggles the checkbox when the header widget is clicked with the mouse button.
 *
 * @param obj The object receiving the event.
 * @param event The event to filter.
 * @return true if the event is handled (mouse button release on headerWidget), false otherwise.
 */
bool FileWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == headerWidget && event->type() == QEvent::MouseButtonRelease) {
        m_checkBox->toggle();
        return true;
    }
    return QFrame::eventFilter(obj, event);
}
