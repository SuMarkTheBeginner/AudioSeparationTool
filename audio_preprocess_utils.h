#ifndef AUDIO_PREPROCESS_UTILS_H
#define AUDIO_PREPROCESS_UTILS_H

#include <torch/torch.h>
#include <QString>

/**
 * @brief Namespace for audio preprocessing utility functions.
 */
namespace AudioPreprocessUtils {

/**
 * @brief Loads audio data from a file path.
 * @param filePath The path to the audio file.
 * @return A tensor representing the audio samples.
 */
torch::Tensor loadAudio(const QString& filePath);

/**
 * @brief Normalizes audio data to a specified range.
 * @param audio The input audio tensor.
 * @param targetMax The maximum value for normalization (default 1.0).
 * @return Normalized audio tensor.
 */
torch::Tensor normalizeAudio(const torch::Tensor& audio, float targetMax = 1.0f);

/**
 * @brief Resamples audio to a target sample rate.
 * @param audio The input audio tensor.
 * @param originalSampleRate The original sample rate.
 * @param targetSampleRate The target sample rate.
 * @return Resampled audio tensor.
 */
torch::Tensor resampleAudio(const torch::Tensor& audio, int originalSampleRate, int targetSampleRate);

/**
 * @brief Converts stereo audio to mono by averaging channels.
 * @param audio The input stereo audio tensor (interleaved).
 * @param numChannels The number of channels.
 * @return Mono audio tensor.
 */
torch::Tensor convertToMono(const torch::Tensor& audio, int numChannels);

/**
 * @brief Extracts a specific channel from stereo audio.
 * @param audio The input stereo audio tensor (interleaved).
 * @param channelIndex The channel index (0 for left, 1 for right).
 * @param numChannels The number of channels (must be >= 2).
 * @return Audio tensor for the specified channel.
 */
torch::Tensor extractChannel(const torch::Tensor& audio, int channelIndex, int numChannels);

/**
 * @brief Trims silence from the beginning and end of audio.
 * @param audio The input audio tensor.
 * @param threshold The silence threshold.
 * @return Trimmed audio tensor.
 */
torch::Tensor trimSilence(const torch::Tensor& audio, float threshold = 0.01f);

/**
 * @brief Saves a torch::Tensor as a WAV file.
 * @param audio The audio tensor to save (1D or 2D).
 * @param filePath The path to save the WAV file.
 * @param sampleRate The sample rate for the WAV file (default 32000).
 * @return True if saving succeeded, false otherwise.
 */
bool saveToWav(const torch::Tensor& audio, const QString& filePath, int sampleRate = 32000);

/**
 * @brief Applies a Hann window to an audio chunk.
 * @param chunk The input audio chunk tensor (1D).
 * @return The chunk multiplied by the Hann window.
 */
torch::Tensor applyHannWindow(const torch::Tensor& chunk);

} // namespace AudioPreprocessUtils

#endif // AUDIO_PREPROCESS_UTILS_H
