#include "resourcemanager.h"
#include <torch/script.h>
#include <torch/torch.h>
#include "fileutils.h"
#include "constants.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include "htsatworker.h"
#include "separationworker.h"
#include <QMetaObject>
#include "filerepo.h"
#include "asyncprocessor.h"
#include "audioserializer.h"
#include "filelocker.h"
#include "logger.h"

// Thread management has been moved to AsyncProcessor

ResourceManager* ResourceManager::m_instance = nullptr;

/**
 * @brief Returns the singleton instance of ResourceManager.
 * @return Pointer to the ResourceManager instance.
 */
ResourceManager* ResourceManager::instance()
{
    if (!m_instance) {
        m_instance = new ResourceManager();
        m_instance->createOutputDirectories();
    }
    return m_instance;
}

ResourceManager::ResourceManager(QObject* parent)
    : QObject(parent),
      m_fileRepo(nullptr),
      m_asyncProcessor(nullptr),
      m_serializer(nullptr),
      m_fileLocker(nullptr)
{
    // Initialize components
    m_fileRepo = new FileRepo(this);
    m_serializer = new AudioSerializer();
    m_asyncProcessor = new AsyncProcessor(m_serializer, this);
    m_fileLocker = new FileLocker(this);

    // Initialize data structures
    m_fileTypeData[FileType::WavForFeature] = FileTypeData();
    m_fileTypeData[FileType::SoundFeature] = FileTypeData();
    m_fileTypeData[FileType::WavForSeparation] = FileTypeData();
    m_isProcessing = false;

    // Connect async processor signals
    connect(m_asyncProcessor, &AsyncProcessor::processingStarted, this, &ResourceManager::processingStarted);
    connect(m_asyncProcessor, &AsyncProcessor::processingProgress, this, &ResourceManager::processingProgress);
    connect(m_asyncProcessor, &AsyncProcessor::processingFinished, this, &ResourceManager::processingFinished);
    connect(m_asyncProcessor, &AsyncProcessor::separationProcessingFinished, this, &ResourceManager::separationProcessingFinished);
    connect(m_asyncProcessor, &AsyncProcessor::processingError, this, &ResourceManager::processingError);

    // Connect file locker signals
    connect(m_fileLocker, &FileLocker::fileLocked, this, &ResourceManager::fileLocked);
    connect(m_fileLocker, &FileLocker::fileUnlocked, this, &ResourceManager::fileUnlocked);
}

/**
 * @brief Creates output directories if they do not exist.
 */
void ResourceManager::createOutputDirectories()
{
    QStringList dirs = {
        Constants::OUTPUT_FEATURES_DIR,
        Constants::SEPARATED_RESULT_DIR,
    };

    for (const QString& dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (dir.mkpath(".")) {
                qDebug() << "Created directory:" << dirPath;
            } else {
                qDebug() << "Failed to create directory:" << dirPath;
            }
        }
    }
}


ResourceManager::~ResourceManager()
{
    // Cleanup components
    delete m_fileRepo;
    delete m_asyncProcessor;
    delete m_serializer;
    delete m_fileLocker;

    // Cleanup data structures (keep for backward compatibility)
    m_lockedFiles.clear();

    for (auto it = m_fileTypeData.begin(); it != m_fileTypeData.end(); ++it) {
        FileTypeData& data = it.value();
        for (auto widget : data.folders.values()) {
            delete widget;
        }
        for (auto widget : data.files.values()) {
            delete widget;
        }
        data.folders.clear();
        data.files.clear();
    }
    m_fileTypeData.clear();
}

/**
 * @brief Adds a folder and its valid files into the resource manager.
 * @param folderPath Path to the folder.
 * @param folderParent Parent widget for the folder UI element.
 * @param type FileType to categorize the files in this folder.
 * @return Pointer to the created FolderWidget, or nullptr if invalid.
 */
