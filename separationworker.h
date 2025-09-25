// SeparationWorker.h
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#ifndef Q_MOC_RUN
#undef slots
#include <torch/torch.h>
#define slots
#endif
#include "zero_shot_asp_feature_extractor.h"
#include "constants.h"

/**
 * @brief Worker class for audio separation processing in a separate thread.
 *
 * Processes audio files using zero-shot audio separation models to separate different sound sources.
 * This class handles both mono and stereo audio processing with optimized overlap-add algorithms.
 */
class SeparationWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Processing result structure containing the separated audio and metadata.
     */
    struct ProcessingResult {
        torch::Tensor separatedAudio;  ///< The separated audio tensor
        QString errorMessage;          ///< Error message if processing failed
        bool success = false;          ///< Whether processing was successful

        ProcessingResult() = default;
        ProcessingResult(const torch::Tensor& audio) : separatedAudio(audio), success(true) {}
        ProcessingResult(const QString& error) : errorMessage(error), success(false) {}
    };

    /**
     * @brief Audio channel information for stereo processing.
     */
    struct AudioChannel {
        torch::Tensor waveform;        ///< Channel waveform data
        QString name;                  ///< Channel identifier (e.g., "left", "right")
        int progressOffset = 0;        ///< Progress offset for this channel (0-100)

        AudioChannel() = default;
        AudioChannel(const torch::Tensor& wf, const QString& channelName, int offset = 0)
            : waveform(wf), name(channelName), progressOffset(offset) {}
    };

    /**
     * @brief Constructs the SeparationWorker.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit SeparationWorker(QObject* parent = nullptr);
    ~SeparationWorker() = default;

    // Public API methods
    /**
     * @brief Loads the feature tensor from the specified feature file.
     * @param featurePath Path to the feature file.
     * @return The loaded feature tensor, or empty tensor on error.
     */
    torch::Tensor loadFeature(const QString& featurePath);

    /**
     * @brief Processes a single audio chunk with the given condition.
     * @param waveform The audio waveform chunk (shape: [1, samples, 1]).
     * @param condition The condition tensor for separation.
     * @param extractor Pointer to the feature extractor.
     * @return The processed separation result tensor.
     */
    torch::Tensor processChunk(const torch::Tensor& waveform,
                               const torch::Tensor& condition,
                               ZeroShotASPFeatureExtractor* extractor);

    /**
     * @brief Performs overlap-add on a list of chunk tensors.
     * @param chunks Vector of chunk tensors to combine.
     * @return The combined tensor using overlap-add.
     */
    torch::Tensor doOverlapAdd(const std::vector<torch::Tensor>& chunks);


signals:

    /**
     * @brief Emitted when separation for a single file is finished.
     * @param audioPath Path to the original audio file.
     * @param featureName Name of the feature used.
     * @param finalTensor The final separated audio tensor.
     */
    void finished(const QString& audioPath,
                            const QString& featureName,
                            const torch::Tensor& finalTensor);

    void progressUpdated(int value); ///< Signal emitted to update progress (0-100)
    void error(const QString& errorMessage); ///< Signal emitted when an error occurs

public slots:
    /**
     * @brief Slot to start processing audio files for separation.
     * @param filePaths List of audio file paths to process.
     * @param featureName Name of the feature file to use for separation.
     */
    void processFile(const QStringList& filePaths, const QString& featureName);

