#pragma once

#ifndef Q_MOC_RUN
#undef slots
#endif
#include <torch/script.h>
#include <torch/autograd.h>
#ifndef Q_MOC_RUN
#define slots
#endif
#include <QString>
#include <QObject>
#include <memory>
#include <optional>

/**
 * @brief Structure containing the model input specification.
 */
struct ASPModelInput {
    torch::Tensor waveform;  // (B, T, C) waveform - batch_size, time_steps, channels
    torch::Tensor condition; // (B, 2048) embedding - batch_size, embedding_dim
};

/**
 * @brief Structure containing the model output specification.
 */
struct ASPModelOutput {
    torch::Tensor wav_out;  // (B, T, C) separated waveform
};

/**
 * @brief Feature extractor for zero-shot audio source separation using TorchScript model.
 *
 * This class provides a clean interface for loading and running PyTorch models for audio
 * source separation. It supports loading models from both file paths and Qt resources,
 * with comprehensive error handling and input validation.
 *
 * Model I/O specification:
 * - Input: waveform (B, T, C), condition (B, 2048)
 * - Output: separated waveform (B, T, C)
 */
class ZeroShotASPFeatureExtractor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the ZeroShotASPFeatureExtractor.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit ZeroShotASPFeatureExtractor(QObject* parent = nullptr);

    /**
     * @brief Destroys the ZeroShotASPFeatureExtractor and cleans up resources.
     */
    ~ZeroShotASPFeatureExtractor() override;

    /**
     * @brief Loads the TorchScript model from the specified path.
     * @param modelPath Path to the model file.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief Performs forward pass through the model with structured input/output.
     * @param input The structured model input containing waveform and condition.
     * @return Optional containing the structured output, or nullopt on failure.
     */
    std::optional<ASPModelOutput> process(const ASPModelInput& input);

    /**
     * @brief Unloads the model to free memory.
     */
    void unloadModel();

    /**
     * @brief Loads the model from a resource path.
     * @param resourcePath Path to the resource containing the model.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadModelFromResource(const QString& resourcePath);

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
     * @brief Signal emitted when processing is finished.
     * @param output The structured model output.
     */
    void finished(const ASPModelOutput& output);

    /**
     * @brief Signal emitted when an error occurs.
     * @param errorMessage Description of the error.
     */
    void error(const QString& errorMessage) const;

private:
    /**
     * @brief Validates the model input structure.
     * @param input The input to validate.
     * @return True if input is valid, false otherwise.
     */
    bool validateInput(const ASPModelInput& input) const;

    /**
     * @brief Prepares the input tensors for model inference.
     * @param input The input to prepare.
     * @return The prepared input ready for inference.
     */
    ASPModelInput prepareInput(const ASPModelInput& input) const;

    /**
     * @brief Validates the model output structure.
     * @param output The output to validate.
     * @return True if output is valid, false otherwise.
     */
    bool validateOutput(const torch::Tensor& output) const;

    /**
     * @brief Extracts structured output from the model result.
     * @param output The raw model output tensor.
     * @return The structured output.
     */
    ASPModelOutput extractStructuredOutput(const torch::Tensor& output) const;

    // Constants
    static constexpr int64_t EXPECTED_CONDITION_DIM = 2048;
    static constexpr int64_t EXPECTED_WAVEFORM_CHANNELS = 1;

    torch::jit::script::Module model_;  // The loaded TorchScript model
    torch::Device device_;              // The device the model runs on
    bool modelLoaded_;                  // Flag indicating if the model is loaded
};
