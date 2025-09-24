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

static QThread* htsatThread = nullptr;
static HTSATWorker* htsatWorker = nullptr;
static QThread* separationThread = nullptr;
static SeparationWorker* separationWorker = nullptr;

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
    : QObject(parent)
{
    m_fileTypeData[FileType::WavForFeature] = FileTypeData();
    m_fileTypeData[FileType::SoundFeature] = FileTypeData();
    m_fileTypeData[FileType::WavForSeparation] = FileTypeData();

    m_isProcessing = false;


    htsatThread = new QThread(this);
    htsatWorker = new HTSATWorker();
    htsatWorker->moveToThread(htsatThread);

    connect(htsatThread, &QThread::finished, htsatWorker, &QObject::deleteLater);
    connect(this, &ResourceManager::startHTSATProcessing, htsatWorker, &HTSATWorker::generateFeatures);
    connect(htsatWorker, &HTSATWorker::progressUpdated, this, &ResourceManager::processingProgress);
    connect(htsatWorker, &HTSATWorker::finished, this, [this](const std::vector<float>& avgEmb, const QString& outputFileName){
        QString filePath = saveEmbedding(avgEmb, outputFileName);
        m_isProcessing = false;
        emit processingFinished(QStringList() << filePath);
        emit featuresUpdated();
    });
    connect(htsatWorker, &HTSATWorker::error, this, [this](const QString& error){
        m_isProcessing = false;
        emit processingError(error);
    });

    htsatThread->start();

    separationThread = new QThread(this);
    separationWorker = new SeparationWorker();
    separationWorker->moveToThread(separationThread);

    connect(separationThread, &QThread::finished, separationWorker, &QObject::deleteLater);
    connect(this, &ResourceManager::startSeparationProcessing, separationWorker, &SeparationWorker::processFile);
    connect(separationWorker, &SeparationWorker::progressUpdated, this, &ResourceManager::processingProgress);
    connect(separationWorker, &SeparationWorker::chunkReady, this, &ResourceManager::handleChunk);
    connect(separationWorker, &SeparationWorker::separationFinished, this, &ResourceManager::handleFinalResult);
    connect(separationWorker, &SeparationWorker::deleteChunkFile, this, [this](const QString& path) {
        removeFile(path, FileType::TempSegment);
    });

    connect(separationWorker, &SeparationWorker::separationFinished, this, [this](const QString& audioPath, const QString& featureName, const torch::Tensor&) {
        m_isProcessing = false;
        QStringList results;
        QString outputName = QFileInfo(audioPath).baseName() + "_" + featureName + ".wav";
        QString outputPath = Constants::SEPARATED_RESULT_DIR + "/" + outputName;
        results << outputPath;
        emit separationProcessingFinished(results);
});

connect(separationWorker, &SeparationWorker::error, this, [this](const QString& error){
    m_isProcessing = false;
    emit processingError(error);
});

    separationThread->start();
    
}

/**
 * @brief Creates output directories if they do not exist.
 */
