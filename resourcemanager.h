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
#include "filerepo.h"
#include "asyncprocessor.h"
#include "audioserializer.h"
#include "filelocker.h"
#include "logger.h"

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
    /**
     * @brief Get the singleton instance of ResourceManager.
     * @return Pointer to the ResourceManager instance.
     */
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
    /**
     * @brief Add a folder to the resource manager for the specified file type.
     * @param folderPath The path of the folder to add.
     * @param folderParent The parent widget for the folder widget.
     * @param type The type of files in the folder (default: WavForFeature).
     * @return Pointer to the created FolderWidget.
     */
    FolderWidget* addFolder(const QString& folderPath, QWidget* folderParent, FileType type = FileType::WavForFeature);

    /**
     * @brief Add a single file to the resource manager for the specified file type.
     * @param filePath The path of the file to add.
     * @param fileParent The parent widget for the file widget.
     * @param type The type of the file (default: WavForFeature).
     * @return Pointer to the created FileWidget.
     */
    FileWidget* addSingleFile(const QString& filePath, QWidget* fileParent, FileType type = FileType::WavForFeature);

    /**
     * @brief Remove a file from the resource manager for the specified file type.
     * @param filePath The path of the file to remove.
     * @param type The type of the file.
     */
    void removeFile(const QString& filePath, FileType type);

    /**
     * @brief Remove a folder from the resource manager for the specified file type.
     * @param folderPath The path of the folder to remove.
     * @param type The type of files in the folder.
     */
    void removeFolder(const QString& folderPath, FileType type);

    /**
     * @brief Sort all files and folders in ascending or descending order.
     * @param order The sort order (default: AscendingOrder).
     */
    void sortAll(Qt::SortOrder order = Qt::AscendingOrder);

    /**
     * @brief Lock a file to prevent modifications during processing.
     * @param filePath The path of the file to lock.
     * @return True if the file was successfully locked, false otherwise.
     */
    bool lockFile(const QString& filePath);

    /**
     * @brief Unlock a previously locked file.
     * @param filePath The path of the file to unlock.
     * @return True if the file was successfully unlocked, false otherwise.
     */
    bool unlockFile(const QString& filePath);

    /**
     * @brief Check if a file is currently locked.
     * @param filePath The path of the file to check.
     * @return True if the file is locked, false otherwise.
     */
    bool isFileLocked(const QString& filePath) const;

    /**
     * @brief Get all added files for the specified file type.
     * @param type The type of files to retrieve (default: WavForFeature).
     * @return A set of file paths.
     */
    QSet<QString> getAddedFiles(FileType type = FileType::WavForFeature) const;

    /**
     * @brief Get all added folders for the specified file type.
     * @param type The type of folders to retrieve (default: WavForFeature).
     * @return A map of folder paths to FolderWidget pointers.
     */
    QMap<QString, FolderWidget*> getFolders(FileType type = FileType::WavForFeature) const;

    /**
     * @brief Get all added single files for the specified file type.
     * @param type The type of files to retrieve (default: WavForFeature).
     * @return A map of file paths to FileWidget pointers.
     */
    QMap<QString, FileWidget*> getSingleFiles(FileType type = FileType::WavForFeature) const;

    // =========================
    // Audio / Feature Processing
    // =========================
    /**
     * @brief Start asynchronous generation of audio features using HTSAT.
     * @param filePaths List of audio file paths to process.
     * @param outputFileName The name for the output feature file.
     */
    void startGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName);

    /**
     * @brief Start asynchronous audio separation using the specified feature.
     * @param filePaths List of audio file paths to separate.
     * @param featureName The name of the feature file to use for separation.
     */
    void startSeparateAudio(const QStringList& filePaths, const QString& featureName);

    // =========================
    // File saving interfaces for workers
    // =========================
    /**
     * @brief Create an output file path for HTSAT features.
     * @param outputFileName The name of the output file.
     * @return The full path to the output file.
     */
    QString createOutputFilePath(const QString& outputFileName);

    /**
     * @brief Save audio embedding to a specified file path.
     * @param embedding The embedding vector to save.
     * @param filePath The path where to save the embedding.
     * @return True if saved successfully, false otherwise.
     */
    bool saveEmbeddingToFile(const std::vector<float>& embedding, const QString& filePath);

    /**
     * @brief Save audio embedding with a generated output file name.
     * @param embedding The embedding vector to save.
     * @param outputFileName The name for the output file.
     * @return The path where the embedding was saved, or empty string on failure.
     */
    QString saveEmbedding(const std::vector<float>& embedding, const QString& outputFileName);

    /**
     * @brief Save a waveform tensor to a WAV file.
     * @param waveform The Torch tensor containing the audio waveform.
     * @param filePath The path where to save the WAV file.
     * @param sampleRate The sample rate of the audio (default: 32000 Hz).
     * @return True if saved successfully, false otherwise.
     */
    bool saveWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate = 32000);

    /**
     * @brief Save the audio separation result to a WAV file.
     * @param waveform The Torch tensor containing the separated audio.
     * @param outputName The name for the output WAV file.
     * @return True if saved successfully, false otherwise.
     */
    bool saveSeparationResult(const torch::Tensor& waveform, const QString& outputName);

    // =========================
    // Non-data / UI-related
    // =========================
    /**
     * @brief Automatically load sound features and update the UI lists.
     */
    void autoLoadSoundFeatures();

    /**
     * @brief Remove a sound feature from the resource manager.
     * @param featureName The name of the feature to remove.
     */
    void removeFeature(const QString& featureName);

    // =========================
    // Helper for deletion policy
    // =========================
    /**
     * @brief Check if a file type is deletable based on the deletion policy.
     * @param type The file type to check.
     * @return True if the file type is deletable, false otherwise.
     */
    static bool isDeletable(FileType type);

    // =========================
    // Directory creation
    // =========================
    /**
     * @brief Create necessary output directories for the application.
     */
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

    // Async processing
    void processingStarted();
    void processingProgress(int value);
    void processingFinished(const QStringList& results);
    void separationProcessingFinished(const QStringList& results);
    void processingError(const QString& error);
    void startHTSATProcessing(const QStringList& filePaths, const QString& outputFileName);
    void startSeparationProcessing(const QStringList& filePaths, const QString& featureName);

