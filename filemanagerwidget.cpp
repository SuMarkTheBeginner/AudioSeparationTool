#include "filemanagerwidget.h"
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDragEnterEvent>
#include <QMimeData>
#include "resourcemanager.h"

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

    QWidget* mainContainer = new QWidget(scrollArea);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainContainer);

    // Folder section
    folderContainer = new QWidget(mainContainer);
    folderLayout = new QVBoxLayout(folderContainer);
    folderLayout->setAlignment(Qt::AlignTop);
    folderLayout->setSpacing(0);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderContainer->setLayout(folderLayout);
    // Set size policy and minimum height to avoid initial vertical space but keep width
    folderContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    folderContainer->setMinimumHeight(0);
    mainLayout->addWidget(folderContainer);

    // Single file section
    singleFileContainer = new QWidget(mainContainer);
    singleFileLayout = new QVBoxLayout(singleFileContainer);
    singleFileLayout->setAlignment(Qt::AlignTop);
    singleFileLayout->setSpacing(0);
    singleFileLayout->setContentsMargins(0, 0, 0, 0);
    singleFileContainer->setLayout(singleFileLayout);
    // Set size policy and minimum height to avoid initial vertical space but keep width
    singleFileContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    singleFileContainer->setMinimumHeight(0);
    mainLayout->addWidget(singleFileContainer);

    mainContainer->setLayout(mainLayout);
    scrollArea->setWidget(mainContainer);
    scrollArea->setMinimumHeight(300);

    layout->addWidget(scrollArea);

    // Buttons for adding files and folders
    addFolderButton = new QPushButton(addFolderText, this);
    addFileButton = new QPushButton(addFileText, this);
    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
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
        } else if (fi.isFile() && fi.suffix().toLower() == "wav") {
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
        statusLabel->setText("Error: Folder does not exist: " + folderPath);
        return;
    }

    QStringList wavFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);
    if (wavFiles.isEmpty()) {
        statusLabel->setText("No WAV files found in folder: " + folderPath);
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    // Remove single files that are in this folder
    QMap<QString, FileWidget*> singleFiles = rm->getSingleFiles(m_fileType);
    for (const QString& filePath : singleFiles.keys()) {
        if (filePath.startsWith(folderPath + "/")) {
            rm->removeFile(filePath, m_fileType);
            FileWidget* fw = singleFiles[filePath];
            singleFileLayout->removeWidget(fw);
            fw->deleteLater();
        }
    }

    // Delegate to ResourceManager
    FolderWidget* folderWidget = rm->addFolder(folderPath, folderContainer, m_fileType);

    if (folderWidget) {
        // Add to layout if newly created
        if (folderLayout->indexOf(folderWidget) == -1) {
            folderLayout->addWidget(folderWidget);
        }

        // Handle removal connections
        connect(folderWidget, &FolderWidget::fileRemoved, this, [this, rm](const QString& path){
            rm->removeFile(path, m_fileType);
        });

        connect(folderWidget, &FolderWidget::folderRemoved, this, [this, rm, folderWidget](const QString& folderPath){
            rm->removeFolder(folderPath, m_fileType);
            folderLayout->removeWidget(folderWidget);
            folderWidget->deleteLater();
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
        statusLabel->setText("Error: File does not exist: " + filePath);
        return;
    }
    if (!fi.isReadable()) {
        statusLabel->setText("Error: File is not readable: " + filePath);
        return;
    }
    if (fi.suffix().toLower() != "wav") {
        statusLabel->setText("Error: File is not a WAV file: " + filePath);
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    FileWidget* fileWidget = rm->addSingleFile(filePath, singleFileContainer, m_fileType);
    if (fileWidget) {
        singleFileLayout->addWidget(fileWidget);

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
