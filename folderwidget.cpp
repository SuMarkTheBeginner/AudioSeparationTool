#include "folderwidget.h"
#include <QFileInfo>
#include <QDir>
#include <QSignalBlocker>
#include <QEvent>
#include <QPushButton>

/**
 * @brief Constructs the FolderWidget.
 *
 * Sets the frame style and shadow, stores the folder path, and sets up the UI.
 *
 * @param folderPath The path to the folder.
 * @param parent The parent widget (default is nullptr).
 */
FolderWidget::FolderWidget(const QString& folderPath, QWidget* parent)
    : QFrame(parent), m_folderPath(folderPath)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setupUI();
}

/**
 * @brief Sets up the user interface components.
 *
 * Creates the main layout, header with tri-state checkbox, folder name, and arrow label,
 * files container, and folder path label. Sets up connections for checkbox interactions
 * and installs event filters for click handling.
 */
void FolderWidget::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(2);

    // Header widget
    headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    folderCheckBox = new QCheckBox(headerWidget);
    folderCheckBox->setTristate(true);
    folderCheckBox->setCheckState(Qt::Checked);

    QFileInfo fi(m_folderPath);
    folderNameLabel = new QLabel(fi.fileName(), headerWidget);
    arrowLabel = new QLabel("v", headerWidget);
    arrowLabel->setFixedWidth(12);

    removeFolderBtn = new QPushButton("✕", headerWidget);
    removeFolderBtn->setFixedSize(20, 20);

    headerLayout->addWidget(folderCheckBox);
    headerLayout->addWidget(folderNameLabel, 1);
    headerLayout->addWidget(arrowLabel);
    headerLayout->addWidget(removeFolderBtn);

    mainLayout->addWidget(headerWidget);

    // Files container
    filesContainer = new QWidget(this);
    filesLayout = new QVBoxLayout(filesContainer);
    filesLayout->setContentsMargins(20, 0, 0, 0);
    filesLayout->setSpacing(0);
    filesContainer->setLayout(filesLayout);
    mainLayout->addWidget(filesContainer);

    QLabel* folderPathLabel = new QLabel(m_folderPath, this);
    folderPathLabel->setStyleSheet("color: gray; font-size: 10px; padding-left: 4px;");
    mainLayout->addWidget(folderPathLabel);

    // Interaction logic for folder checkbox
    connect(folderCheckBox, &QCheckBox::clicked, this, [this](bool checked) {
        QSignalBlocker block(folderCheckBox);
        folderCheckBox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

        for (WideCheckBox* cb : filesCheckBoxes) {
            QSignalBlocker blockChild(cb);
            cb->setChecked(checked);
        }
    });

    for (WideCheckBox* cb : filesCheckBoxes) {
        connect(cb, &QCheckBox::stateChanged, this, &FolderWidget::refreshFolderCheckState);
    }

    connect(removeFolderBtn, &QPushButton::clicked, this, [this]() {
        emit folderRemoved(m_folderPath);
    });

    headerWidget->installEventFilter(this);
    folderNameLabel->installEventFilter(this);
    arrowLabel->installEventFilter(this);
}

/**
 * @brief Appends files to the folder widget.
 *
 * Filters out already added files to prevent duplicates, then calls addFilesInternal
 * to add the new files.
 *
 * @param files List of file names to append.
 */
void FolderWidget::appendFiles(const QStringList& files)
{
    QStringList newFiles;
    for (const QString& f : files) {
        QString fullPath = QDir(m_folderPath).absoluteFilePath(f);
        if (!addedFiles.contains(fullPath)) {
            addedFiles.insert(fullPath);
            newFiles.append(f);
        }
    }
    addFilesInternal(newFiles);
}

/**
 * @brief Adds files internally to the widget.
 *
 * Creates WideCheckBox instances for each file, sets them up with styling and connections,
 * adds them to the files layout, and refreshes the folder checkbox state.
 *
 * @param files List of file names to add.
 */
