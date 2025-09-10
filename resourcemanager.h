#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>
#include <QtGlobal>
#include "folderwidget.h"
#include "filewidget.h"
#include "htsatprocessor.h"

/**
 * @brief Singleton class for managing all file resources in the application.
 *
 * Centralizes file management including adding/removing files and folders,
 * duplicate checking, locking/unlocking files, and emitting signals for UI updates.
 * Supports three types of files: WAV for feature generation, sound feature vectors,
 * and WAV files for separation.
 */
class ResourceManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Gets the singleton instance of ResourceManager.
     * @return Pointer to the ResourceManager instance.
     */
    static ResourceManager* instance();

    /**
     * @brief Enumeration for file types managed by ResourceManager.
     */
    enum class FileType {
        WavForFeature,     ///< WAV files used to generate sound feature vectors
        SoundFeature,      ///< Sound feature vector files
        WavForSeparation   ///< WAV files to be separated using feature vectors
    };

    // Core management methods
    FolderWidget* addFolder(const QString& folderPath, QWidget* folderParent, FileType type = FileType::WavForFeature);
    FileWidget* addSingleFile(const QString& filePath, QWidget* fileParent, FileType type = FileType::WavForFeature);
    void removeFile(const QString& filePath);
    void removeFolder(const QString& folderPath);
    void sortAll(Qt::SortOrder order = Qt::AscendingOrder); // Simplified sorting

    // Locking methods
    bool lockFile(const QString& filePath);
    bool unlockFile(const QString& filePath);
    bool isFileLocked(const QString& filePath) const;

    // Getters
    QSet<QString> getAddedFiles(FileType type = FileType::WavForFeature) const;
    QMap<QString, FolderWidget*> getFolders(FileType type = FileType::WavForFeature) const;
    QMap<QString, FileWidget*> getSingleFiles(FileType type = FileType::WavForFeature) const;

    // Processing
    bool loadModel(const QString& modelPath);
    void generateAudioFeatures(const QStringList& filePaths, const QString& outputFileName);

signals:
    void fileAdded(const QString& path, ResourceManager::FileType type);
    void fileRemoved(const QString& path, ResourceManager::FileType type);
    void folderAdded(const QString& folderPath, ResourceManager::FileType type);
    void folderRemoved(const QString& folderPath, ResourceManager::FileType type);
    void fileLocked(const QString& path);
    void fileUnlocked(const QString& path);
    void progressUpdated(int value);

private:
    // Singleton pattern
    static ResourceManager* m_instance;
    ResourceManager(QObject* parent = nullptr);
    ~ResourceManager();

    HTSATProcessor* m_htsatProcessor; ///< HTSATProcessor instance for audio processing

    // Data members for file type 1 (WAV for feature generation)
    QSet<QString> m_addedFilePaths;           ///< All added file paths for type 1
    QMap<QString, FolderWidget*> m_folderMap; ///< Folder widgets for type 1
    QMap<QString, FileWidget*> m_singleFileMap; ///< Single file widgets for type 1
    QSet<QString> m_lockedFiles;              ///< Locked file paths

    // Placeholders for other file types (to be implemented later)
    // QSet<QString> m_soundFeaturePaths;
    // QMap<QString, ...> m_soundFeatureWidgets;
    // QSet<QString> m_separationWavPaths;
    // QMap<QString, ...> m_separationWavWidgets;

    // Helper methods
    bool isDuplicate(const QString& path, FileType type) const;
    void emitFileAdded(const QString& path, FileType type);
    void emitFileRemoved(const QString& path, FileType type);
    void emitFolderAdded(const QString& folderPath, FileType type);
    void emitFolderRemoved(const QString& folderPath, FileType type);
};

#endif // RESOURCEMANAGER_H
