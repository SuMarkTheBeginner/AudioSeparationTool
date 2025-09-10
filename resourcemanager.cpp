#include "resourcemanager.h"
#include "fileutils.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <QDateTime>

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
}

/**
 * @brief Destructor for ResourceManager.
 */
ResourceManager::~ResourceManager()
{
    // Cleanup widgets
    for (auto widget : m_folderMap.values()) {
        delete widget;
    }
    for (auto widget : m_singleFileMap.values()) {
        delete widget;
    }
    m_folderMap.clear();
    m_singleFileMap.clear();
}

/**
 * @brief Adds a folder and its WAV files to the resource manager.
 * @param folderPath The path to the folder.
 * @param folderParent The parent widget for FolderWidget.
 * @param type The file type (currently only WavForFeature supported).
 */
FolderWidget* ResourceManager::addFolder(const QString& folderPath, QWidget* folderParent, FileType type)
{
    if (type != FileType::WavForFeature) {
        qDebug() << "FileType not yet supported:" << static_cast<int>(type);
        return nullptr;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << "Folder does not exist:" << folderPath;
        return nullptr;
    }

    QStringList wavFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);
    if (wavFiles.isEmpty()) {
        qDebug() << "No WAV files found in folder:" << folderPath;
        return nullptr;
    }

    qDebug() << "Adding folder:" << folderPath << "with" << wavFiles.size() << "WAV files";

    // Create FolderWidget if not exists
    FolderWidget* folderWidget = nullptr;
    if (m_folderMap.contains(folderPath)) {
        folderWidget = m_folderMap[folderPath];
    } else {
        folderWidget = new FolderWidget(folderPath, folderParent);
        m_folderMap.insert(folderPath, folderWidget);
        emitFolderAdded(folderPath, type);
    }

    QStringList newFiles;
    for (const QString& f : wavFiles) {
        QString fullPath = dir.absoluteFilePath(f);
        if (!isDuplicate(fullPath, type)) {
            newFiles.append(f);
            m_addedFilePaths.insert(fullPath);
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
    if (type != FileType::WavForFeature) {
        qDebug() << "FileType not yet supported:" << static_cast<int>(type);
        return nullptr;
    }

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable() || fi.suffix().toLower() != "wav") {
        qDebug() << "Invalid file:" << filePath;
        return nullptr;
    }

    if (isDuplicate(fi.absoluteFilePath(), type)) {
        qDebug() << "File already added:" << filePath;
        return nullptr;
    }

    qDebug() << "Adding single file:" << filePath;

    FileWidget* fileWidget = new FileWidget(filePath, fileParent);
    m_singleFileMap.insert(fi.absoluteFilePath(), fileWidget);
    m_addedFilePaths.insert(fi.absoluteFilePath());
    emitFileAdded(fi.absoluteFilePath(), type);

    return fileWidget;
}

/**
 * @brief Removes a file from the resource manager.
 * @param filePath The path to the file to remove.
 */
void ResourceManager::removeFile(const QString& filePath)
{
    if (m_addedFilePaths.remove(filePath)) {
        // Remove from single files if present
        if (m_singleFileMap.contains(filePath)) {
            FileWidget* fw = m_singleFileMap.take(filePath);
            fw->deleteLater();
        }
        // Note: For folder files, removal is handled by FolderWidget
        emitFileRemoved(filePath, FileType::WavForFeature);
    }
}

/**
 * @brief Removes a folder from the resource manager.
 * @param folderPath The path to the folder to remove.
 */
void ResourceManager::removeFolder(const QString& folderPath)
{
    if (m_folderMap.contains(folderPath)) {
        FolderWidget* fw = m_folderMap.take(folderPath);
        fw->deleteLater();

        // Remove all files in the folder from addedFilePaths
        QSet<QString> toRemove;
        for (const QString& filePath : m_addedFilePaths) {
            if (filePath.startsWith(folderPath + "/")) {
                toRemove.insert(filePath);
            }
        }
        for (const QString& filePath : toRemove) {
            m_addedFilePaths.remove(filePath);
            emitFileRemoved(filePath, FileType::WavForFeature);
        }
        emitFolderRemoved(folderPath, FileType::WavForFeature);
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
    if (type == FileType::WavForFeature) {
        return m_addedFilePaths;
    }
    return QSet<QString>();
}

/**
 * @brief Gets the map of folder widgets for a given type.
 * @param type The file type.
 * @return Map of folder paths to FolderWidget pointers.
 */
QMap<QString, FolderWidget*> ResourceManager::getFolders(FileType type) const
{
    if (type == FileType::WavForFeature) {
        return m_folderMap;
    }
    return QMap<QString, FolderWidget*>();
}

/**
 * @brief Gets the map of single file widgets for a given type.
 * @param type The file type.
 * @return Map of file paths to FileWidget pointers.
 */
QMap<QString, FileWidget*> ResourceManager::getSingleFiles(FileType type) const
{
    if (type == FileType::WavForFeature) {
        return m_singleFileMap;
    }
    return QMap<QString, FileWidget*>();
}

/**
 * @brief Checks if a path is a duplicate for the given type.
 * @param path The file path.
 * @param type The file type.
 * @return True if duplicate, false otherwise.
 */
bool ResourceManager::isDuplicate(const QString& path, FileType type) const
{
    if (type == FileType::WavForFeature) {
        return m_addedFilePaths.contains(path);
    }
    return false;
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

/**
 * @brief Processes a list of audio files to generate embeddings and saves them to a file.
 * @param filePaths List of audio file paths.
 * @param outputFileName Path to the output file.
 */
void ResourceManager::generateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    // Load model if not loaded
    if (!m_htsatProcessor->isModelLoaded()) {
        QString modelPath = "models/htsat_embedding_model.pt";
        qDebug() << "Current working directory:" << QDir::currentPath();
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
}
