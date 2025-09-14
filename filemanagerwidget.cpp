#include "filemanagerwidget.h"
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDragEnterEvent>
#include <QMimeData>
#include "resourcemanager.h"
#include "constants.h"
#include "errorhandler.h"

/**
 * @brief Constructs the FileManagerWidget.
 * @param fileType The type of files managed by this widget.
 * @param parent The parent widget (default is nullptr).
 */
FileManagerWidget::FileManagerWidget(ResourceManager::FileType fileType, QWidget* parent)
    : QWidget(parent), m_fileType(fileType)
{
    setAcceptDrops(true);
}

/**
 * @brief Sets up the common UI components.
 * @param instructionText Text for the instruction label.
 * @param addFolderText Text for the add folder button.
 * @param addFileText Text for the add file button.
 */
void FileManagerWidget::setupCommonUI(const QString& instructionText, const QString& addFolderText, const QString& addFileText)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* infoLabel = new QLabel(instructionText, this);
    layout->addWidget(infoLabel);

    statusLabel = new QLabel("", this);
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    // Scrollable area for content
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    fileContainer = new QWidget(scrollArea);
    fileLayout = new QVBoxLayout(fileContainer);
    fileLayout->setAlignment(Qt::AlignTop);
    fileLayout->setSpacing(0);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileContainer->setLayout(fileLayout);
    scrollArea->setWidget(fileContainer);
    scrollArea->setMinimumHeight(Constants::SCROLL_AREA_MIN_HEIGHT);

    layout->addWidget(scrollArea);

    // Buttons for adding files and folders
    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
    addFolderButton = new QPushButton(addFolderText, this);
    addFileButton = new QPushButton(addFileText, this);
    addButtonsLayout->addWidget(addFolderButton);
    addButtonsLayout->addWidget(addFileButton);
    layout->addLayout(addButtonsLayout);

    setLayout(layout);

    // Connect buttons
    connect(addFolderButton, &QPushButton::clicked, [this]() {
        QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder");
        if (!folderPath.isEmpty()) {
            addFolder(folderPath);
        }
    });

    connect(addFileButton, &QPushButton::clicked, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select WAV Files", QString(), "WAV Files (*.wav)");
        for (const QString& file : files) {
            addSingleFile(file);
        }
    });
}

/**
 * @brief Handles drag enter events for drag-and-drop functionality.
 * @param event The drag enter event.
 */
void FileManagerWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

/**
 * @brief Handles drop events for drag-and-drop functionality.
 * @param event The drop event.
 */
void FileManagerWidget::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        QString path = url.toLocalFile();
        QFileInfo fi(path);

        if (fi.isDir()) {
            addFolder(path);
        } else if (fi.isFile() && fi.suffix().toLower() == Constants::WAV_EXTENSION.mid(1)) { // Remove the dot
            addSingleFile(path);
        }
    }
}

/**
 * @brief Adds a folder to the widget.
 * @param folderPath The path to the folder.
 */
void FileManagerWidget::addFolder(const QString& folderPath)
{
    ResourceManager* rm = ResourceManager::instance();

    // Check if folder exists and has WAV files
    QDir dir(folderPath);
    if (!dir.exists()) {
        statusLabel->setText(Constants::FOLDER_NOT_EXIST.arg(folderPath));
        return;
    }

    QStringList wavFiles = dir.entryList(QStringList() << Constants::WAV_EXTENSION, QDir::Files);
    if (wavFiles.isEmpty()) {
        statusLabel->setText(Constants::NO_WAV_FILES_IN_FOLDER.arg(folderPath));
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    // Remove single files that are in this folder
    QMap<QString, FileWidget*> singleFiles = rm->getSingleFiles(m_fileType);
    for (const QString& filePath : singleFiles.keys()) {
        if (filePath.startsWith(folderPath + "/")) {
            rm->removeFile(filePath, m_fileType);
            FileWidget* fw = singleFiles[filePath];
            fileLayout->removeWidget(fw);
            // Widget is deleted in ResourceManager
        }
    }

    // Delegate to ResourceManager
    FolderWidget* folderWidget = rm->addFolder(folderPath, fileContainer, m_fileType);

    if (folderWidget) {
        // Add to layout if newly created
        if (fileLayout->indexOf(folderWidget) == -1) {
            fileLayout->addWidget(folderWidget);
        }

        // Handle removal connections
        connect(folderWidget, &FolderWidget::fileRemoved, this, [this, rm](const QString& path){
            rm->removeFile(path, m_fileType);
        });

        connect(folderWidget, &FolderWidget::folderRemoved, this, [this, rm, folderWidget](const QString& folderPath){
            rm->removeFolder(folderPath, m_fileType);
            fileLayout->removeWidget(folderWidget);
            // Widget is deleted in ResourceManager
        });

        connect(folderWidget, &FolderWidget::playRequested, this, [this](const QString& path){
            emit playRequested(path);
        });
    }
}

/**
 * @brief Adds a single file to the widget.
 * @param filePath The path to the file.
 */
void FileManagerWidget::addSingleFile(const QString& filePath)
{
    ResourceManager* rm = ResourceManager::instance();

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        statusLabel->setText(Constants::FILE_NOT_EXIST.arg(filePath));
        return;
    }
    if (!fi.isReadable()) {
        statusLabel->setText(Constants::FILE_NOT_READABLE.arg(filePath));
        return;
    }
    if (fi.suffix().toLower() != Constants::WAV_EXTENSION.mid(1)) {
        statusLabel->setText(Constants::FILE_NOT_WAV.arg(filePath));
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    FileWidget* fileWidget = rm->addSingleFile(filePath, fileContainer, m_fileType);
    if (fileWidget) {
        fileLayout->addWidget(fileWidget);

        connect(fileWidget, &FileWidget::fileRemoved,
                this, [this, rm](const QString& path){
                    rm->removeFile(path, m_fileType);
                });

        connect(fileWidget, &FileWidget::playRequested,
                this, [this](const QString& path){
                    emit playRequested(path);
                });
    }
}
