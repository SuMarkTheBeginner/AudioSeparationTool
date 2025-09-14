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
#include "zero_shot_asp_feature_extractor.h"

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
    void removeFile(const QString& filePath, FileType type = FileType::WavForFeature);
    void removeFolder(const QString& folderPath, FileType type = FileType::WavForFeature);
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
    void autoLoadSoundFeatures(); ///< Automatically load sound features from output_features folder
    void removeFeature(const QString& featureName); ///< Remove a sound feature by name

    // ZeroShotASP Processing
    bool loadZeroShotASPModel(const QString& modelPath);
    torch::Tensor separateAudio(const torch::Tensor& waveform, const torch::Tensor& condition);

    // New method to split and save wav chunks using HTSATProcessor and ZeroShotASPFeatureExtractor
    QStringList splitAndSaveWavChunks(const QString& audioPath);
    // New method to process audio with feature and save separated chunks
    QStringList processAndSaveSeparatedChunks(const QString& audioPath, const QString& featureName);


    // Public wrappers for private methods
    std::vector<float> readAndResampleAudio(const QString& audioPath);
    bool saveWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate = 32000);

signals:
    void fileAdded(const QString& path, ResourceManager::FileType type);
    void fileRemoved(const QString& path, ResourceManager::FileType type);
    void folderAdded(const QString& folderPath, ResourceManager::FileType type);
    void folderRemoved(const QString& folderPath, ResourceManager::FileType type);
    void fileLocked(const QString& path);
    void fileUnlocked(const QString& path);
    void progressUpdated(int value);

    void featuresUpdated(); // Signal to notify that features have been updated

private:
    // Singleton pattern
    static ResourceManager* m_instance;
    ResourceManager(QObject* parent = nullptr);
    ~ResourceManager();

    HTSATProcessor* m_htsatProcessor; ///< HTSATProcessor instance for audio processing
    ZeroShotASPFeatureExtractor* m_zeroShotAspProcessor; ///< ZeroShotASPFeatureExtractor instance for audio separation

    // Data members for WavForFeature
    QSet<QString> m_wavForFeaturePaths;           ///< All added file paths for WavForFeature
    QMap<QString, FolderWidget*> m_wavForFeatureFolders; ///< Folder widgets for WavForFeature
    QMap<QString, FileWidget*> m_wavForFeatureFiles; ///< Single file widgets for WavForFeature

    // Data members for WavForSeparation
    QSet<QString> m_wavForSeparationPaths;           ///< All added file paths for WavForSeparation
    QMap<QString, FolderWidget*> m_wavForSeparationFolders; ///< Folder widgets for WavForSeparation
    QMap<QString, FileWidget*> m_wavForSeparationFiles; ///< Single file widgets for WavForSeparation

    // Data members for SoundFeature (placeholder)
    QSet<QString> m_soundFeaturePaths;           ///< All added file paths for SoundFeature
    QMap<QString, FolderWidget*> m_soundFeatureFolders; ///< Folder widgets for SoundFeature
    QMap<QString, FileWidget*> m_soundFeatureFiles; ///< Single file widgets for SoundFeature

    QSet<QString> m_lockedFiles;              ///< Locked file paths

    // Helper methods
    bool isDuplicate(const QString& path, FileType type) const;
    void emitFileAdded(const QString& path, FileType type);
    void emitFileRemoved(const QString& path, FileType type);
    void emitFolderAdded(const QString& folderPath, FileType type);
    void emitFolderRemoved(const QString& folderPath, FileType type);
};

#endif // RESOURCEMANAGER_H
