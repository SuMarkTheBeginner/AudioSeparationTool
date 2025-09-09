#include "addsoundfeaturewidget.h"
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <QDragEnterEvent>
#include <QMimeData>

#include "htsatprocessor.h"


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

    // Buttons for actions
    QHBoxLayout* btnLayout = new QHBoxLayout();
    createFeatureBtn = new QPushButton("Create Feature", this);
    selectFolderBtn = new QPushButton("Select Folder", this);
    btnLayout->addWidget(createFeatureBtn);
    btnLayout->addWidget(selectFolderBtn);
    layout->addLayout(btnLayout);

    setLayout(layout);

    // Connect select folder button to open directory dialog
    connect(selectFolderBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Folder", "");
        if (!dir.isEmpty()) addFolder(dir);
    });

    // Connect create feature button to print selected files
    connect(createFeatureBtn, &QPushButton::clicked, [this]() {
        qDebug() << "Selected files:";
        for (const QString& filePath : addedFilePaths) {
            qDebug() << filePath;
        }
    });
}



/**
 * @brief Adds a folder and its WAV files to the interface.
 *
 * Scans the specified folder for WAV files, creates or reuses a FolderWidget,
 * handles duplicates by removing single file entries if they exist, and adds
 * new files to the folder widget.
 *
 * @param folderPath The path to the folder to add.
 */
void AddSoundFeatureWidget::addFolder(const QString& folderPath)
{
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
    qDebug() << "Adding folder:" << folderPath << "with" << wavFiles.size() << "WAV files";

    // Prepare FolderWidget: reuse existing or create new
    FolderWidget* folderWidget;
    if (folderMap.contains(folderPath)) {
        folderWidget = folderMap[folderPath];
    } else {
        folderWidget = new FolderWidget(folderPath, folderContainer);
        folderLayout->addWidget(folderWidget);
        folderMap.insert(folderPath, folderWidget);


    }

    QStringList newFiles;

    for (const QString& f : wavFiles) {
        QString fullPath = dir.absoluteFilePath(f);

        // If file already exists as a single file, remove it from single file list
        if (singleFileMap.contains(fullPath)) {
            FileWidget* fw = singleFileMap.take(fullPath);
            singleFileLayout->removeWidget(fw);
            fw->deleteLater();
            addedFilePaths.remove(fullPath);
        }

        // If not already added, include in new files list
        if (!addedFilePaths.contains(fullPath)) {
            newFiles.append(f);
            addedFilePaths.insert(fullPath);

            emit fileAdded(fullPath);
        }
    }

    if (!newFiles.isEmpty()) {
        folderWidget->appendFiles(newFiles);
    }

    connect(folderWidget, &FolderWidget::fileRemoved, this, [this](const QString& path){
        addedFilePaths.remove(path);
        emit fileRemoved(path); // 通知 MainWindow
    });

    connect(folderWidget, &FolderWidget::folderRemoved, this, [this, folderWidget](const QString& folderPath){
        folderLayout->removeWidget(folderWidget);
        folderWidget->deleteLater();
        folderMap.remove(folderPath);
        // Remove all files in the folder from addedFilePaths and emit fileRemoved
        QSet<QString> toRemove;
        for (const QString& filePath : addedFilePaths) {
            if (filePath.startsWith(folderPath + "/")) {
                toRemove.insert(filePath);
            }
        }
        for (const QString& filePath : toRemove) {
            addedFilePaths.remove(filePath);
            emit fileRemoved(filePath);
        }
        emit folderRemoved(folderPath);
    });
}


/**
 * @brief Adds a single WAV file to the interface.
 *
 * Checks if the file is already added to prevent duplicates, creates a FileWidget,
 * adds it to the single file container, and updates the maps and sets.
 *
 * @param filePath The path to the WAV file to add.
 */
void AddSoundFeatureWidget::addSingleFile(const QString& filePath)
{
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
    if (addedFilePaths.contains(fi.absoluteFilePath())) {
        statusLabel->setText("File already added: " + filePath);
        qDebug() << "File already added:" << filePath;
        return;
    }

    statusLabel->setText(""); // Clear previous errors
    qDebug() << "Adding single file:" << filePath;

    FileWidget* fileWidget = new FileWidget(filePath, singleFileContainer);
    singleFileLayout->addWidget(fileWidget);
    singleFileMap.insert(fi.absoluteFilePath(), fileWidget);
    addedFilePaths.insert(fi.absoluteFilePath());

    connect(fileWidget, &FileWidget::fileRemoved,
            this, [this](const QString& path){
                addedFilePaths.remove(path);
                emit fileRemoved(path);
            });

    emit fileAdded(fi.absoluteFilePath());
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
 * Uses std::sort with lambda functions to compare FolderWidget and FileWidget
 * instances based on name or creation time, then reorders them in the layouts.
 *
 * @param type The sorting criteria (name ascending/descending, creation time ascending/descending).
 */
void AddSoundFeatureWidget::sortAll(SortType type)
{
    // Sort folders
    QList<FolderWidget*> folders = folderMap.values();
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
    QList<FileWidget*> singles = singleFileMap.values();
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