FolderWidget* ResourceManager::addFolder(const QString& folderPath, QWidget* folderParent, FileType type)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << "Folder does not exist:" << folderPath;
        return nullptr;
    }

    QStringList fileFilters;
    QString fileTypeDescription;
    if (type == FileType::SoundFeature) {
        fileFilters << "*.txt";
        fileTypeDescription = "sound feature";
    } else {
        fileFilters << "*.wav";
        fileTypeDescription = "WAV";
    }

    QStringList files = dir.entryList(fileFilters, QDir::Files);
    if (files.isEmpty()) {
        qDebug() << "No" << fileTypeDescription << "files found in folder:" << folderPath;
        return nullptr;
    }

    qDebug() << "Adding folder:" << folderPath << "with" << files.size() << fileTypeDescription << "files";

    FileTypeData& data = m_fileTypeData[type];
    QMap<QString, FolderWidget*>& folderMap = data.folders;
    QSet<QString>& pathSet = data.paths;

    FolderWidget* folderWidget = nullptr;
    if (folderMap.contains(folderPath)) {
        folderWidget = folderMap[folderPath];
    } else {
        folderWidget = new FolderWidget(folderPath, folderParent);
        folderMap.insert(folderPath, folderWidget);
        emit folderAdded(folderPath, type);
    }

    QStringList newFiles;
    for (const QString& f : files) {
        QString fullPath = dir.absoluteFilePath(f);
        if (!isDuplicate(fullPath, type)) {
            newFiles.append(f);
            pathSet.insert(fullPath);
            emit fileAdded(fullPath, type);
        }
    }

    if (!newFiles.isEmpty()) {
        folderWidget->appendFiles(newFiles);
    }

    return folderWidget;
}

/**
 * @brief Adds a single file to the resource manager.
 * @param filePath Absolute path to the file.
 * @param fileParent Parent widget for the file UI element.
 * @param type FileType to categorize the file.
 * @return Pointer to the created FileWidget, or nullptr if invalid.
 */
FileWidget* ResourceManager::addSingleFile(const QString& filePath, QWidget* fileParent, FileType type)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        qDebug() << "Invalid file:" << filePath;
        return nullptr;
    }

    QString expectedSuffix = (type == FileType::SoundFeature) ? "txt" : "wav";

    if (fi.suffix().toLower() != expectedSuffix) {
        qDebug() << "Invalid file type for" << static_cast<int>(type) << ":" << filePath;
        return nullptr;
    }

    if (isDuplicate(fi.absoluteFilePath(), type)) {
        qDebug() << "File already added:" << filePath;
        return nullptr;
    }

    qDebug() << "Adding single file:" << filePath;

    FileTypeData& data = m_fileTypeData[type];
    QMap<QString, FileWidget*>& fileMap = data.files;
    QSet<QString>& pathSet = data.paths;

    FileWidget* fileWidget = new FileWidget(filePath, fileParent);
    fileMap.insert(fi.absoluteFilePath(), fileWidget);
    pathSet.insert(fi.absoluteFilePath());
    emit fileAdded(fi.absoluteFilePath(), type);

    return fileWidget;
}

/**
 * @brief Removes a file from the resource manager.
 * @param filePath Absolute path to the file.
 * @param type FileType of the file to remove.
 */
void ResourceManager::removeFile(const QString& filePath, FileType type)
{
    FileTypeData& data = m_fileTypeData[type];
    QSet<QString>& pathSet = data.paths;
    QMap<QString, FileWidget*>& fileMap = data.files;

    if (pathSet.remove(filePath)) {
        if (fileMap.contains(filePath)) {
            FileWidget* fw = fileMap.take(filePath);
            fw->deleteLater();
        }
        emit fileRemoved(filePath, type);
    }
}

/**
 * @brief Removes a folder and its files from the resource manager.
 * @param folderPath Absolute path to the folder.
 * @param type FileType of the folder contents.
 */
