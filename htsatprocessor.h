#ifndef HTSATPROCESSOR_H
#define HTSATPROCESSOR_H

#include <QObject>
#include <QString>

#ifndef Q_MOC_RUN
#undef slots
#include <torch/script.h>
#define slots
#endif
#include <vector>

/**
 * @brief Class for handling HTSAT (Hierarchical Token-Semantic Audio Transformer) model processing.
 *
 * This class loads TorchScript models, processes audio files by resampling to 32kHz,
 * prepares tensors, and generates embeddings using the HTSAT model.
 */
class HTSATProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the HTSATProcessor.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit HTSATProcessor(QObject *parent = nullptr);

    /**
     * @brief Loads the TorchScript model from the specified path.
     * @param modelPath Path to the TorchScript model file (e.g., "htsat_embedding_model.pt").
     * @return True if loading succeeded, false otherwise.
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief Processes an audio file to generate an embedding.
     * @param audioPath Path to the audio file (WAV format).
     * @return The generated embedding as a vector of floats, or empty vector on failure.
     */
    std::vector<float> processAudio(const QString& audioPath);

    /**
     * @brief Checks if the model is loaded and ready for inference.
     * @return True if model is loaded, false otherwise.
     */
    bool isModelLoaded() const;

    /**
     * @brief Reads and resamples audio file to 32kHz.
     * @param audioPath Path to the audio file.
     * @return Resampled audio data as vector of floats.
     */
    std::vector<float> readAndResampleAudio(const QString& audioPath);

signals:
    /**
     * @brief Emitted when an error occurs during processing.
     * @param errorMessage Description of the error.
     */
    void errorOccurred(const QString& errorMessage);

    /**
     * @brief Emitted when processing is complete.
     * @param embedding The generated embedding.
     */
    void processingFinished(const std::vector<float>& embedding);

private:
    torch::jit::script::Module model; ///< The loaded TorchScript model
    bool modelLoaded;                 ///< Flag indicating if the model is loaded

    /**
     * @brief Prepares the tensor for model input.
     * @param audioData The resampled audio data.
     * @return The prepared tensor.
     */
    torch::Tensor prepareTensor(const std::vector<float>& audioData);
};

#endif // HTSATPROCESSOR_H
