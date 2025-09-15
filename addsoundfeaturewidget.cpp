#include "addsoundfeaturewidget.h"
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <QDragEnterEvent>
#include <QMimeData>

#include "resourcemanager.h"
#include "constants.h"


/**
 * @brief Constructs the AddSoundFeatureWidget.
 *
 * Enables drag-and-drop functionality and sets up the user interface.
 *
 * @param parent The parent widget (default is nullptr).
 */
AddSoundFeatureWidget::AddSoundFeatureWidget(QWidget *parent)
    : FileManagerWidget(ResourceManager::FileType::WavForFeature, parent)
{
    setupCommonUI(Constants::SELECT_WAV_FOLDERS_TEXT, "Select Folder", "Select WAV Files");
    setupFeatureInputUI();
    setupFeatureButtonConnections();
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
    // Removed because setupCommonUI is called in constructor now
}

/**
 * @brief Sets up the feature input UI components.
 *
 * Creates the output file name input field and create feature button.
 */
void AddSoundFeatureWidget::setupFeatureInputUI()
{
    // Access the main layout from the base class
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(this->QWidget::layout());
    if (!mainLayout) {
        qDebug() << "Error: Main layout is not a QVBoxLayout";
        return;
    }

    // Add create feature button and output file name input below the scroll area and buttons
    QHBoxLayout* inputBtnLayout = new QHBoxLayout();
    fileNameLabel = new QLabel(Constants::OUTPUT_FILE_NAME_LABEL, this);
    fileNameInput = new QLineEdit(this);
    fileNameInput->setPlaceholderText(Constants::FILE_NAME_PLACEHOLDER);
    createFeatureBtn = new QPushButton(Constants::CREATE_FEATURE_BUTTON, this);

    inputBtnLayout->addWidget(fileNameLabel);
    inputBtnLayout->addWidget(fileNameInput);
    inputBtnLayout->addWidget(createFeatureBtn);

    mainLayout->addLayout(inputBtnLayout);
}

/**
 * @brief Sets up the connections for the feature creation button.
 *
 * Connects the create feature button to process selected files.
 */
void AddSoundFeatureWidget::setupFeatureButtonConnections()
{
    // Connect create feature button to process selected files
    connect(createFeatureBtn, &QPushButton::clicked, [this]() {
        QStringList selectedFiles;

        // Collect selected files from folder widgets
        QMap<QString, FolderWidget*> folders = ResourceManager::instance()->getFolders(fileType());
        for (FolderWidget* fw : folders.values()) {
            QStringList selected = fw->getSelectedFiles();
            for (const QString& path : selected) {
                selectedFiles.append(path);
            }
        }

        // Collect selected files from single file widgets
        QMap<QString, FileWidget*> singles = ResourceManager::instance()->getSingleFiles(fileType());
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
        statusLabel->setText(Constants::FOLDER_NOT_EXIST.arg(folderPath));
        qDebug() << "Folder does not exist:" << folderPath;
        return;
    }

    QStringList wavFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);
    if (wavFiles.isEmpty()) {
        statusLabel->setText(Constants::NO_WAV_FILES_IN_FOLDER.arg(folderPath));
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
        statusLabel->setText(Constants::FILE_NOT_EXIST.arg(filePath));
        qDebug() << "File does not exist:" << filePath;
        return;
    }
    if (!fi.isReadable()) {
        statusLabel->setText(Constants::FILE_NOT_READABLE.arg(filePath));
        qDebug() << "File is not readable:" << filePath;
        return;
    }
    if (fi.suffix().toLower() != "wav") {
        statusLabel->setText(Constants::FILE_NOT_WAV.arg(filePath));
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
