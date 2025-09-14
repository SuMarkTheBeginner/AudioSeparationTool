#include "addsoundfeaturewidget.h"
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <QDragEnterEvent>
#include <QMimeData>

#include "resourcemanager.h"


/**
 * @brief Constructs the AddSoundFeatureWidget.
 *
 * Enables drag-and-drop functionality and sets up the user interface.
 *
 * @param parent The parent widget (default is nullptr).
 */
AddSoundFeatureWidget::AddSoundFeatureWidget(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setupUI();
}

/**
 * @brief Sets up the user interface components.
 *
 * Creates the main layout, info label, scrollable area with folder and single file containers,
 * and buttons for selecting folders and creating features. Connects the select folder button
 * to open a directory dialog.
 */
void AddSoundFeatureWidget::setupUI()
{
    layout = new QVBoxLayout(this);

    infoLabel = new QLabel("Select folders containing WAV files or drag them here:", this);
    layout->addWidget(infoLabel);

    statusLabel = new QLabel("", this);
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    // Scrollable area for content
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setAlignment(Qt::AlignTop);
    scrollLayout->setSpacing(0);
    scrollLayout->setContentsMargins(0, 0, 0, 0);

    // Container for folder widgets
    folderContainer = new QWidget(scrollContent);
    folderLayout = new QVBoxLayout(folderContainer);
    folderLayout->setAlignment(Qt::AlignTop);
    folderLayout->setSpacing(0);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderContainer->setLayout(folderLayout);
    folderContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    folderContainer->setMinimumHeight(0);

    // Container for single file widgets
    singleFileContainer = new QWidget(scrollContent);
    singleFileLayout = new QVBoxLayout(singleFileContainer);
    singleFileLayout->setAlignment(Qt::AlignTop);
    singleFileLayout->setSpacing(0);
    singleFileLayout->setContentsMargins(0, 0, 0, 0);
    singleFileContainer->setLayout(singleFileLayout);
    singleFileContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    singleFileContainer->setMinimumHeight(0);

    scrollLayout->addWidget(folderContainer);
    scrollLayout->addWidget(singleFileContainer);
    scrollLayout->addStretch();
    scrollContent->setLayout(scrollLayout);

    scrollArea->setWidget(scrollContent);
    scrollArea->setMinimumHeight(300);

    layout->addWidget(scrollArea);

    // Buttons for selecting files and folders
    QHBoxLayout* selectBtnLayout = new QHBoxLayout();
    selectFilesBtn = new QPushButton("Select WAV Files", this);
    selectFolderBtn = new QPushButton("Select Folder", this);
    selectBtnLayout->addWidget(selectFilesBtn);
    selectBtnLayout->addWidget(selectFolderBtn);
    layout->addLayout(selectBtnLayout);

    // File name input and create feature button
    QHBoxLayout* inputBtnLayout = new QHBoxLayout();
    fileNameLabel = new QLabel("Output File Name:", this);
    fileNameInput = new QLineEdit(this);
    fileNameInput->setPlaceholderText("Enter file name...");
    createFeatureBtn = new QPushButton("Create Feature", this);

    inputBtnLayout->addWidget(fileNameLabel);
    inputBtnLayout->addWidget(fileNameInput);
    inputBtnLayout->addWidget(createFeatureBtn);

    layout->addLayout(inputBtnLayout);

    setLayout(layout);

    // Connect select files button to open file dialog
    connect(selectFilesBtn, &QPushButton::clicked, [this]() {
        QFileDialog dialog(this);
        dialog.setWindowTitle("Select WAV Files");
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.setNameFilter("WAV Files (*.wav);;All Files (*)");
        if (dialog.exec()) {
            QStringList selected = dialog.selectedFiles();
            for (const QString& path : selected) {
                QFileInfo fi(path);
                if (fi.isFile() && fi.suffix().toLower() == "wav") {
                    addSingleFile(path);
                }
            }
        }
    });

    // Connect select folder button to open directory dialog
    connect(selectFolderBtn, &QPushButton::clicked, [this]() {
        QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder");
        if (!folderPath.isEmpty()) {
            addFolder(folderPath);
        }
    });

    // Connect create feature button to process selected files
    connect(createFeatureBtn, &QPushButton::clicked, [this]() {
        QStringList selectedFiles;

        // Collect selected files from folder widgets
        QMap<QString, FolderWidget*> folders = ResourceManager::instance()->getFolders();
        for (FolderWidget* fw : folders.values()) {
            QStringList selected = fw->getSelectedFiles();
            for (const QString& path : selected) {
                selectedFiles.append(path);
            }
        }

        // Collect selected files from single file widgets
        QMap<QString, FileWidget*> singles = ResourceManager::instance()->getSingleFiles();
        for (FileWidget* fw : singles.values()) {
            if (fw->checkBox()->isChecked()) {
                selectedFiles.append(fw->filePath());
            }
        }

        QString outputFileName = getOutputFileName();
        if (outputFileName.isEmpty()) {
            qDebug() << "Output file name is empty.";
            return;
        }

        ResourceManager::instance()->generateAudioFeatures(selectedFiles, outputFileName);
    });
}



/**
 * @brief Adds a folder and its WAV files to the interface.
 *
 * Delegates to ResourceManager to add the folder and handles UI updates.
 *
 * @param folderPath The path to the folder to add.
 */