void ResourceManager::createOutputDirectories()
{
    QStringList dirs = {
        Constants::OUTPUT_FEATURES_DIR,
        Constants::SEPARATED_RESULT_DIR,
        Constants::TEMP_SEGMENTS_DIR
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
    for (const QString& filePath : m_lockedFiles) {
        unlockFile(filePath);
    }
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

    if (htsatThread) {
        htsatThread->quit();
        htsatThread->wait();
        htsatThread = nullptr;
        htsatWorker = nullptr;
    }

    if (separationThread) {
        separationThread->quit();
        separationThread->wait();
        separationThread = nullptr;
        separationWorker = nullptr;
    }
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
        emitFolderAdded(folderPath, type);
    }

    QStringList newFiles;
    for (const QString& f : files) {
        QString fullPath = dir.absoluteFilePath(f);
        if (!isDuplicate(fullPath, type)) {
            newFiles.append(f);
            pathSet.insert(fullPath);
            emitFileAdded(fullPath, type);
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
    emitFileAdded(fi.absoluteFilePath(), type);

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
        emitFileRemoved(filePath, type);
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
            emitFileRemoved(filePath, type);
        }
        emitFolderRemoved(folderPath, type);
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
    if (FileUtils::setFileReadOnly(filePath, true) == FileUtils::FileOperationResult::Success) {
        m_lockedFiles.insert(filePath);
        emit fileLocked(filePath);
        return true;
    }
    return false;
}

/**
 * @brief Unlocks a previously locked file by making it writable.
 * @param filePath Absolute path to the file.
 * @return True if successfully unlocked, false otherwise.
 */
bool ResourceManager::unlockFile(const QString& filePath)
{
    if (FileUtils::setFileReadOnly(filePath, false) == FileUtils::FileOperationResult::Success) {
        m_lockedFiles.remove(filePath);
        emit fileUnlocked(filePath);
        return true;
    }
    return false;
}

/**
 * @brief Checks if a file is currently locked.
 * @param filePath Absolute path to the file.
 * @return True if locked, false otherwise.
 */
bool ResourceManager::isFileLocked(const QString& filePath) const
{
    return m_lockedFiles.contains(filePath);
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

void ResourceManager::emitFileAdded(const QString& path, FileType type)
{
    emit fileAdded(path, type);
}

void ResourceManager::emitFileRemoved(const QString& path, FileType type)
{
    emit fileRemoved(path, type);
}

void ResourceManager::emitFolderAdded(const QString& folderPath, FileType type)
{
    emit folderAdded(folderPath, type);
}

void ResourceManager::emitFolderRemoved(const QString& folderPath, FileType type)
{
    emit folderRemoved(folderPath, type);
}

/**
 * @brief Starts audio feature generation process.
 * @param filePaths List of file paths to process.
 * @param outputFileName Base name for output feature file.
 */
void ResourceManager::startGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    if (m_isProcessing) return;
    m_isProcessing = true;
    emit processingStarted();
    emit startHTSATProcessing(filePaths, outputFileName);
}

void ResourceManager::startSeparateAudio(const QStringList& filePaths, const QString& outputFileName)
{
    if (m_isProcessing) return;
    m_isProcessing = true;
    emit processingStarted();
    emit startSeparationProcessing(filePaths, outputFileName);
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
    qDebug() << "Debug: saveWav called for" << filePath;
    qDebug() << "Debug: Input tensor shape:" << waveform.size(0) << "x" << (waveform.dim() > 1 ? waveform.size(1) : 1) << "x" << (waveform.dim() > 2 ? waveform.size(2) : 1);
    qDebug() << "Debug: Input tensor dimensions:" << waveform.dim();
    qDebug() << "Debug: Input tensor numel:" << waveform.numel();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Debug: Failed to open file for writing:" << filePath;
        return false;
    }

    torch::Tensor audio2d;
    if (waveform.dim() == 1) {
        qDebug() << "Debug: Converting 1D tensor to 2D";
        audio2d = waveform.unsqueeze(1);
    } else if (waveform.dim() == 2) {
        qDebug() << "Debug: Using 2D tensor as is";
        audio2d = waveform;
    } else if (waveform.dim() == 3 && waveform.size(0) == 1 && waveform.size(2) == 1) {
        qDebug() << "Debug: Converting 3D tensor (1, frames, 1) to 2D";
        qDebug() << "Debug: Before squeeze - shape:" << waveform.size(0) << "x" << waveform.size(1) << "x" << waveform.size(2);
        torch::Tensor afterFirstSqueeze = waveform.squeeze(0);
        qDebug() << "Debug: After squeeze(0) - shape:" << afterFirstSqueeze.size(0) << "x" << afterFirstSqueeze.size(1);
        // For (frames, 1) tensor, we don't need squeeze(2), just unsqueeze to make it (frames, 1)
        audio2d = afterFirstSqueeze;
        qDebug() << "Debug: Using (frames, 1) tensor as 2D - shape:" << audio2d.size(0) << "x" << audio2d.size(1);
    } else {
        qDebug() << "Debug: Unsupported tensor shape for saveWav - dim:" << waveform.dim() << "shape:" << waveform.size(0) << "x" << (waveform.dim() > 1 ? waveform.size(1) : 1) << "x" << (waveform.dim() > 2 ? waveform.size(2) : 1);
        return false;
    }

    qDebug() << "Debug: Final audio2d shape:" << audio2d.size(0) << "x" << audio2d.size(1);
    qDebug() << "Debug: Final audio2d numel:" << audio2d.numel();

    short channels = static_cast<short>(audio2d.size(1));
    int64_t numSamples = audio2d.numel();
    float* data = audio2d.data_ptr<float>();

    qDebug() << "Debug: Writing WAV with channels:" << channels << "numSamples:" << numSamples;

    file.write("RIFF", 4);
    int fileSizePos = file.pos();
    file.write("\x00\x00\x00\x00", 4);
    file.write("WAVE", 4);

    file.write("fmt ", 4);
    int fmtSize = 16;
    file.write((char*)&fmtSize, 4);
    short format = 3;
    file.write((char*)&format, 2);
    file.write((char*)&channels, 2);
    file.write((char*)&sampleRate, 4);
    int byteRate = sampleRate * channels * 4;
    file.write((char*)&byteRate, 4);
    short blockAlign = channels * 4;
    file.write((char*)&blockAlign, 2);
    short bitsPerSample = 32;
    file.write((char*)&bitsPerSample, 2);

    file.write("data", 4);
    int dataSize = static_cast<int>(numSamples * 4);
    file.write((char*)&dataSize, 4);
    file.write((char*)data, dataSize);

    int totalSize = 36 + dataSize;
    file.seek(fileSizePos);
    file.write((char*)&totalSize, 4);

    file.close();
    return true;
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
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open output file:" << filePath;
        return false;
    }

    QTextStream out(&file);
    for (size_t i = 0; i < embedding.size(); ++i) {
        out << embedding[i];
        if (i < embedding.size() - 1) out << " ";
    }
    out << "\n";
    file.close();

    qDebug() << "Averaged embedding saved to:" << filePath;
    return true;
}

/**
 * @brief Creates a temporary file path for a chunk or segment.
 * @param baseName Base name for the file.
 * @param index Index of the chunk.
 * @param type FileType to categorize the temp file.
 * @return Full path to the temporary file.
 */
QString ResourceManager::createTempFilePath(const QString& baseName, int index, FileType type)
{
    QString tempFolder = Constants::TEMP_SEGMENTS_DIR;
    QDir dir(tempFolder);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString suffix = "wav";
    QString fileName = QString("%1_chunk_%2.%3").arg(baseName).arg(index).arg(suffix);
    return dir.absoluteFilePath(fileName);
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

void ResourceManager::handleChunk(const QString& chunkFilePath, const QString& featureName, const torch::Tensor& chunkData)
{
    saveWav(chunkData, chunkFilePath);
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
