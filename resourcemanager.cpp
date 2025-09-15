#include "resourcemanager.h"
#include <torch/script.h>
#include <torch/torch.h>
#include "fileutils.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include "htsatworker.h"
#include "separationworker.h"

static QThread* htsatThread = nullptr;
static HTSATWorker* htsatWorker = nullptr;
static QThread* separationThread = nullptr;
static SeparationWorker* separationWorker = nullptr;

// Singleton instance
ResourceManager* ResourceManager::m_instance = nullptr;

/**
 * @brief Gets the singleton instance of ResourceManager.
 * @return Pointer to the ResourceManager instance.
 */
ResourceManager* ResourceManager::instance()
{
    if (!m_instance) {
        m_instance = new ResourceManager();
    }
    return m_instance;
}

/**
 * @brief Private constructor for singleton pattern.
 * @param parent The parent QObject.
 */
ResourceManager::ResourceManager(QObject* parent)
    : QObject(parent)
{
    m_htsatProcessor = new HTSATProcessor(this);
    m_zeroShotAspProcessor = new ZeroShotASPFeatureExtractor(this);

    // Initialize FileTypeData for each file type
    m_fileTypeData[FileType::WavForFeature] = FileTypeData();
    m_fileTypeData[FileType::SoundFeature] = FileTypeData();
    m_fileTypeData[FileType::WavForSeparation] = FileTypeData();

    m_isProcessing = false;

    // Setup HTSAT worker thread
    htsatThread = new QThread(this);
    htsatWorker = new HTSATWorker();
    htsatWorker->moveToThread(htsatThread);

    connect(htsatThread, &QThread::finished, htsatWorker, &QObject::deleteLater);
    connect(this, &ResourceManager::startHTSATProcessing, htsatWorker, &HTSATWorker::generateFeatures);
    connect(htsatWorker, &HTSATWorker::progressUpdated, this, &ResourceManager::processingProgress);
    connect(htsatWorker, &HTSATWorker::finished, this, [this](const QString& resultFile){
        m_isProcessing = false;
        emit processingFinished(QStringList() << resultFile);
    });
    connect(htsatWorker, &HTSATWorker::error, this, [this](const QString& error){
        m_isProcessing = false;
        emit processingError(error);
    });

    htsatThread->start();

    // Setup separation worker thread
    separationThread = new QThread(this);
    separationWorker = new SeparationWorker();
    separationWorker->moveToThread(separationThread);

    connect(separationThread, &QThread::finished, separationWorker, &QObject::deleteLater);
    connect(this, &ResourceManager::startSeparationProcessing, separationWorker, &SeparationWorker::processFile);
    connect(separationWorker, &SeparationWorker::progressUpdated, this, &ResourceManager::processingProgress);
    connect(separationWorker, &SeparationWorker::finished, this, [this](const QStringList& results){
        m_isProcessing = false;
        emit processingFinished(results);
    });
    connect(separationWorker, &SeparationWorker::error, this, [this](const QString& error){
        m_isProcessing = false;
        emit processingError(error);
    });

    separationThread->start();
}

/**
 * @brief Destructor for ResourceManager.
 */