void ResourceManager::removeFolder(const QString& folderPath, FileType type)
{
    FileTypeData& data = m_fileTypeData[type];
    QMap<QString, FolderWidget*>& folderMap = data.folders;
    QSet<QString>& pathSet = data.paths;

    if (folderMap.contains(folderPath)) {
        FolderWidget* fw = folderMap.take(folderPath);
        fw->deleteLater();

        QSet<QString> toRemove;
        for (const QString& filePath : pathSet) {
            if (filePath.startsWith(folderPath + "/")) {
                toRemove.insert(filePath);
            }
        }
        for (const QString& filePath : toRemove) {
            pathSet.remove(filePath);
            emit fileRemoved(filePath, type);
        }
        emit folderRemoved(folderPath, type);
    }
}

void ResourceManager::sortAll(Qt::SortOrder order)
{
    qDebug() << "sortAll called with order:" << (order == Qt::AscendingOrder ? "Ascending" : "Descending");
}

/**
 * @brief Attempts to set a file to read-only and lock it.
 * @param filePath Absolute path to the file.
 * @return True if successfully locked, false otherwise.
 */
bool ResourceManager::lockFile(const QString& filePath)
{
    if (!m_fileLocker) return false;
    return m_fileLocker->lockFile(filePath);
}

/**
 * @brief Unlocks a previously locked file by making it writable.
 * @param filePath Absolute path to the file.
 * @return True if successfully unlocked, false otherwise.
 */
bool ResourceManager::unlockFile(const QString& filePath)
{
    if (!m_fileLocker) return false;
    return m_fileLocker->unlockFile(filePath);
}

/**
 * @brief Checks if a file is currently locked.
 * @param filePath Absolute path to the file.
 * @return True if locked, false otherwise.
 */
bool ResourceManager::isFileLocked(const QString& filePath) const
{
    if (!m_fileLocker) return false;
    return m_fileLocker->isFileLocked(filePath);
}

/**
 * @brief Gets the set of added file paths for a given file type.
 * @param type FileType to query.
 * @return QSet of file paths.
 */
QSet<QString> ResourceManager::getAddedFiles(FileType type) const
{
    return m_fileTypeData.value(type).paths;
}

/**
 * @brief Gets the map of added folders for a given file type.
 * @param type FileType to query.
 * @return QMap of folder paths to FolderWidget pointers.
 */
QMap<QString, FolderWidget*> ResourceManager::getFolders(FileType type) const
{
    return m_fileTypeData.value(type).folders;
}

/**
 * @brief Gets the map of added single files for a given file type.
 * @param type FileType to query.
 * @return QMap of file paths to FileWidget pointers.
 */
QMap<QString, FileWidget*> ResourceManager::getSingleFiles(FileType type) const
{
    return m_fileTypeData.value(type).files;
}

bool ResourceManager::isDuplicate(const QString& path, FileType type) const
{
    return m_fileTypeData.value(type).paths.contains(path);
}



/**
 * @brief Starts audio feature generation process.
 * @param filePaths List of file paths to process.
 * @param outputFileName Base name for output feature file.
 */
void ResourceManager::startGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    if (!m_asyncProcessor) return;
    m_asyncProcessor->startFeatureGeneration(filePaths, outputFileName);
}

void ResourceManager::startSeparateAudio(const QStringList& filePaths, const QString& featureName)
{
    if (!m_asyncProcessor) return;
    m_asyncProcessor->startAudioSeparation(filePaths, featureName);
}

void ResourceManager::autoLoadSoundFeatures()
{
    qDebug() << "autoLoadSoundFeatures called";
}

/**
 * @brief Removes a saved sound feature by name.
 * @param featureName Base name of the feature to delete.
 */
void ResourceManager::removeFeature(const QString& featureName)
{
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        qDebug() << "Output features folder does not exist:" << outputFolder;
        return;
    }

    QStringList featureFiles = dir.entryList(QStringList() << "*.txt", QDir::Files);
    QString fileToDelete;
    for (const QString& file : featureFiles) {
        if (file.startsWith(featureName + "_") || file == featureName + ".txt") {
            fileToDelete = dir.absoluteFilePath(file);
            break;
        }
    }

    if (fileToDelete.isEmpty()) {
        qDebug() << "Feature file not found for:" << featureName;
        return;
    }

    QFile file(fileToDelete);
    if (file.remove()) {
        qDebug() << "Deleted feature file:" << fileToDelete;
        emit featuresUpdated();
    } else {
        qDebug() << "Failed to delete feature file:" << fileToDelete;
    }
}