void FolderWidget::addFilesInternal(const QStringList& files)
{
    for(const QString& f : files){
        QWidget* fileItemWidget = new QWidget(filesContainer);
        QHBoxLayout* fileLayout = new QHBoxLayout(fileItemWidget);
        fileLayout->setContentsMargins(0, 0, 0, 0);
        fileLayout->setSpacing(6);

        QLabel* arrowLabel = new QLabel(">", fileItemWidget);
        arrowLabel->setFixedWidth(12);

        WideCheckBox* cb = new WideCheckBox(f, fileItemWidget);
        cb->setChecked(true);
        cb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cb->setMinimumHeight(28);
        cb->setStyleSheet("QCheckBox { padding: 6px; }");

        QPushButton* removeBtn = new QPushButton("✕", fileItemWidget);
        removeBtn->setFixedSize(20, 20);

        fileLayout->addWidget(arrowLabel);
        fileLayout->addWidget(cb);
        fileLayout->addWidget(removeBtn);
        fileItemWidget->setLayout(fileLayout);

        filesLayout->addWidget(fileItemWidget);
        filesCheckBoxes.append(cb);

        connect(cb, &QCheckBox::stateChanged, this, &FolderWidget::refreshFolderCheckState);
        connect(removeBtn, &QPushButton::clicked, this, [this, f, fileItemWidget]() {
            filesLayout->removeWidget(fileItemWidget);
            fileItemWidget->deleteLater();
            filesCheckBoxes.removeOne(findCheckBoxByText(f));
            addedFiles.remove(QDir(m_folderPath).absoluteFilePath(f));
            emit fileRemoved(QDir(m_folderPath).absoluteFilePath(f));
            refreshFolderCheckState();
            if (filesCheckBoxes.isEmpty()) {
                emit folderRemoved(m_folderPath);
            }
        });
    }
    refreshFolderCheckState();
}

/**
 * @brief Refreshes the folder checkbox state based on file checkbox states.
 *
 * Counts the number of checked file checkboxes and sets the folder checkbox
 * to Checked, Unchecked, or PartiallyChecked accordingly.
 */
void FolderWidget::refreshFolderCheckState()
{
    int checkedCount = 0;
    const int total = filesCheckBoxes.size();
    for (WideCheckBox* cb : filesCheckBoxes) if (cb->checkState() == Qt::Checked) ++checkedCount;

    QSignalBlocker block(folderCheckBox);
    if (checkedCount == 0) folderCheckBox->setCheckState(Qt::Unchecked);
    else if (checkedCount == total) folderCheckBox->setCheckState(Qt::Checked);
    else folderCheckBox->setCheckState(Qt::PartiallyChecked);
}

/**
 * @brief Toggles the visibility of the file list.
 *
 * Shows or hides the files container and updates the arrow label text.
 */
void FolderWidget::toggleFilesVisible()
{
    bool visible = filesContainer->isVisible();
    filesContainer->setVisible(!visible);
    arrowLabel->setText(visible ? ">" : "v");
}

/**
 * @brief Event filter to handle mouse events on header components.
 *
 * Toggles the file list visibility when the header widget, folder name label,
 * or arrow label is clicked with the mouse button.
 *
 * @param obj The object receiving the event.
 * @param event The event to filter.
 * @return true if the event is handled (mouse button release on header components), false otherwise.
 */
bool FolderWidget::eventFilter(QObject* obj, QEvent* event)
{
    if ((obj == headerWidget || obj == folderNameLabel || obj == arrowLabel) && event->type() == QEvent::MouseButtonRelease) {
        toggleFilesVisible();
        return true;
    }
    return QFrame::eventFilter(obj, event);
}


WideCheckBox* FolderWidget::findCheckBoxByText(const QString& text) const
{
    for(WideCheckBox* cb : filesCheckBoxes){
        if(cb->text() == text) return cb;
    }
    return nullptr;
}
