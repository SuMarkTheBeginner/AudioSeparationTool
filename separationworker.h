// SeparationWorker.h
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
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
 */
class SeparationWorker : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the SeparationWorker.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit SeparationWorker(QObject* parent = nullptr);
    ~SeparationWorker() = default;

    /**
     * @brief Loads the feature tensor from the specified feature file.
     * @param featurePath Path to the feature file.
     * @return The loaded feature tensor.
     */
    torch::Tensor loadFeature(const QString& featurePath);

    /**
     * @brief Processes a single audio chunk with the given condition.
     * @param waveform The audio waveform chunk.
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

    /**
     * @brief Performs overlap-add on chunks loaded from file paths.
     * @param chunkFilePaths List of paths to chunk files.
     * @return The combined tensor using overlap-add.
     */
    torch::Tensor doOverlapAdd(const QStringList& chunkFilePaths);

signals:
    /**
     * @brief Emitted when a single chunk processing is ready.
     * @param audioPath Path to the original audio file.
     * @param featureName Name of the feature used.
     * @param chunkData The processed chunk data.
     */
    void chunkReady(const QString& audioPath,
                    const QString& featureName,
                    const torch::Tensor& chunkData);

    /**
     * @brief Emitted when separation for a single file is finished.
     * @param audioPath Path to the original audio file.
     * @param featureName Name of the feature used.
     * @param finalTensor The final separated audio tensor.
     */
    void separationFinished(const QString& audioPath,
                            const QString& featureName,
                            const torch::Tensor& finalTensor);

    void progressUpdated(int value); ///< Signal emitted to update progress (0-100)
    void error(const QString& errorMessage); ///< Signal emitted when an error occurs
    void deleteChunkFile(const QString& path); ///< Signal emitted to delete a temporary chunk file


public slots:
    /**
     * @brief Slot to start processing audio files for separation.
     * @param filePaths List of audio file paths to process.
     * @param featureName Name of the feature file to use for separation.
     */
    void processFile(const QStringList& filePaths, const QString& featureName);

