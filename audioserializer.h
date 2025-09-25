#ifndef AUDIOSERIALIZER_H
#define AUDIOSERIALIZER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <vector>

#ifndef Q_MOC_RUN
#undef slots
#include <torch/torch.h>
#define slots
#endif

/**
 * @brief Handles audio serialization operations (WAV files and embeddings).
 *
 * Provides clean, focused methods for saving/loading audio data and embeddings.
 * Separates tensor manipulation logic from the main business logic.
 */
class AudioSerializer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor.
     * @param parent Optional parent QObject.
     */
    explicit AudioSerializer(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AudioSerializer();

    /**
     * @brief Save a waveform tensor as a WAV file.
     * @param waveform Torch tensor containing audio samples (supports various shapes).
     * @param filePath Output file path for the WAV file.
     * @param sampleRate Sample rate in Hz (default: 32000).
     * @return True if saving succeeded, false otherwise.
     */
    bool saveWavToFile(const torch::Tensor& waveform, const QString& filePath, int sampleRate = 32000);

    /**
     * @brief Load a waveform tensor from a WAV file.
     * @param filePath Path to the WAV file to load.
     * @return Loaded waveform tensor, or empty tensor on failure.
     */
    torch::Tensor loadWavFromFile(const QString& filePath);

    /**
     * @brief Save embedding vectors to a text file.
     * @param embedding Vector of float values representing the embedding.
     * @param filePath Output file path for the embedding file.
     * @return True if saving succeeded, false otherwise.
     */
    bool saveEmbeddingToFile(const std::vector<float>& embedding, const QString& filePath);

    /**
     * @brief Load embedding vectors from a text file.
     * @param filePath Path to the embedding file to load.
     * @return Vector of embedding values, or empty vector on failure.
     */
    std::vector<float> loadEmbeddingFromFile(const QString& filePath);

private:
    /**
     * @brief Normalize tensor shape for WAV writing.
     *
     * Ensures the tensor is in the correct format (channels, samples).
     * @param waveform Input tensor to normalize.
     * @return Normalized tensor suitable for WAV writing.
     */
    torch::Tensor normalizeTensorShape(const torch::Tensor& waveform);

    /**
     * @brief Write WAV file header.
     * @param file Open file handle to write to.
     * @param channels Number of audio channels.
     * @param sampleRate Sample rate in Hz.
     * @param numSamples Total number of samples.
     * @return True if header written successfully.
     */
    bool writeWavHeader(QFile& file, short channels, int sampleRate, int64_t numSamples);

    /**
     * @brief Write WAV audio data.
     * @param file Open file handle to write to.
     * @param data Pointer to float audio data.
     * @param numSamples Number of samples to write.
     * @return True if data written successfully.
     */
    bool writeWavData(QFile& file, const float* data, int64_t numSamples);
};

#endif // AUDIOSERIALIZER_H
