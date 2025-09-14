#ifndef ZEROSHOTASPFEATUREEXTRACTOR_H
#define ZEROSHOTASPFEATUREEXTRACTOR_H

#include <QObject>
#include <QString>

#ifndef Q_MOC_RUN
#undef slots
#include <torch/script.h>
#define slots
#endif
#include <vector>

/**
 * @brief Class for handling ZeroShotASP (Zero-Shot Audio Separation) model processing.
 *
 * This class loads TorchScript models, processes audio waveforms with condition embeddings,
 * and generates separated audio waveforms using the ZeroShotASP model.
 */
class ZeroShotASPFeatureExtractor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the ZeroShotASPFeatureExtractor.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit ZeroShotASPFeatureExtractor(QObject *parent = nullptr);

    /**
     * @brief Loads the TorchScript model from the specified path.
     * @param modelPath Path to the TorchScript model file (e.g., "zero_shot_asp_separation_model.pt").
     * @return True if loading succeeded, false otherwise.
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief Processes waveform and condition tensors to generate separated audio.
     * @param waveform Input waveform tensor of shape (B, N, 1).
     * @param condition Condition embedding tensor of shape (B, 2048).
     * @param outputPath Optional output file path to save the separated audio as 32kHz WAV.
     * @return Separated waveform tensor of shape (B, N, 1), or empty tensor on failure.
     */
    torch::Tensor separateAudio(const torch::Tensor& waveform, const torch::Tensor& condition, const QString& outputPath = QString());

    /**
     * @brief Checks if the model is loaded and ready for inference.
     * @return True if model is loaded, false otherwise.
     */
    bool isModelLoaded() const;

    /**
     * @brief Saves a waveform tensor as a 32-bit float WAV file at the specified sample rate.
     * @param waveform Tensor of shape (N, 1) with float32 samples.
     * @param filePath Output file path.
     * @param sampleRate Sample rate in Hz (default 32000).
     * @return True if saving succeeded, false otherwise.
     */
    bool saveAsWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate = 32000);

signals:
    /**
     * @brief Emitted when an error occurs during processing.
     * @param errorMessage Description of the error.
     */
    void errorOccurred(const QString& errorMessage);

    /**
     * @brief Emitted when processing is complete.
     * @param separated Separated waveform tensor.
     */
    void processingFinished(const torch::Tensor& separated);

private:
    torch::jit::script::Module model; ///< The loaded TorchScript model
    bool modelLoaded;                 ///< Flag indicating if the model is loaded
};

#endif // ZEROSHOTASPFEATUREEXTRACTOR_H