ResourceManager::~ResourceManager()
{
    // Unlock all locked files before cleanup
    for (const QString& filePath : m_lockedFiles) {
        unlockFile(filePath);
    }
    m_lockedFiles.clear();

    // Cleanup widgets for all file types
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
 * @brief Adds a folder and its WAV files to the resource manager.
 * @param folderPath The path to the folder.
 * @param folderParent The parent widget for FolderWidget.
 * @param type The file type (currently only WavForFeature supported).
 */
FolderWidget* ResourceManager::addFolder(const QString& folderPath, QWidget* folderParent, FileType type)
{
    if (type != FileType::WavForFeature && type != FileType::WavForSeparation && type != FileType::SoundFeature) {
        qDebug() << "FileType not supported:" << static_cast<int>(type);
        return nullptr;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << "Folder does not exist:" << folderPath;
        return nullptr;
    }

    QStringList fileFilters;
    QString fileTypeDescription;
    if (type == FileType::WavForFeature || type == FileType::WavForSeparation) {
        fileFilters << "*.wav";
        fileTypeDescription = "WAV";
    } else if (type == FileType::SoundFeature) {
        fileFilters << "*.txt";
        fileTypeDescription = "sound feature";
    }

    QStringList files = dir.entryList(fileFilters, QDir::Files);
    if (files.isEmpty()) {
        qDebug() << "No" << fileTypeDescription << "files found in folder:" << folderPath;
        return nullptr;
    }

    qDebug() << "Adding folder:" << folderPath << "with" << files.size() << fileTypeDescription << "files";

    // Get the appropriate data for the type
    FileTypeData& data = m_fileTypeData[type];
    QMap<QString, FolderWidget*>& folderMap = data.folders;
    QSet<QString>& pathSet = data.paths;

    // Create FolderWidget if not exists
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
 * @brief Adds a single WAV file to the resource manager.
 * @param filePath The path to the WAV file.
 * @param fileParent The parent widget for FileWidget.
 * @param type The file type (currently only WavForFeature supported).
 */
FileWidget* ResourceManager::addSingleFile(const QString& filePath, QWidget* fileParent, FileType type)
{
    if (type != FileType::WavForFeature && type != FileType::WavForSeparation && type != FileType::SoundFeature) {
        qDebug() << "FileType not supported:" << static_cast<int>(type);
        return nullptr;
    }

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        qDebug() << "Invalid file:" << filePath;
        return nullptr;
    }

    QString expectedSuffix;
    if (type == FileType::WavForFeature || type == FileType::WavForSeparation) {
        expectedSuffix = "wav";
    } else if (type == FileType::SoundFeature) {
        expectedSuffix = "txt";
    }

    if (fi.suffix().toLower() != expectedSuffix) {
        qDebug() << "Invalid file type for" << static_cast<int>(type) << ":" << filePath;
        return nullptr;
    }

    if (isDuplicate(fi.absoluteFilePath(), type)) {
        qDebug() << "File already added:" << filePath;
        return nullptr;
    }

    qDebug() << "Adding single file:" << filePath;

    // Get the appropriate data for the type
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
 * @param filePath The path to the file to remove.
 * @param type The file type.
 */
void ResourceManager::removeFile(const QString& filePath, FileType type)
{
    // Get the appropriate data for the type
    FileTypeData& data = m_fileTypeData[type];
    QSet<QString>& pathSet = data.paths;
    QMap<QString, FileWidget*>& fileMap = data.files;

    if (pathSet.remove(filePath)) {
        // Remove from single files if present
        if (fileMap.contains(filePath)) {
            FileWidget* fw = fileMap.take(filePath);
            fw->deleteLater();
        }
        // Note: For folder files, removal is handled by FolderWidget
        emitFileRemoved(filePath, type);
    }
}

/**
 * @brief Removes a folder from the resource manager.
 * @param folderPath The path to the folder to remove.
 * @param type The file type.
 */
void ResourceManager::removeFolder(const QString& folderPath, FileType type)
{
    // Get the appropriate data for the type
    FileTypeData& data = m_fileTypeData[type];
    QMap<QString, FolderWidget*>& folderMap = data.folders;
    QSet<QString>& pathSet = data.paths;

    if (folderMap.contains(folderPath)) {
        FolderWidget* fw = folderMap.take(folderPath);
        fw->deleteLater();

        // Remove all files in the folder from pathSet
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

/**
 * @brief Sorts all folders and single files.
 * @param order The sort order (ascending or descending).
 */
void ResourceManager::sortAll(Qt::SortOrder order)
{
    // This is a placeholder - actual sorting of layouts is done in AddSoundFeatureWidget
    // Here we can sort the maps if needed, but since QMap is ordered by key,
    // and keys are paths, sorting by path might be sufficient for now.
    qDebug() << "sortAll called with order:" << (order == Qt::AscendingOrder ? "Ascending" : "Descending");

    // Example: sort folders and files by keys (paths)
    for (auto& data : m_fileTypeData) {
        // QMap is already sorted by keys, so no action needed
        Q_UNUSED(data);
    }
}

/**
 * @brief Locks a file by setting it to read-only.
 * @param filePath The path to the file.
 * @return True if successful, false otherwise.
 */
bool ResourceManager::lockFile(const QString& filePath)
{
    if (FileUtils::setFileReadOnly(filePath, true)) {
        m_lockedFiles.insert(filePath);
        emit fileLocked(filePath);
        return true;
    }
    return false;
}

/**
 * @brief Unlocks a file by removing read-only attribute.
 * @param filePath The path to the file.
 * @return True if successful, false otherwise.
 */
bool ResourceManager::unlockFile(const QString& filePath)
{
    if (FileUtils::setFileReadOnly(filePath, false)) {
        m_lockedFiles.remove(filePath);
        emit fileUnlocked(filePath);
        return true;
    }
    return false;
}

/**
 * @brief Checks if a file is locked.
 * @param filePath The path to the file.
 * @return True if locked, false otherwise.
 */
bool ResourceManager::isFileLocked(const QString& filePath) const
{
    return m_lockedFiles.contains(filePath);
}

/**
 * @brief Gets the set of added files for a given type.
 * @param type The file type.
 * @return Set of file paths.
 */
QSet<QString> ResourceManager::getAddedFiles(FileType type) const
{
    return m_fileTypeData.value(type).paths;
}

/**
 * @brief Gets the map of folder widgets for a given type.
 * @param type The file type.
 * @return Map of folder paths to FolderWidget pointers.
 */
QMap<QString, FolderWidget*> ResourceManager::getFolders(FileType type) const
{
    return m_fileTypeData.value(type).folders;
}

/**
 * @brief Gets the map of single file widgets for a given type.
 * @param type The file type.
 * @return Map of file paths to FileWidget pointers.
 */
QMap<QString, FileWidget*> ResourceManager::getSingleFiles(FileType type) const
{
    return m_fileTypeData.value(type).files;
}

/**
 * @brief Checks if a path is a duplicate for the given type.
 * @param path The file path.
 * @param type The file type.
 * @return True if duplicate, false otherwise.
 */
bool ResourceManager::isDuplicate(const QString& path, FileType type) const
{
    return m_fileTypeData.value(type).paths.contains(path);
}

/**
 * @brief Emits the fileAdded signal.
 * @param path The file path.
 * @param type The file type.
 */
void ResourceManager::emitFileAdded(const QString& path, FileType type)
{
    emit fileAdded(path, type);
}

/**
 * @brief Emits the fileRemoved signal.
 * @param path The file path.
 * @param type The file type.
 */
void ResourceManager::emitFileRemoved(const QString& path, FileType type)
{
    emit fileRemoved(path, type);
}

/**
 * @brief Emits the folderAdded signal.
 * @param folderPath The folder path.
 * @param type The file type.
 */
void ResourceManager::emitFolderAdded(const QString& folderPath, FileType type)
{
    emit folderAdded(folderPath, type);
}

/**
 * @brief Emits the folderRemoved signal.
 * @param folderPath The folder path.
 * @param type The file type.
 */
void ResourceManager::emitFolderRemoved(const QString& folderPath, FileType type)
{
    emit folderRemoved(folderPath, type);
}

/**
 * @brief Loads the HTSAT model from the specified path.
 * @param modelPath Path to the TorchScript model file.
 * @return True if loading succeeded, false otherwise.
 */
bool ResourceManager::loadModel(const QString& modelPath)
{
    return m_htsatProcessor->loadModel(modelPath);
}

bool ResourceManager::loadZeroShotASPModel(const QString& modelPath)
{
    return m_zeroShotAspProcessor->loadModel(modelPath);
}

torch::Tensor ResourceManager::separateAudio(const torch::Tensor& waveform, const torch::Tensor& condition)
{
    if (!m_zeroShotAspProcessor->isModelLoaded()) {
        qWarning() << "ZeroShotASP model not loaded.";
        return torch::Tensor();
    }
    // Provide an output path for WAV file saving if needed, here empty to keep old behavior
    return m_zeroShotAspProcessor->separateAudio(waveform, condition, QString());
}

/**
 * @brief Processes a list of audio files to generate embeddings and saves them to a file.
 * @param filePaths List of audio file paths.
 * @param outputFileName Path to the output file.
 */
void ResourceManager::generateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    // Load model if not loaded
    if (!m_htsatProcessor->isModelLoaded()) {
        QString modelPath = "/models/htsat_embedding_model.pt";
        qDebug() << "Current working directory:" << QDir::currentPath();
        qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();
        qDebug() << "Model path:" << modelPath;
        QFileInfo fi(modelPath);
        qDebug() << "Absolute model path:" << fi.absoluteFilePath();
        if (!loadModel(modelPath)) {
            qDebug() << "Failed to load HTSAT model from:" << modelPath;
            return;
        }
    }

    QVector<std::vector<float>> embeddings;
    int totalFiles = filePaths.size();
    for (int i = 0; i < totalFiles; ++i) {
        const QString& filePath = filePaths[i];
        std::vector<float> embedding = m_htsatProcessor->processAudio(filePath);
        if (embedding.empty()) {
            qDebug() << "Failed to process file:" << filePath;
            continue;
        }
        embeddings.append(embedding);
        int progress = (i + 1) * 100 / totalFiles;
        emit progressUpdated(progress);
    }

    if (embeddings.isEmpty()) {
        qDebug() << "No embeddings generated.";
        return;
    }

    // Define output folder
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create output folder:" << outputFolder;
            return;
        }
    }

    // Generate timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    // Construct base name
    QString baseName = QFileInfo(outputFileName).baseName();
    if (baseName.isEmpty()) {
        baseName = "output";
    }

    // Construct full output file path with timestamp
    QString finalOutputFileName = outputFolder + "/" + baseName + "_" + timestamp + ".txt";

    // Compute average embedding
    std::vector<float> avg_emb(embeddings[0].size(), 0.0f);
    for (const auto& emb : embeddings) {
        for (size_t i = 0; i < emb.size(); ++i) {
            avg_emb[i] += emb[i];
        }
    }
    for (size_t i = 0; i < avg_emb.size(); ++i) {
        avg_emb[i] /= embeddings.size();
    }

    // Save averaged embedding to file
    QFile file(finalOutputFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open output file:" << finalOutputFileName;
        return;
    }

    QTextStream out(&file);
    for (size_t i = 0; i < avg_emb.size(); ++i) {
        out << avg_emb[i];
        if (i < avg_emb.size() - 1) out << " ";
    }
    out << "\n";
    file.close();

    qDebug() << "Averaged embedding saved to:" << finalOutputFileName;

    emit featuresUpdated(); // Emit signal to notify features updated
}

/**
 * @brief Starts asynchronous generation of audio features.
 * @param filePaths List of audio file paths.
 * @param outputFileName Path to the output file.
 */
void ResourceManager::startGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    if (m_isProcessing) return;
    m_isProcessing = true;
    emit processingStarted();
    emit startHTSATProcessing(filePaths, outputFileName);
}

