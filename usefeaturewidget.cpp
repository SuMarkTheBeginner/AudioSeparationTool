#include "usefeaturewidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

#include "resourcemanager.h"
#include "constants.h"

UseFeatureWidget::UseFeatureWidget(QWidget *parent)
    : FileManagerWidget(ResourceManager::FileType::WavForSeparation, parent)
{
    setupUI();
    loadFeatures();
}

void UseFeatureWidget::setupUI()
{
    mainLayout = new QVBoxLayout(this);

    setupFeatureSelectionUI();
    setupFileManagementUI();
    setupProcessingUI();
    setupConnections();
}

/**
 * @brief Sets up the feature selection UI components.
 *
 * Creates the feature label, combo box, and delete button.
 */
void UseFeatureWidget::setupFeatureSelectionUI()
{
    featureLabel = new QLabel(Constants::SELECT_FEATURE_LABEL, this);
    mainLayout->addWidget(featureLabel);

    // Create horizontal layout for combobox and delete button
    QHBoxLayout* featureLayout = new QHBoxLayout();
    featureComboBox = new QComboBox(this);
    featureLayout->addWidget(featureComboBox);

    QPushButton* deleteButton = new QPushButton(Constants::DELETE_BUTTON, this);
    featureLayout->addWidget(deleteButton);

    mainLayout->addLayout(featureLayout);
}

/**
 * @brief Sets up the file management UI components.
 *
 * Creates the file label, status label, add buttons, and scroll area.
 */
void UseFeatureWidget::setupFileManagementUI()
{
    fileLabel = new QLabel(Constants::SELECT_WAV_FILES_TEXT, this);
    mainLayout->addWidget(fileLabel);

    statusLabel = new QLabel("", this);
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);

    // Add buttons to add files and folders
    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
    QPushButton* addFolderButton = new QPushButton("Add Folder", this);
    QPushButton* addFileButton = new QPushButton("Add File", this);
    addButtonsLayout->addWidget(addFolderButton);
    addButtonsLayout->addWidget(addFileButton);
    mainLayout->addLayout(addButtonsLayout);

    fileScrollArea = new QScrollArea(this);
    fileScrollArea->setWidgetResizable(true);
    fileContainer = new QWidget(this);
    fileLayout = new QVBoxLayout(fileContainer);
    fileScrollArea->setWidget(fileContainer);
    mainLayout->addWidget(fileScrollArea, 1);
}

/**
 * @brief Sets up the processing UI components.
 *
 * Creates the process button, result label, and result list.
 */
void UseFeatureWidget::setupProcessingUI()
{
    processButton = new QPushButton(Constants::PROCESS_BUTTON, this);
    mainLayout->addWidget(processButton);

    resultLabel = new QLabel(Constants::PROCESSED_FILES_LABEL, this);
    mainLayout->addWidget(resultLabel);

    resultList = new QListWidget(this);
    mainLayout->addWidget(resultList, 1);
}

/**
 * @brief Sets up the signal-slot connections.
 *
 * Connects buttons and signals for functionality.
 */
void UseFeatureWidget::setupConnections()
{
    connect(processButton, &QPushButton::clicked, this, &UseFeatureWidget::onProcessClicked);
    connect(this, &UseFeatureWidget::playRequested, this, &UseFeatureWidget::onPlayResult);

    // Find the add buttons from the layout
    QHBoxLayout* addButtonsLayout = qobject_cast<QHBoxLayout*>(mainLayout->itemAt(3)->layout());
    if (addButtonsLayout) {
        QPushButton* addFolderButton = qobject_cast<QPushButton*>(addButtonsLayout->itemAt(0)->widget());
        QPushButton* addFileButton = qobject_cast<QPushButton*>(addButtonsLayout->itemAt(1)->widget());

        if (addFolderButton) {
            connect(addFolderButton, &QPushButton::clicked, this, [this]() {
                QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder to Add");
                if (!folderPath.isEmpty()) {
                    addFolder(folderPath);
                }
            });
        }

        if (addFileButton) {
            connect(addFileButton, &QPushButton::clicked, this, [this]() {
                QStringList files = QFileDialog::getOpenFileNames(this, "Select WAV Files to Add", QString(), "WAV Files (*.wav)");
                for (const QString& file : files) {
                    addSingleFile(file);
                }
            });
        }
    }

    // Find the delete button from the feature layout
    QHBoxLayout* featureLayout = qobject_cast<QHBoxLayout*>(mainLayout->itemAt(1)->layout());
    if (featureLayout) {
        QPushButton* deleteButton = qobject_cast<QPushButton*>(featureLayout->itemAt(1)->widget());
        if (deleteButton) {
            connect(deleteButton, &QPushButton::clicked, this, &UseFeatureWidget::onDeleteClicked);
        }
    }
}