private:
    // Core processing methods
    /**
     * @brief Validates audio file and loads waveform tensor.
     * @param audioPath Path to the audio file.
     * @param keepOriginalFormat Whether to preserve stereo format or convert to mono.
     * @return Loaded waveform tensor or empty tensor on error.
     */
    torch::Tensor loadAudioWaveform(const QString& audioPath, bool keepOriginalFormat = false);

    /**
     * @brief Determines if audio is stereo and delegates to appropriate processing method.
     * @param audioPath Path to the audio file.
     * @param featureName Name of the feature to use for separation.
     */
    void processAudioFile(const QString& audioPath, const QString& featureName);

    /**
     * @brief Processes a mono audio file using chunked separation.
     * @param audioPath Path to the mono audio file.
     * @param featureName Name of the feature to use.
     * @return ProcessingResult containing the separated audio or error information.
     */
    ProcessingResult processMonoAudio(const QString& audioPath, const QString& featureName);

    /**
     * @brief Processes a stereo audio file by separating channels independently.
     * @param audioPath Path to the stereo audio file.
     * @param featureName Name of the feature to use.
     * @return ProcessingResult containing the separated audio or error information.
     */
    ProcessingResult processStereoAudio(const QString& audioPath, const QString& featureName);

    /**
     * @brief Extracts individual channels from stereo audio.
     * @param stereoWaveform The stereo waveform tensor.
     * @return Vector of AudioChannel objects for left and right channels.
     */
    std::vector<AudioChannel> extractStereoChannels(const torch::Tensor& stereoWaveform);

    /**
     * @brief Processes a single audio channel using chunked separation.
     * @param channel The audio channel to process.
     * @param condition The conditioning tensor for separation.
     * @param extractor The feature extractor to use.
     * @return ProcessingResult containing the processed channel or error information.
     */
    ProcessingResult processAudioChannel(const AudioChannel& channel,
                                       const torch::Tensor& condition,
                                       ZeroShotASPFeatureExtractor* extractor);

    /**
     * @brief Creates a feature extractor and loads the model.
     * @return Unique pointer to the initialized extractor, or nullptr on error.
     */
    std::unique_ptr<ZeroShotASPFeatureExtractor> createAndLoadExtractor();

    /**
     * @brief Loads and validates the conditioning feature tensor.
     * @param featureName Name of the feature to load.
     * @return Loaded feature tensor or empty tensor on error.
     */
    torch::Tensor loadConditioningFeature(const QString& featureName);

    /**
     * @brief Divides audio into overlapping chunks for processing.
     * @param waveform The audio waveform to chunk.
     * @return Vector of audio chunks ready for processing.
     */
    std::vector<torch::Tensor> createAudioChunks(const torch::Tensor& waveform);

    /**
     * @brief Processes multiple audio chunks through the separation model.
     * @param chunks Vector of audio chunks to process.
     * @param condition The conditioning tensor for separation.
     * @param extractor The feature extractor to use.
     * @param progressCallback Callback function for progress updates.
     * @return Vector of processed chunks or empty vector on error.
     */
    std::vector<torch::Tensor> processAudioChunks(const std::vector<torch::Tensor>& chunks,
                                                 const torch::Tensor& condition,
                                                 ZeroShotASPFeatureExtractor* extractor,
                                                 std::function<void(int)> progressCallback);

    /**
     * @brief Reconstructs full audio from processed chunks using overlap-add.
     * @param processedChunks Vector of processed audio chunks.
     * @param originalLength Expected length of the reconstructed audio.
     * @return Reconstructed audio tensor or empty tensor on error.
     */
    torch::Tensor reconstructAudioFromChunks(const std::vector<torch::Tensor>& processedChunks,
                                           int64_t originalLength);

    /**
     * @brief Combines stereo channels back into a single stereo tensor.
     * @param leftChannel Processed left channel data.
     * @param rightChannel Processed right channel data.
     * @return Combined stereo tensor or empty tensor on error.
     */
    torch::Tensor combineStereoChannels(const torch::Tensor& leftChannel,
                                       const torch::Tensor& rightChannel);

    // Utility methods
    /**
     * @brief Validates that a tensor has the expected shape.
     * @param tensor The tensor to validate.
     * @param expectedSizes Expected tensor dimensions.
     * @param tensorName Name of the tensor for error messages.
     * @return True if tensor shape is valid, false otherwise.
     */
    bool validateTensorShape(const torch::Tensor& tensor,
                           const std::vector<int64_t>& expectedSizes,
                           const QString& tensorName);

    /**
     * @brief Emits an error signal with formatted message.
     * @param message The error message to emit.
     */
    void emitError(const QString& message);

    /**
     * @brief Emits progress update signal.
     * @param progress Progress value (0-100).
     */
    void emitProgress(int progress);

    // Configuration constants
    static constexpr float DEFAULT_OVERLAP_RATE = 0.5f;  ///< Default overlap rate for chunk processing
    static constexpr int DEFAULT_CLIP_SAMPLES = 44100;    ///< Default number of samples per chunk

    // Member variables
    float m_overlapRate = DEFAULT_OVERLAP_RATE;  ///< Overlap rate for chunk processing
    int m_clipSamples = DEFAULT_CLIP_SAMPLES;    ///< Number of samples per chunk
};
