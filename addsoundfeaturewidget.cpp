#include "addsoundfeaturewidget.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

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

        ResourceManager::instance()->startGenerateAudioFeatures(selectedFiles, outputFileName);
    });
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
    QList<FolderWidget*> folders = rm->getFolders(fileType()).values();
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
    QList<FileWidget*> singles = rm->getSingleFiles(fileType()).values();
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