void UseFeatureWidget::loadFeatures()
{
    featureComboBox->clear();
    QDir featuresDir("output_features");
    if (!featuresDir.exists()) {
        QMessageBox::warning(this, "Warning", "output_features folder does not exist.");
        return;
    }
    QStringList featureFiles = featuresDir.entryList(QStringList() << "*.txt", QDir::Files);
    // Remove the .txt extension for display
    QStringList displayNames;
    for (const QString& file : featureFiles) {
        displayNames.append(QFileInfo(file).baseName());
    }
    featureComboBox->addItems(displayNames);
}

void UseFeatureWidget::refreshFeatures()
{
    loadFeatures();
}

void UseFeatureWidget::addFolder(const QString& folderPath)
{
    ResourceManager* rm = ResourceManager::instance();

    // Check if folder exists and has WAV files
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
    QMap<QString, FileWidget*> singleFiles = rm->getSingleFiles(ResourceManager::FileType::WavForSeparation);
    for (const QString& filePath : singleFiles.keys()) {
        if (filePath.startsWith(folderPath + "/")) {
            rm->removeFile(filePath);
            FileWidget* fw = singleFiles[filePath];
            fileLayout->removeWidget(fw);
            // Widget is deleted in ResourceManager
        }
    }

    // Delegate to ResourceManager
    FolderWidget* folderWidget = rm->addFolder(folderPath, fileContainer, ResourceManager::FileType::WavForSeparation);

    if (folderWidget) {
        // Add to layout if newly created
        if (fileLayout->indexOf(folderWidget) == -1) {
            fileLayout->addWidget(folderWidget);
        }

        // Handle removal connections
        connect(folderWidget, &FolderWidget::fileRemoved, this, [this, rm](const QString& path){
            rm->removeFile(path);
        });

        connect(folderWidget, &FolderWidget::folderRemoved, this, [this, rm, folderWidget](const QString& folderPath){
            rm->removeFolder(folderPath);
            fileLayout->removeWidget(folderWidget);
            // Widget is deleted in ResourceManager
        });

        connect(folderWidget, &FolderWidget::playRequested, this, [this](const QString& path){
            emit playRequested(path);
        });
    }
}

void UseFeatureWidget::addSingleFile(const QString& filePath)
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

    FileWidget* fileWidget = rm->addSingleFile(filePath, fileContainer, ResourceManager::FileType::WavForSeparation);
    if (fileWidget) {
        fileLayout->addWidget(fileWidget);

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

void UseFeatureWidget::onProcessClicked()
{
    QString selectedFeature = featureComboBox->currentText();
    if (selectedFeature.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a sound feature.");
        return;
    }

    // Gather selected files from folders and single files using ResourceManager
    QStringList filesToProcess;

    ResourceManager* rm = ResourceManager::instance();

    // From folders
    QMap<QString, FolderWidget*> folders = rm->getFolders(ResourceManager::FileType::WavForSeparation);
    for (FolderWidget* fw : folders.values()) {
        QStringList selected = fw->getSelectedFiles();
        for (const QString& path : selected) {
            filesToProcess.append(path);
        }
    }

    // From single files
    QMap<QString, FileWidget*> singles = rm->getSingleFiles(ResourceManager::FileType::WavForSeparation);
    for (FileWidget* fw : singles.values()) {
        if (fw->checkBox()->isChecked()) {
            filesToProcess.append(fw->filePath());
        }
    }

    if (filesToProcess.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select at least one file to process.");
        return;
    }

    // Clear previous results
    resultList->clear();

    // Process each file: separate and save chunks using the selected feature
for (const QString& file : filesToProcess) {
    QStringList separatedFiles = rm->processAndSaveSeparatedChunks(file, selectedFeature);
    for (const QString& separatedFile : separatedFiles) {
        resultList->addItem(separatedFile);
    }
}

}





void UseFeatureWidget::onPlayResult(const QString& filePath)
{
    emit playRequested(filePath);
}

void UseFeatureWidget::onDeleteClicked()
{
    QString selectedFeature = featureComboBox->currentText();
    if (selectedFeature.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No feature selected to delete.");
        return;
    }

    // Confirm deletion
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Feature",
                                  QString("Are you sure you want to delete the feature '%1'?").arg(selectedFeature),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    // Notify ResourceManager to remove the feature
    ResourceManager* rm = ResourceManager::instance();
    rm->removeFeature(selectedFeature);

    // Refresh the feature list
    refreshFeatures();
}