void AddSoundFeatureWidget::addFolder(const QString& folderPath)
{
    ResourceManager* rm = ResourceManager::instance();

    // Check if folder exists and has WAV files (for status message)
    QDir dir(folderPath);
    if (!dir.exists()) {
        statusLabel->setText("Error: Folder does not exist: " + folderPath);
        qDebug() << "Folder does not exist:" << folderPath;
        return;
    }

    QStringList wavFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);
    if (wavFiles.isEmpty()) {
        statusLabel->setText("No WAV files found in folder: " + folderPath);
        qDebug() << "No WAV files found in folder:" << folderPath;
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    // Remove single files that are in this folder
    QMap<QString, FileWidget*> singleFiles = rm->getSingleFiles();
    for (const QString& filePath : singleFiles.keys()) {
        if (filePath.startsWith(folderPath + "/")) {
            rm->removeFile(filePath);
            FileWidget* fw = singleFiles[filePath];
            singleFileLayout->removeWidget(fw);
            // Widget is deleted in ResourceManager
        }
    }

    // Delegate to ResourceManager
    FolderWidget* folderWidget = rm->addFolder(folderPath, folderContainer);

    if (folderWidget) {
        // Add to layout if newly created
        if (folderLayout->indexOf(folderWidget) == -1) { // If not already in layout
            folderLayout->addWidget(folderWidget);
        }

        // Handle removal connections
        connect(folderWidget, &FolderWidget::fileRemoved, this, [this, rm](const QString& path){
            rm->removeFile(path);
        });

        connect(folderWidget, &FolderWidget::folderRemoved, this, [this, rm, folderWidget](const QString& folderPath){
            rm->removeFolder(folderPath);
            folderLayout->removeWidget(folderWidget);
            // Widget is deleted in ResourceManager
        });

        connect(folderWidget, &FolderWidget::playRequested, this, [this](const QString& path){
            emit playRequested(path);
        });
    }
}


/**
 * @brief Adds a single WAV file to the interface.
 *
 * Delegates to ResourceManager to add the file and handles UI updates.
 *
 * @param filePath The path to the WAV file to add.
 */
void AddSoundFeatureWidget::addSingleFile(const QString& filePath)
{
    ResourceManager* rm = ResourceManager::instance();

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        statusLabel->setText("Error: File does not exist: " + filePath);
        qDebug() << "File does not exist:" << filePath;
        return;
    }
    if (!fi.isReadable()) {
        statusLabel->setText("Error: File is not readable: " + filePath);
        qDebug() << "File is not readable:" << filePath;
        return;
    }
    if (fi.suffix().toLower() != "wav") {
        statusLabel->setText("Error: File is not a WAV file: " + filePath);
        qDebug() << "File is not a WAV file:" << filePath;
        return;
    }

    statusLabel->setText(""); // Clear previous errors

    FileWidget* fileWidget = rm->addSingleFile(filePath, singleFileContainer);
    if (fileWidget) {
        singleFileLayout->addWidget(fileWidget);

        connect(fileWidget, &FileWidget::fileRemoved,
                this, [this, rm](const QString& path){
                    rm->removeFile(path);
                });

        connect(fileWidget, &FileWidget::playRequested,
                this, [this](const QString& path){
                    // Emit to MainWindow
                    emit playRequested(path);
                });
    }
}

/**
 * @brief Handles drag enter events for drag-and-drop functionality.
 *
 * Accepts the drag event if it contains URLs (file paths), otherwise ignores it.
 *
 * @param event The drag enter event.
 */
void AddSoundFeatureWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

/**
 * @brief Handles drop events for drag-and-drop functionality.
 *
 * Processes the dropped URLs, determines if each is a directory or WAV file,
 * and adds folders or single files accordingly.
 *
 * @param event The drop event.
 */
void AddSoundFeatureWidget::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        QString path = url.toLocalFile();
        QFileInfo fi(path);

        if (fi.isDir()) addFolder(path);
        else if (fi.isFile() && fi.suffix().toLower() == "wav") addSingleFile(path);
    }
}

/**
 * @brief Sorts all folders and single files based on the specified type.
 *
 * Gets widgets from ResourceManager and sorts them in the layouts.
 *
 * @param type The sorting criteria (name ascending/descending, creation time ascending/descending).
 */
void AddSoundFeatureWidget::sortAll(SortType type)
{
    ResourceManager* rm = ResourceManager::instance();

    // Sort folders
    QList<FolderWidget*> folders = rm->getFolders().values();
    std::sort(folders.begin(), folders.end(), [type](FolderWidget* a, FolderWidget* b) {
        QFileInfo fa(a->folderPath());
        QFileInfo fb(b->folderPath());
        switch (type) {
        case SortType::NameAsc: return fa.fileName() < fb.fileName();
        case SortType::NameDesc: return fa.fileName() > fb.fileName();
        case SortType::CreatedAsc: return fa.birthTime() < fb.birthTime();
        case SortType::CreatedDesc: return fa.birthTime() > fb.birthTime();
        }
        return true;
    });
    for (int i = 0; i < folders.size(); ++i) {
        folderLayout->insertWidget(i, folders[i]);
    }

    // Sort single files
    QList<FileWidget*> singles = rm->getSingleFiles().values();
    std::sort(singles.begin(), singles.end(), [type](FileWidget* a, FileWidget* b) {
        QFileInfo fa(a->filePath());
        QFileInfo fb(b->filePath());
        switch (type) {
        case SortType::NameAsc: return fa.fileName() < fb.fileName();
        case SortType::NameDesc: return fa.fileName() > fb.fileName();
        case SortType::CreatedAsc: return fa.birthTime() < fb.birthTime();
        case SortType::CreatedDesc: return fa.birthTime() > fb.birthTime();
        }
        return true;
    });
    for (int i = 0; i < singles.size(); ++i) {
        singleFileLayout->insertWidget(i, singles[i]);
    }
}

/**
 * @brief Gets the output file name from the input field.
 * @return The file name entered by the user.
 */
QString AddSoundFeatureWidget::getOutputFileName() const
{
    return fileNameInput->text();
}