/**
 * @brief Automatically loads sound features from the output_features folder.
 */
void ResourceManager::autoLoadSoundFeatures()
{
    // This method can be used to automatically load features if needed
    // For now, it's a placeholder
    qDebug() << "autoLoadSoundFeatures called";
}

/**
 * @brief Removes a sound feature by name.
 * @param featureName The name of the feature to remove (without .txt extension).
 */
void ResourceManager::removeFeature(const QString& featureName)
{
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        qDebug() << "Output features folder does not exist:" << outputFolder;
        return;
    }

    // Find the file that matches the feature name (may have timestamp)
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
        emit featuresUpdated(); // Notify that features have been updated
    } else {
        qDebug() << "Failed to delete feature file:" << fileToDelete;
    }
}

/**
 * @brief Splits an audio file into chunks and saves each chunk as a WAV file.
 * @param audioPath Path to the input audio file.
 * @return List of paths to the saved chunk WAV files.
 */
QStringList ResourceManager::splitAndSaveWavChunks(const QString& audioPath)
{
    QStringList tempFiles;
    std::vector<float> audioData = readAndResampleAudio(audioPath);
    if (audioData.empty()) {
        return tempFiles;
    }

    const int64_t chunkSize = 320000;  // 10 seconds at 32kHz
    QDir tempDir("temp_chunks");
    if (!tempDir.exists()) {
        tempDir.mkpath(".");
    }

    QFileInfo fi(audioPath);
    QString baseName = fi.baseName();
    int chunkIndex = 0;

    for (size_t start = 0; start < audioData.size(); start += chunkSize) {
        size_t end = std::min(start + chunkSize, audioData.size());
        std::vector<float> chunk(audioData.begin() + start, audioData.begin() + end);

        // Pad the last chunk with zeros to make it the same size
        if (chunk.size() < chunkSize) {
            chunk.resize(chunkSize, 0.0f);
        }

        // Convert to tensor and save as WAV
        torch::Tensor tensor = torch::from_blob(chunk.data(), {chunkSize}, torch::kFloat32).clone();
        QString tempFile = QString("temp_chunks/%1_chunk_%2.wav").arg(baseName).arg(chunkIndex++);

        if (saveWav(tensor, tempFile, 32000)) {
            tempFiles.append(tempFile);
        }
    }

    return tempFiles;
}

