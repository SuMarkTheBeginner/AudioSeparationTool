#ifndef ASYNCPROCESSOR_H
#define ASYNCPROCESSOR_H

#include <QObject>
#include <QStringList>
#include <memory>

class QThread;
class HTSATWorker;
class SeparationWorker;
class AudioSerializer;

#ifndef Q_MOC_RUN
#undef slots
#include <torch/torch.h>
#define slots
#endif

/**
 * @brief Handles asynchronous processing operations using worker threads.
 *
 * Manages threading coordination and worker execution for audio feature generation
 * and audio separation tasks using RAII thread management (no static globals).
 */
class AsyncProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the AsyncProcessor.
     * @param parent Optional parent QObject.
     */
    explicit AsyncProcessor(AudioSerializer* serializer, QObject* parent = nullptr);

    /**
     * @brief Destructor - safely cleans up threads and workers.
     */
    ~AsyncProcessor();

    /**
     * @brief Start asynchronous generation of audio features using HTSAT.
     * @param filePaths List of audio file paths to process.
     * @param outputFileName The name for the output feature file.
     */
    void startFeatureGeneration(const QStringList& filePaths, const QString& outputFileName);

    /**
     * @brief Start asynchronous audio separation using the specified feature.
     * @param filePaths List of audio file paths to separate.
     * @param featureName The name of the feature file to use for separation.
     */
    void startAudioSeparation(const QStringList& filePaths, const QString& featureName);

    /**
     * @brief Check if any processing is currently running.
     * @return True if processing is active, false otherwise.
     */
    bool isProcessing() const;

signals:
    /**
     * @brief Emitted when processing starts.
     */
    void processingStarted();

    /**
     * @brief Emitted to update progress during processing.
     * @param value Progress value (0-100).
     */
    void processingProgress(int value);

    /**
     * @brief Emitted when feature generation processing is finished.
     * @param results List of produced file paths.
     */
    void processingFinished(const QStringList& results);

    /**
     * @brief Emitted when audio separation processing is finished.
     * @param results List of produced file paths.
     */
    void separationProcessingFinished(const QStringList& results);

    /**
     * @brief Emitted when an error occurs during processing.
     * @param error Description of the error.
     */
    void processingError(const QString& error);

private:
    /**
     * @brief Initialize thread and worker setup.
     */
    void initializeThreadsAndWorkers();

    /**
     * @brief Clean up threads and workers safely.
     */
    void cleanupThreadsAndWorkers();

    /**
     * @brief Handle the final separated audio result.
     *
     * This slot saves the separated audio as a WAV file and emits completion signals.
     * Generates output filename with convention: {original_filename}_{feature_name}.wav
     *
     * @param audioPath Path to the original input audio file.
     * @param featureName Name of the separation feature used.
     * @param finalTensor Separated audio tensor (channels, samples) at 32kHz.
     */
    void handleFinalResult(const QString& audioPath,
                           const QString& featureName,
                           const torch::Tensor& finalTensor);

private:
    AudioSerializer* m_serializer;               ///< Audio serialization dependency

    QThread* m_htsatThread = nullptr;            ///< Thread for HTSAT feature generation
    HTSATWorker* m_htsatWorker = nullptr;        ///< HTSAT worker instance

    QThread* m_separationThread = nullptr;       ///< Thread for audio separation
    SeparationWorker* m_separationWorker = nullptr; ///< Separation worker instance

    bool m_isProcessing = false;                 ///< Current processing state
};

#endif // ASYNCPROCESSOR_H
