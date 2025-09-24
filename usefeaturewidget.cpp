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

    // Connect to ResourceManager featuresUpdated signal to auto-refresh feature list
    ResourceManager* rm = ResourceManager::instance();
    connect(rm, &ResourceManager::featuresUpdated, this, &UseFeatureWidget::refreshFeatures);

    // Connect processing signals once here
    connect(rm, &ResourceManager::processingProgress, this, &UseFeatureWidget::onProcessingProgress);
    connect(rm, &ResourceManager::processingFinished, this, &UseFeatureWidget::onProcessingFinished);
    connect(rm, &ResourceManager::separationProcessingFinished, this, &UseFeatureWidget::onSeparationProcessingFinished);
    connect(rm, &ResourceManager::processingError, this, &UseFeatureWidget::onProcessingError);

    // Connect to processingStarted to disable button when any worker starts
    connect(rm, &ResourceManager::processingStarted, this, [this]() {
        processButton->setEnabled(false);
    });
}

void UseFeatureWidget::setupUI()
{
    setupCommonUI(Constants::SELECT_WAV_FILES_TEXT, "Select Folder", "Select WAV Files");
    setupFeatureSelectionUI();
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
    // Access the main layout from the base class
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(this->QWidget::layout());
    if (!mainLayout) {
        qDebug() << "Error: Main layout is not a QVBoxLayout";
        return;
    }

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
 * @brief Sets up the processing UI components.
 *
 * Creates the process button, result label, and result list.
 */
void UseFeatureWidget::setupProcessingUI()
{
    // Access the main layout from the base class
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(this->QWidget::layout());
    if (!mainLayout) {
        qDebug() << "Error: Main layout is not a QVBoxLayout";
        return;
    }

    processButton = new QPushButton(Constants::PROCESS_BUTTON, this);
    mainLayout->addWidget(processButton);

    resultLabel = new QLabel(Constants::PROCESSED_FILES_LABEL, this);
    resultLabel->setVisible(false); // Hide initially - only show when there are results
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

    // Find the delete button from the feature layout
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(this->QWidget::layout());
    if (mainLayout) {
        QHBoxLayout* featureLayout = qobject_cast<QHBoxLayout*>(mainLayout->itemAt(5)->layout());
        if (featureLayout) {
            QPushButton* deleteButton = qobject_cast<QPushButton*>(featureLayout->itemAt(1)->widget());
            if (deleteButton) {
                connect(deleteButton, &QPushButton::clicked, this, &UseFeatureWidget::onDeleteClicked);
            }
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
    QMap<QString, FileWidget*> singleFiles = rm->getSingleFiles(fileType());
    for (const QString& filePath : singleFiles.keys()) {
        if (filePath.startsWith(folderPath + "/")) {
            rm->removeFile(filePath, fileType());
            FileWidget* fw = singleFiles[filePath];
            singleFileLayout->removeWidget(fw);
            // Widget is deleted in ResourceManager
        }
    }

    // Delegate to ResourceManager
    FolderWidget* folderWidget = rm->addFolder(folderPath, folderContainer, fileType());

    if (folderWidget) {
        // Add to layout if newly created
        if (folderLayout->indexOf(folderWidget) == -1) {
            folderLayout->addWidget(folderWidget);
        }

        // Handle removal connections
        connect(folderWidget, &FolderWidget::fileRemoved, this, [this, rm](const QString& path){
            rm->removeFile(path, fileType());
        });

        connect(folderWidget, &FolderWidget::folderRemoved, this, [this, rm, folderWidget](const QString& folderPath){
            rm->removeFolder(folderPath, fileType());
            folderLayout->removeWidget(folderWidget);
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

    FileWidget* fileWidget = rm->addSingleFile(filePath, singleFileContainer, fileType());
    if (fileWidget) {
        singleFileLayout->addWidget(fileWidget);

        connect(fileWidget, &FileWidget::fileRemoved,
                this, [this, rm](const QString& path){
                    rm->removeFile(path, fileType());
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
    QMap<QString, FolderWidget*> folders = rm->getFolders(fileType());
    for (FolderWidget* fw : folders.values()) {
        QStringList selected = fw->getSelectedFiles();
        for (const QString& path : selected) {
            filesToProcess.append(path);
        }
    }

    // From single files
    QMap<QString, FileWidget*> singles = rm->getSingleFiles(fileType());
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

    // Disable process button during processing
    processButton->setEnabled(false);

    // Start async processing
    rm->startSeparateAudio(filesToProcess, selectedFeature);
}

void UseFeatureWidget::onProcessingProgress(int value)
{
    // Don't show progress percentage text - user doesn't want it
    // Progress is shown in the main window progress bar instead
}

void UseFeatureWidget::onProcessingFinished(const QStringList& results)
{
    // This slot handles create feature processing finished
    // We do not add create feature results to resultList as per user request

    // Re-enable process button
    processButton->setEnabled(true);

    // Refresh feature list or results
    loadFeatures();

    resultLabel->setVisible(false);
}

void UseFeatureWidget::onSeparationProcessingFinished(const QStringList& results)
{
    // This slot handles separation processing finished
    // Add separation results to the resultList

    for (const QString& result : results) {
        resultList->addItem(result);
    }

    // Re-enable process button
    processButton->setEnabled(true);

    // Make the process indicator disappear
    resultLabel->setVisible(false);

    // Ensure progress bar is hidden when separation is complete
    // The main window will handle hiding the global progress bar via processingFinished signal
}

void UseFeatureWidget::onProcessingError(const QString& error)
{
    // Re-enable process button on error
    processButton->setEnabled(true);

    // Keep the error visible temporarily
    resultLabel->setText("Processing error: " + error);
    resultLabel->setVisible(true);
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
