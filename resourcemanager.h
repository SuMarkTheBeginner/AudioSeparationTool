#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>
#include <QtGlobal>
#include "folderwidget.h"
#include "filewidget.h"
#include <vector>
#ifndef Q_MOC_RUN
#undef slots
#endif
#include <torch/torch.h>
#ifndef Q_MOC_RUN
#define slots
#endif

/**
 * @brief Singleton class for managing all file resources in the application.
 *
 * Centralizes file management including adding/removing files and folders,
 * duplicate checking, locking/unlocking files, and emitting signals for UI updates.
 * Supports multiple file types including WAVs, sound features, temporary segments,
 * and final separation results.
 */
class ResourceManager : public QObject
{
    Q_OBJECT

public:
    // Singleton instance
    static ResourceManager* instance();

    // File types
    enum class FileType {
        WavForFeature,      ///< WAV files used to generate sound feature vectors
        WavForSeparation,   ///< WAV files to be separated using feature vectors
        TempSegment,        ///< Temporary chunks during separation
        SoundFeature,       ///< Sound feature vector files
        SeparationResult    ///< Final separated audio result files
    };

    // =========================
    // Core management (檔案/資料夾管理)
    // =========================
    FolderWidget* addFolder(const QString& folderPath, QWidget* folderParent, FileType type = FileType::WavForFeature);
    FileWidget* addSingleFile(const QString& filePath, QWidget* fileParent, FileType type = FileType::WavForFeature);
    void removeFile(const QString& filePath, FileType type);
    void removeFolder(const QString& folderPath, FileType type);
    void sortAll(Qt::SortOrder order = Qt::AscendingOrder);
    bool lockFile(const QString& filePath);
    bool unlockFile(const QString& filePath);
    bool isFileLocked(const QString& filePath) const;
    QSet<QString> getAddedFiles(FileType type = FileType::WavForFeature) const;
    QMap<QString, FolderWidget*> getFolders(FileType type = FileType::WavForFeature) const;
    QMap<QString, FileWidget*> getSingleFiles(FileType type = FileType::WavForFeature) const;

    // =========================
    // Audio / Feature Processing (Synchronous)
    // =========================
    void startGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName); // Synchronous HTSAT
    void startSeparateAudio(const QStringList& filePaths, const QString& featureName);         // Synchronous separation

    // =========================
    // File saving interfaces for workers
    // =========================
    QString createOutputFilePath(const QString& outputFileName);                     // HTSAT feature
    bool saveEmbeddingToFile(const std::vector<float>& embedding, const QString& filePath);
    QString saveEmbedding(const std::vector<float>& embedding, const QString& outputFileName);

    QString createTempFilePath(const QString& baseName, int index, FileType type = FileType::TempSegment); // Temp chunk
    bool saveWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate = 32000);          // Temp chunk or SeparationResult
    bool saveSeparationResult(const torch::Tensor& waveform, const QString& outputName);

    // =========================
    // Non-data / UI-related
    // =========================
    void autoLoadSoundFeatures(); ///< 自動載入 sound feature，僅影響 UI/列表
    void removeFeature(const QString& featureName);

    // =========================
    // Helper for deletion policy
    // =========================
    static bool isDeletable(FileType type);

    // =========================
    // Directory creation
    // =========================
    void createOutputDirectories();


signals:
    // Resource management / UI update
    void fileAdded(const QString& path, ResourceManager::FileType type);
    void fileRemoved(const QString& path, ResourceManager::FileType type);
    void folderAdded(const QString& folderPath, ResourceManager::FileType type);
    void folderRemoved(const QString& folderPath, ResourceManager::FileType type);
    void fileLocked(const QString& path);
    void fileUnlocked(const QString& path);
    void progressUpdated(int value);
    void featuresUpdated();

    // Synchronous processing
    void processingStarted();
    void processingProgress(int value);
    void processingFinished(const QStringList& results);
    void separationProcessingFinished(const QStringList& results);
    void processingError(const QString& error);

private:
    // Singleton pattern
    static ResourceManager* m_instance;
    ResourceManager(QObject* parent = nullptr);
    ~ResourceManager();

    // Data storage
    struct FileTypeData {
        QSet<QString> paths;
        QMap<QString, FolderWidget*> folders;
        QMap<QString, FileWidget*> files;
    };
    QMap<FileType, FileTypeData> m_fileTypeData;
    QSet<QString> m_lockedFiles;
    bool m_isProcessing;

    // Private helpers
    bool isDuplicate(const QString& path, FileType type) const;
    void emitFileAdded(const QString& path, FileType type);
    void emitFileRemoved(const QString& path, FileType type);
    void emitFolderAdded(const QString& folderPath, FileType type);
    void emitFolderRemoved(const QString& folderPath, FileType type);

};

#endif // RESOURCEMANAGER_H