private:
    // Singleton pattern
    static ResourceManager* m_instance;
    ResourceManager(QObject* parent = nullptr);
    ~ResourceManager();

    // Component instances
    FileRepo* m_fileRepo;
    AsyncProcessor* m_asyncProcessor;
    AudioSerializer* m_serializer;
    FileLocker* m_fileLocker;

    // Legacy data structures for backward compatibility
    struct FileTypeData {
        QSet<QString> paths;
        QMap<QString, FolderWidget*> folders;
        QMap<QString, FileWidget*> files;
    };
    QMap<FileType, FileTypeData> m_fileTypeData;
    bool m_isProcessing;
    QSet<QString> m_lockedFiles;

    // Private helpers
    bool isDuplicate(const QString& path, FileType type) const;

    /**
     * @brief Handles and saves the final separated audio result for a processed file.
     *
     * This slot receives the completed separation result from the SeparationWorker after
     * all chunk processing and overlap-add reconstruction is finished. It generates an
     * appropriate output filename, saves the separated audio as a WAV file in the
     * designated separation results directory, and logs the operation status.
     *
     * The output filename follows the convention: {original_filename}_{feature_name}.wav
     * where the original file extension is removed and the feature name is appended.
     * This allows multiple separations of the same audio file to coexist with different
     * features applied.
     *
     * @param audioPath Original input audio file path (used to generate output filename)
     * @param featureName Name of the separation feature used for this result
     * @param finalTensor Separated audio tensor with shape (channels, samples) or (samples,)
     *                   at 32kHz sample rate, representing the final reconstructed audio
     *
     * @note This method is automatically called upon separation completion and should not
     *       be invoked manually.
     * @note The output directory (SEPARATED_RESULT_DIR) is automatically created if needed.
     * @note Processing state is reset after successful save, allowing subsequent operations.
     * @note Stereo files maintain channel separation in the saved WAV format.
     *
     * @see SeparationWorker::finished signal, saveWav, Constants::SEPARATED_RESULT_DIR
     */
    void handleFinalResult(const QString& audioPath,
                           const QString& featureName,
                           const torch::Tensor& finalTensor);

};

#endif // RESOURCEMANAGER_H
