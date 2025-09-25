#ifndef HTSATPROCESSOR_H
#define HTSATPROCESSOR_H

#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#ifndef Q_MOC_RUN
#undef slots
#include <torch/script.h>
#include <torch/autograd.h>
#define slots
#endif

/**
 * @brief Structure containing HTSAT model inputs.
 */
struct HTSATInput {
    torch::Tensor audio_tensor;  // [batch_size, num_samples] input tensor
};

/**
 * @brief Structure containing HTSAT model outputs.
 */
struct HTSATOutput {
    torch::Tensor framewise_output;  // [batch_size, num_classes, time_steps]
    torch::Tensor clipwise_output;   // [batch_size, num_classes]
    torch::Tensor latent_output;     // [batch_size, hidden_dim]
};

/**
 * @brief Class for handling HTSAT (Hierarchical Token-Semantic Audio Transformer) model processing.
 *
 * This class loads TorchScript models and processes preprocessed audio tensors to generate
 * structured outputs including framewise, clipwise, and latent representations.
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
     * @brief Destroys the HTSATProcessor and cleans up resources.
     */
    ~HTSATProcessor() override;

    /**
     * @brief Loads the TorchScript model from the specified path.
     * @param modelPath Path to the TorchScript model file.
     * @return True if loading succeeded, false otherwise.
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief Loads the TorchScript model from a resource path.
     * @param resourcePath Path to the resource containing the model.
     * @return True if loading succeeded, false otherwise.
     */
    bool loadModelFromResource(const QString& resourcePath);

    /**
     * @brief Processes a preprocessed audio tensor to generate structured outputs.
     * @param audioTensor The preprocessed audio tensor (shape: [batch_size, num_classes]).
     * @return Optional containing the structured outputs, or nullopt on failure.
     */
    std::optional<HTSATOutput> process(const torch::Tensor& audioTensor);

    /**
     * @brief Checks if the model is loaded and ready for inference.
     * @return True if model is loaded, false otherwise.
     */
    bool isModelLoaded() const;

    /**
     * @brief Gets the device the model is running on.
     * @return The device (CPU or CUDA).
     */
    torch::Device getDevice() const;

signals:
    /**
     * @brief Emitted when an error occurs during processing.
     * @param errorMessage Description of the error.
     */
    void errorOccurred(const QString& errorMessage);
    
private:
    /**
     * @brief Validates the model output structure.
     * @param output The output dictionary from the model.
     * @return True if the output structure is valid, false otherwise.
     */
    bool validateModelOutput(const torch::IValue& output) const;

    /**
     * @brief Prepares the input tensor for model inference.
     * @param audioTensor The input audio tensor.
     * @return The prepared tensor ready for inference.
     */
    torch::Tensor prepareInputTensor(const torch::Tensor& audioTensor) const;

    /**
     * @brief Extracts structured outputs from the model result.
     * @param output The model output dictionary.
     * @return The structured outputs.
     */
    HTSATOutput extractStructuredOutput(const torch::IValue& output) const;

    torch::jit::script::Module model_;  // The loaded TorchScript model
    torch::Device device_;              // The device the model runs on
    bool modelLoaded_;                  // Flag indicating if the model is loaded
};

#endif // HTSATPROCESSOR_H