private:
    /**
     * @brief Processes a single audio (mono) file using zero-shot audio separation.
     *
     * This method handles the complete pipeline for separating a mono audio file into targeted
     * audio sources based on the provided feature representation. The processing involves:
     * - Loading and validating the audio waveform
     * - Verifying the audio is mono (delegates to stereo processing if not)
     * - Loading the separation model (ZeroShotASP)
     * - Loading the conditioning feature tensor
     * - Chunking the audio with overlap for memory-efficient processing
     * - Running each chunk through the neural network model
     * - Reconstructing the full audio using overlap-add synthesis
     * - Emitting completion signals with the separated audio
     *
     * The method uses constant overlap rate and clip sample sizes defined in Constants.h
     * for consistent chunking behavior across different audio files.
     *
     * @param audioPath Path to the input audio file (must be readable WAV/MP3 format)
     * @param featureName Name of the feature file used for conditioning separation
     *                   (loaded from Constants::OUTPUT_FEATURES_DIR/featureName.txt)
     *
     * @note This method assumes mono input audio. Stereo files are automatically delegated
     *       to processSingleFileStereo for proper channel handling.
     * @note Progress updates are emitted during chunk processing.
     * @note Any processing errors result in error signal emission and early termination.
     * @note The separation model is loaded fresh and unloaded after processing to manage memory.
     *
     * @see processSingleFileStereo, processChannel, doOverlapAdd, ZeroShotASPFeatureExtractor
     */
    void processSingleFile(const QString& audioPath, const QString& featureName);

    /**
     * @brief Processes a stereo audio file by separating channels independently.
     *
     * This method handles stereo audio files by treating each channel (left/right) as a separate
     * mono audio stream and processing them individually using zero-shot separation. The stereo
     * separation pipeline includes:
     * - Loading and validating the stereo audio waveform (must be 2-channel)
     * - Loading the separation model and conditioning feature tensor
     * - Extracting left and right audio channels from the stereo input
     * - Processing each channel separately via processChannel method
     * - Recombining the processed mono results back into stereo format
     * - Trimming any excess samples to match original audio length
     * - Emitting the final stereo separation result
     *
     * The method uses the same model instance for both channels to maintain consistency
     * across the stereo separation. Progress is tracked separately for each channel
     * (0-50% for left, 50-100% for right).
     *
     * @param audioPath Path to the stereo input audio file (must be 2-channel WAV/MP3 format)
     * @param featureName Name of the feature file used for conditioning separation
     *                   (loaded from Constants::OUTPUT_FEATURES_DIR/featureName.txt)
     *
     * @note Input audio must be explicitly stereo (2 channels). Mono files are handled
     *       by processSingleFile and will not reach this method.
     * @note Each channel is processed completely independently, allowing for different
     *       separation behavior per channel based on the model's response.
     * @note Stereo reconstruction preserves channel ordering (left=0, right=1).
     * @note Processing errors in any channel result in termination without output.
     * @note The model is loaded once and unloaded after both channels complete.
     *
     * @see processSingleFile, processChannel, ZeroShotASPFeatureExtractor
     */
    void processSingleFileStereo(const QString& audioPath, const QString& featureName);

    /**
     * @brief Processes a single audio channel using overlapped chunk separation.
     *
     * This method implements the core audio separation algorithm for one audio channel
     * using a sliding window approach with overlap for memory-efficient neural network
     * processing. The method breaks down the mono audio channel into smaller chunks,
     * processes each chunk independently through the separation model, and reconstructs
     * the full separated audio using overlap-add synthesis.
     *
     * Processing pipeline:
     * - Validates input tensor shape (expects 1D audio waveform)
     * - Calculates chunk boundaries with configured overlap rate
     * - Pads final chunk if necessary to maintain consistent chunk sizes
     * - Reshapes each chunk to required model input dimensions (1, clipSamples, 1)
     * - Processes each chunk through the neural network model
     * - Collects processed chunk results in memory
     * - Performs overlap-add reconstruction to combine chunks
     * - Normalizes amplitude by overlap weights
     * - Trims or pads final result to match exact input length
     *
     * The overlap-add implementation uses linear ramps at chunk boundaries to minimize
     * amplitude discontinuities during reconstruction.
     *
     * @param channelWaveform 1D tensor (frames,) containing the mono audio channel samples
     * @param condition Feature conditioning tensor for separation (shape (1, feature_size))
     * @param channelFeatureName Identifier for this channel (used for error messages and potential future logging)
     * @param extractor Pointer to initialized ZeroShotASPFeatureExtractor instance
     * @param progressOffset Offset added to progress percentage (useful for multi-channel processing)
     * @return Processed channel tensor with shape (1, original_frames, 1). Returns empty tensor on any error.
     *
     * @note Input waveform must be 1D torch tensor with floating point samples.
     * @note Chunk size and overlap rate are determined by Constants::AUDIO_CLIP_SAMPLES and Constants::AUDIO_OVERLAP_RATE.
     * @note Progress updates are emitted incrementally during chunk processing (progressOffset to progressOffset+50 for full channel).
     * @note Memory usage scales with number of chunks; long audio files may require significant RAM.
     * @note Model inference errors cause immediate termination with error emission.
     * @note Overlap normalization prevents amplitude scaling artifacts in reconstructed audio.
     *
     * @see processSingleFileStereo, doOverlapAdd, ZeroShotASPFeatureExtractor::forward
     */
    torch::Tensor processChannel(const torch::Tensor& channelWaveform,
                                 const torch::Tensor& condition,
                                 const QString& channelFeatureName,
                                 ZeroShotASPFeatureExtractor* extractor,
                                 int progressOffset = 0);

    float m_overlapRate; ///< Overlap rate for chunk processing
    int m_clipSamples;   ///< Number of samples to clip
};
