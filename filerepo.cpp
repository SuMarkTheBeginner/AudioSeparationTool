#include "filerepo.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include "logger.h"

/**
 * @brief Constructor. Initializes data structures for different file types.
 */
FileRepo::FileRepo(QObject* parent) : QObject(parent)
{
    m_fileTypeData[FileType::WavForFeature] = FileTypeData();
    m_fileTypeData[FileType::SoundFeature] = FileTypeData();
    m_fileTypeData[FileType::WavForSeparation] = FileTypeData();
}

/**
 * @brief Adds a folder and its valid files.
 * @param folderPath Path to the folder.
 * @param folderParent Parent widget for FolderWidget.
 * @param type FileType to categorize.
 * @return Pointer to created FolderWidget, or nullptr if invalid.
 */
FolderWidget* FileRepo::addFolder(const QString& folderPath, QWidget* folderParent, FileType type)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        Logger::log(Logger::Level::Warning, "Folder does not exist: " + folderPath);
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
        Logger::log(Logger::Level::Warning, "No " + fileTypeDescription + " files found in folder: " + folderPath);
        return nullptr;
    }

    Logger::log(Logger::Level::Info, "Adding folder: " + folderPath + " with " + QString::number(files.size()) + " " + fileTypeDescription + " files");

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
 * @brief Adds a single file.
 * @param filePath Absolute path to the file.
 * @param fileParent Parent widget for FileWidget.
 * @param type FileType to categorize.
 * @return Pointer to created FileWidget, or nullptr if invalid.
 */
FileWidget* FileRepo::addSingleFile(const QString& filePath, QWidget* fileParent, FileType type)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        Logger::log(Logger::Level::Warning, "Invalid file: " + filePath);
        return nullptr;
    }

    QString expectedSuffix = (type == FileType::SoundFeature) ? "txt" : "wav";

    if (fi.suffix().toLower() != expectedSuffix) {
        Logger::log(Logger::Level::Warning, "Invalid file type for " + QString::number(static_cast<int>(type)) + ": " + filePath);
        return nullptr;
    }

    if (isDuplicate(fi.absoluteFilePath(), type)) {
        Logger::log(Logger::Level::Warning, "File already added: " + filePath);
        return nullptr;
    }

    Logger::log(Logger::Level::Info, "Adding single file: " + filePath);

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
 * @brief Removes a file.
 * @param filePath Path to remove.
 * @param type FileType.
 */
void FileRepo::removeFile(const QString& filePath, FileType type)
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
 * @brief Removes a folder and its files.
 * @param folderPath Path to remove.
 * @param type FileType.
 */
void FileRepo::removeFolder(const QString& folderPath, FileType type)
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

/**
 * @brief Gets added file paths.
 * @param type FileType.
 * @return QSet of paths.
 */
QSet<QString> FileRepo::getAddedFiles(FileType type) const
{
    return m_fileTypeData.value(type).paths;
}

/**
 * @brief Gets added folders.
 * @param type FileType.
 * @return QMap of folder paths to FolderWidget*.
 */
QMap<QString, FolderWidget*> FileRepo::getFolders(FileType type) const
{
    return m_fileTypeData.value(type).folders;
}

/**
 * @brief Gets added single files.
 * @param type FileType.
 * @return QMap of file paths to FileWidget*.
 */
QMap<QString, FileWidget*> FileRepo::getSingleFiles(FileType type) const
{
    return m_fileTypeData.value(type).files;
}

/**
 * @brief Checks if path is duplicate.
 * @param path File path.
 * @param type FileType.
 * @return True if duplicate.
 */
bool FileRepo::isDuplicate(const QString& path, FileType type) const
{
    return m_fileTypeData.value(type).paths.contains(path);
}

/**
 * @brief Emits file added signal.
 * @param path File path.
 * @param type FileType.
 */
void FileRepo::emitFileAdded(const QString& path, FileType type)
{
    emit fileAdded(path, type);
}

/**
 * @brief Emits file removed signal.
 * @param path File path.
 * @param type FileType.
 */
void FileRepo::emitFileRemoved(const QString& path, FileType type)
{
    emit fileRemoved(path, type);
}

/**
 * @brief Emits folder added signal.
 * @param folderPath Folder path.
 * @param type FileType.
 */
void FileRepo::emitFolderAdded(const QString& folderPath, FileType type)
{
    emit folderAdded(folderPath, type);
}

/**
 * @brief Emits folder removed signal.
 * @param folderPath Folder path.
 * @param type FileType.
 */
void FileRepo::emitFolderRemoved(const QString& folderPath, FileType type)
{
    emit folderRemoved(folderPath, type);
}
