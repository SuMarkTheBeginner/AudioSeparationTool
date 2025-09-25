#ifndef FILEREPOS_H
#define FILEREPOS_H

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>
#include "folderwidget.h"
#include "filewidget.h"

/**
 * @brief Class for pure data operations for file and folder management.
 *
 * Handles adding/removing files and folders, duplicate checking, and querying
 * without UI or threading concerns. Designed to be a data access layer.
 */
class FileRepo : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor. Initializes data structures for different file types.
     * @param parent Parent QObject.
     */
    FileRepo(QObject* parent = nullptr);

    // File types (shared with ResourceManager)
    enum class FileType {
        WavForFeature,      ///< WAV files used to generate sound feature vectors
        WavForSeparation,   ///< WAV files to be separated using feature vectors
        TempSegment,        ///< Temporary chunks during separation
        SoundFeature,       ///< Sound feature vector files
        SeparationResult    ///< Final separated audio result files
    };

    // =========================
    // Core data operations
    // =========================

    /**
     * @brief Adds a folder and its valid files.
     * @param folderPath Path to the folder.
     * @param folderParent Parent widget for FolderWidget.
     * @param type FileType to categorize.
     * @return Pointer to created FolderWidget, or nullptr if invalid.
     */
    FolderWidget* addFolder(const QString& folderPath, QWidget* folderParent, FileType type);

    /**
     * @brief Adds a single file.
     * @param filePath Absolute path to the file.
     * @param fileParent Parent widget for FileWidget.
     * @param type FileType to categorize.
     * @return Pointer to created FileWidget, or nullptr if invalid.
     */
    FileWidget* addSingleFile(const QString& filePath, QWidget* fileParent, FileType type);

    /**
     * @brief Removes a file.
     * @param filePath Path to remove.
     * @param type FileType.
     */
    void removeFile(const QString& filePath, FileType type);

    /**
     * @brief Removes a folder and its files.
     * @param folderPath Path to remove.
     * @param type FileType.
     */
    void removeFolder(const QString& folderPath, FileType type);

    /**
     * @brief Gets added file paths.
     * @param type FileType.
     * @return QSet of paths.
     */
    QSet<QString> getAddedFiles(FileType type) const;

    /**
     * @brief Gets added folders.
     * @param type FileType.
     * @return QMap of folder paths to FolderWidget*.
     */
    QMap<QString, FolderWidget*> getFolders(FileType type) const;

    /**
     * @brief Gets added single files.
     * @param type FileType.
     * @return QMap of file paths to FileWidget*.
     */
    QMap<QString, FileWidget*> getSingleFiles(FileType type) const;

signals:
    // Signals for UI updates or observers
    void fileAdded(const QString& path, FileType type);
    void fileRemoved(const QString& path, FileType type);
    void folderAdded(const QString& folderPath, FileType type);
    void folderRemoved(const QString& folderPath, FileType type);

private:
    // Internal data structure
    struct FileTypeData {
        QSet<QString> paths;
        QMap<QString, FolderWidget*> folders;
        QMap<QString, FileWidget*> files;
    };

    QMap<FileType, FileTypeData> m_fileTypeData;

    // Helper methods
    bool isDuplicate(const QString& path, FileType type) const;
    void emitFileAdded(const QString& path, FileType type);
    void emitFileRemoved(const QString& path, FileType type);
    void emitFolderAdded(const QString& folderPath, FileType type);
    void emitFolderRemoved(const QString& folderPath, FileType type);
};

#endif // FILEREPOS_H
