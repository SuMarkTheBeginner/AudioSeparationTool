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

    // Debug: Connect checkbox state changes
    connect(m_checkBox, &QCheckBox::stateChanged, this, [this](int state) {
        QString stateStr = (state == Qt::Checked) ? "CHECKED" : "UNCHECKED";
        qDebug() << "FileWidget::checkboxStateChanged - File:" << m_filePath
                 << "State:" << stateStr;
    });

    // File name label
    QFileInfo fi(m_filePath);
    fileNameLabel = new QLabel(fi.fileName(), headerWidget);
    fileNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fileNameLabel->setMinimumWidth(50); // Ensure some minimum space for filename
    fileNameLabel->setWordWrap(false);
    headerLayout->addWidget(fileNameLabel, 1); // Give it expansion priority

    // Play button
    playButton = new QToolButton(headerWidget);
    playButton->setText("▶"); // Play icon
    playButton->setToolTip("播放此檔案");
    playButton->setFixedSize(20, 20);
    playButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    headerLayout->addWidget(playButton);

    // Remove button ("X")
    QToolButton* removeButton = new QToolButton(headerWidget);
    removeButton->setText("✕"); // 或者用 "X"、垃圾桶圖示
    removeButton->setToolTip("移除此檔案");
    removeButton->setFixedSize(20, 20);
    removeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    headerLayout->addWidget(removeButton);

    mainLayout->addWidget(headerWidget);

    // Install event filter for checkbox toggle
    headerWidget->installEventFilter(this);

    // --- Connect play button ---
    connect(playButton, &QToolButton::clicked, this, [this]() {
        emit playRequested(m_filePath);
    });

    // --- Connect remove button ---
    connect(removeButton, &QToolButton::clicked, this, [this]() {
        emit fileRemoved(m_filePath);  // 自訂一個 signal 給外部 MainWindow
        this->deleteLater();           // 自己刪掉自己
    });

    // --- Connect play button ---
    connect(playButton, &QToolButton::clicked, this, [this]() {
        emit playRequested(m_filePath);
    });
}

/**
 * @brief Event filter to handle mouse events on the header widget.
 *
 * Toggles the checkbox when the header widget is clicked with the mouse button,
 * but ignores clicks within the checkbox area since it handles its own clicks.
 *
 * @param obj The object receiving the event.
 * @param event The event to filter.
 * @return true if the event is handled (mouse button release on headerWidget), false otherwise.
 */
bool FileWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == headerWidget && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        // Check if the click position is within the checkbox's rectangle
        if (!m_checkBox->geometry().contains(mouseEvent->pos())) {
            m_checkBox->toggle();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}