/**
 * @brief Saves a waveform tensor as a WAV file.
 * @param waveform float32 tensor containing audio samples (flattened internally).
 * @param filePath Output file path.
 * @param sampleRate Sampling rate in Hz.
 * @return True if saving succeeded, false otherwise.
 */
bool ResourceManager::saveWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate)
{
    if (!m_serializer) return false;
    return m_serializer->saveWavToFile(waveform, filePath, sampleRate);
}

/**
 * @brief Saves an averaged embedding vector to a text file.
 * @param embedding Vector of floats.
 * @param outputFileName Base name for the output file.
 * @return Path to the saved file, or empty string if failed.
 */
QString ResourceManager::saveEmbedding(const std::vector<float>& embedding, const QString& outputFileName)
{
    QString filePath = createOutputFilePath(outputFileName);
    if (saveEmbeddingToFile(embedding, filePath)) {
        return filePath;
    } else {
        return QString();
    }
}

/**
 * @brief Creates a unique output file path in the features directory.
 * @param outputFileName Desired base name for the file.
 * @return Full path to the new output file.
 */
QString ResourceManager::createOutputFilePath(const QString& outputFileName)
{
    QString outputFolder = Constants::OUTPUT_FEATURES_DIR;
    QDir dir(outputFolder);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create output folder:" << outputFolder;
            return QString();
        }
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString baseName = QFileInfo(outputFileName).baseName();
    if (baseName.isEmpty()) {
        baseName = "output";
    }

    QString candidate = outputFolder + "/" + baseName + "_" + timestamp + ".txt";
    int counter = 1;
    while (QFile::exists(candidate)) {
        candidate = outputFolder + "/" + baseName + "_" + timestamp + "_" + QString::number(counter) + ".txt";
        counter++;
    }
    return candidate;
}

/**
 * @brief Saves an embedding vector to a specified text file.
 * @param embedding Vector of floats.
 * @param filePath Absolute path of the output file.
 * @return True if saving succeeded, false otherwise.
 */
bool ResourceManager::saveEmbeddingToFile(const std::vector<float>& embedding, const QString& filePath)
{
    if (!m_serializer) return false;
    return m_serializer->saveEmbeddingToFile(embedding, filePath);
}

/**
 * @brief Saves a separation result waveform as a WAV file.
 * @param waveform Tensor containing audio samples.
 * @param outputName Output file name.
 * @return True if saving succeeded, false otherwise.
 */
bool ResourceManager::saveSeparationResult(const torch::Tensor& waveform, const QString& outputName)
{
    return saveWav(waveform, outputName, 32000);
}

/**
 * @brief Checks if a file type is deletable.
 * @param type FileType to check.
 * @return True if deletable, false otherwise.
 */
bool ResourceManager::isDeletable(FileType type)
{
    switch (type) {
        case FileType::SoundFeature:
        case FileType::TempSegment:
        case FileType::SeparationResult:
            return true;
        default:
            return false;
    }
}

void ResourceManager::handleFinalResult(const QString& audioPath, const QString& featureName, const torch::Tensor& finalTensor)
{
    qDebug() << "Debug: handleFinalResult called for" << audioPath << "with feature" << featureName;
    qDebug() << "Debug: Final tensor shape:" << finalTensor.size(0) << "x" << (finalTensor.dim() > 1 ? finalTensor.size(1) : 1);

    QString outputName = QFileInfo(audioPath).baseName() + "_" + featureName + ".wav";
    QString outputPath = Constants::SEPARATED_RESULT_DIR + "/" + outputName;
    qDebug() << "Debug: Saving separation result to:" << outputPath;

    bool saveResult = saveWav(finalTensor, outputPath);
    if (saveResult) {
        qDebug() << "Debug: Successfully saved separation result:" << outputPath;
    } else {
        qDebug() << "Debug: Failed to save separation result:" << outputPath;
    }
}