/**
 * @brief Processes an audio file with a feature and saves the separated audio.
 * @param audioPath Path to the input audio file.
 * @param featureName Name of the feature to use for separation.
 * @return List of paths to the saved separated audio files (one per input file).
 */
QStringList ResourceManager::processAndSaveSeparatedChunks(const QString& audioPath, const QString& featureName)
{
    QStringList separatedFiles;

    // Load ZeroShotASP model if not loaded
    if (!m_zeroShotAspProcessor->isModelLoaded()) {
        QString modelPath = "/models/zero_shot_asp_separation_model.pt";
        if (!loadZeroShotASPModel(modelPath)) {
            qWarning() << "Failed to load ZeroShotASP model from:" << modelPath;
            return separatedFiles;
        }
    }

    // Find the feature file
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        qWarning() << "Output features folder does not exist:" << outputFolder;
        return separatedFiles;
    }

    QStringList featureFiles = dir.entryList(QStringList() << "*.txt", QDir::Files);
    QString featureFile;
    for (const QString& file : featureFiles) {
        if (file.startsWith(featureName + "_") || file == featureName + ".txt") {
            featureFile = dir.absoluteFilePath(file);
            break;
        }
    }

    if (featureFile.isEmpty()) {
        qWarning() << "Feature file not found for:" << featureName;
        return separatedFiles;
    }

    // Load the embedding
    QFile file(featureFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open feature file:" << featureFile;
        return separatedFiles;
    }

    QTextStream in(&file);
    QString line = in.readLine();
    QStringList parts = line.split(" ", Qt::SkipEmptyParts);
    std::vector<float> embedding;
    for (const QString& part : parts) {
        embedding.push_back(part.toFloat());
    }
    file.close();

    if (embedding.size() != 2048) {
        qWarning() << "Invalid embedding size:" << embedding.size() << "expected 2048";
        return separatedFiles;
    }

    // Read and resample audio to 32kHz mono
    std::vector<float> audioData = readAndResampleAudio(audioPath);
    if (audioData.empty()) {
        return separatedFiles;
    }

    const int64_t chunkSize = 320000;  // 10 seconds at 32kHz
    QFileInfo fi(audioPath);
    QString baseName = fi.baseName();

    // Collect separated chunks
    std::vector<torch::Tensor> separatedChunks;

    for (size_t start = 0; start < audioData.size(); start += chunkSize) {
        size_t end = std::min(start + chunkSize, audioData.size());
        std::vector<float> chunk(audioData.begin() + start, audioData.begin() + end);
        size_t originalLength = chunk.size();

        // Pad the chunk with zeros to make it chunkSize
        if (chunk.size() < chunkSize) {
            chunk.resize(chunkSize, 0.0f);
        }

        // Convert to tensor and add batch dimension
        torch::Tensor waveform = torch::from_blob(chunk.data(), {1, chunkSize, 1}, torch::kFloat32).clone();
        torch::Tensor condition = torch::from_blob(embedding.data(), {1, 2048}, torch::kFloat32).clone();

        // Separate audio
        torch::Tensor separated = separateAudio(waveform, condition);
        if (separated.numel() == 0) {
            qWarning() << "Separation failed for chunk starting at" << start;
            continue;
        }

        // Trim to original length
        torch::Tensor separatedTrimmed = separated.squeeze(0).squeeze(1).slice(0, 0, originalLength);
        separatedChunks.push_back(separatedTrimmed);
    }

    if (separatedChunks.empty()) {
        qWarning() << "No separated chunks generated for file:" << audioPath;
        return separatedFiles;
    }

    // Combine all separated chunks into one tensor
    torch::Tensor combinedSeparated = torch::cat(separatedChunks, 0);

    // Create output directory
    QDir separatedDir("separated_chunks");
    if (!separatedDir.exists()) {
        separatedDir.mkpath(".");
    }

    // Save combined separated audio
    QString separatedFile = QString("separated_chunks/separated_%1.wav").arg(baseName);
    if (saveWav(combinedSeparated, separatedFile, 32000)) {
        separatedFiles.append(separatedFile);
    }

    return separatedFiles;
}

/**
 * @brief Starts asynchronous processing of audio file with feature and saves separated chunks.
 * @param audioPath Path to the input audio file.
 * @param featureName Name of the feature to use for separation.
 */
void ResourceManager::startProcessAndSaveSeparatedChunks(const QString& audioPath, const QString& featureName)
{
    if (m_isProcessing) return;
    m_isProcessing = true;
    emit processingStarted();
    emit startSeparationProcessing(audioPath, featureName);
}

/**
 * @brief Public wrapper for HTSATProcessor::readAndResampleAudio.
 * @param audioPath Path to the audio file.
 * @return Resampled audio data as vector of floats.
 */
std::vector<float> ResourceManager::readAndResampleAudio(const QString& audioPath)
{
    return m_htsatProcessor->readAndResampleAudio(audioPath);
}

/**
 * @brief Public wrapper for ZeroShotASPFeatureExtractor::saveAsWav.
 * @param waveform Tensor of shape (N,) with float32 samples.
 * @param filePath Output file path.
 * @param sampleRate Sample rate in Hz (default 32000).
 * @return True if saving succeeded, false otherwise.
 */
bool ResourceManager::saveWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate)
{
    return m_zeroShotAspProcessor->saveAsWav(waveform, filePath, sampleRate);
}
