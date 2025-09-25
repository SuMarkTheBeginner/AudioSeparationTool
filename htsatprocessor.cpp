#include "htsatprocessor.h"
#include <torch/script.h>
#include <torch/torch.h>
#include <QString>
#include <QDebug>
#include <QResource>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <optional>
#include <vector>
#include <string>

#include "constants.h"
// Constants for model configuration
namespace {
    const QString MODEL_CACHE_DIR = "models";
    const QString TEMP_MODEL_PREFIX = "htsat_model_";
    const torch::Device DEFAULT_DEVICE = torch::kCPU;
}

/**
 * @brief Constructs the HTSATProcessor.
 * @param parent The parent QObject (default is nullptr).
 */
HTSATProcessor::HTSATProcessor(QObject *parent)
    : QObject(parent)
    , device_(Constants::DEFAULT_DEVICE)
    , modelLoaded_(false)
{
}

/**
 * @brief Destroys the HTSATProcessor and cleans up resources.
 */
HTSATProcessor::~HTSATProcessor() = default;

/**
 * @brief Loads the TorchScript model from the specified path.
 * @param modelPath Path to the TorchScript model file.
 * @return True if loading succeeded, false otherwise.
 */
bool HTSATProcessor::loadModel(const QString& modelPath)
{
    if (modelPath.isEmpty()) {
        emit errorOccurred("Model path is empty");
        return false;
    }

    if (!QFile::exists(modelPath)) {
        emit errorOccurred(QString("Model file does not exist: %1").arg(modelPath));
        return false;
    }

    torch::NoGradGuard no_grad;
    try {
        model_ = torch::jit::load(modelPath.toStdString());
        model_.eval();
        model_.to(device_);

        modelLoaded_ = true;
        qDebug() << "[HTSAT] Model loaded successfully on" << device_.str().c_str();
        return true;
    }
    catch (const c10::Error& e) {
        emit errorOccurred(QString("Failed to load model from %1: %2").arg(modelPath, e.what()));
        modelLoaded_ = false;
        return false;
    }
    catch (const std::exception& e) {
        emit errorOccurred(QString("Standard exception loading model: %1").arg(e.what()));
        modelLoaded_ = false;
        return false;
    }
    catch (...) {
        emit errorOccurred("Unknown error occurred while loading model");
        modelLoaded_ = false;
        return false;
    }
}

/**
 * @brief Loads the TorchScript model from a resource path.
 * @param resourcePath Path to the resource containing the model.
 * @return True if loading succeeded, false otherwise.
 */
bool HTSATProcessor::loadModelFromResource(const QString& resourcePath)
{
    if (resourcePath.isEmpty()) {
        emit errorOccurred("Resource path is empty");
        return false;
    }

    QResource resource(resourcePath);
    if (!resource.isValid()) {
        emit errorOccurred(QString("Invalid resource path: %1").arg(resourcePath));
        return false;
    }

    // Create temporary file for the model
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + "/" + TEMP_MODEL_PREFIX + "XXXXXX.pt");
    tempFile.setAutoRemove(false);

    if (!tempFile.open()) {
        emit errorOccurred("Failed to create temporary file for model");
        return false;
    }

    // Write resource data to temporary file
    QByteArray data = resource.uncompressedData();
    if (data.isEmpty()) {
        emit errorOccurred("Resource data is empty");
        return false;
    }

    qint64 bytesWritten = tempFile.write(data);
    if (bytesWritten != data.size()) {
        emit errorOccurred("Failed to write complete model data to temporary file");
        tempFile.close();
        return false;
    }

    tempFile.close();

    // Load the model from temporary file
    bool success = loadModel(tempFile.fileName());

    // Clean up temporary file
    QFile::remove(tempFile.fileName());

    if (success) {
        qDebug() << "[HTSAT] Model loaded successfully from resource";
    }

    return success;
}

/**
 * @brief Processes a preprocessed audio tensor to generate structured outputs.
 * @param audioTensor The preprocessed audio tensor (shape: [batch_size, num_classes]).
 * @return Optional containing the structured outputs, or nullopt on failure.
 */
std::optional<HTSATOutput> HTSATProcessor::process(const torch::Tensor& audioTensor)
{
    if (!modelLoaded_) {
        emit errorOccurred("Model not loaded");
        return std::nullopt;
    }

    // Validate input tensor
    if (audioTensor.numel() == 0) {
        emit errorOccurred("Input tensor is empty");
        return std::nullopt;
    }

    if (audioTensor.dim() != 2) {
        emit errorOccurred("Input tensor must be 2-dimensional [batch_size, num_classes]");
        return std::nullopt;
    }

    if (!torch::isfinite(audioTensor).all().item<bool>()) {
        emit errorOccurred("Input tensor contains invalid values (NaN or infinite)");
        return std::nullopt;
    }

    // Prepare input tensor for inference
    torch::Tensor inputTensor = prepareInputTensor(audioTensor);
    std::vector<torch::jit::IValue> inputs = {inputTensor};

    torch::NoGradGuard no_grad;
    try {
        torch::IValue output = model_.forward(inputs);

        if (!validateModelOutput(output)) {
            emit errorOccurred("Model output validation failed");
            return std::nullopt;
        }

        HTSATOutput structuredOutput = extractStructuredOutput(output);
        return structuredOutput;
    }
    catch (const c10::Error& e) {
        emit errorOccurred(QString("Model inference error: %1").arg(e.what()));
        return std::nullopt;
    }
    catch (const std::exception& e) {
        emit errorOccurred(QString("Processing error: %1").arg(e.what()));
        return std::nullopt;
    }
    catch (...) {
        emit errorOccurred("Unknown error occurred during processing");
        return std::nullopt;
    }
}

/**
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool HTSATProcessor::isModelLoaded() const
{
    return modelLoaded_;
}

/**
 * @brief Gets the device the model is running on.
 * @return The device (CPU or CUDA).
 */
torch::Device HTSATProcessor::getDevice() const
{
    return device_;
}

/**
 * @brief Validates the model output structure.
 * @param output The output dictionary from the model.
 * @return True if the output structure is valid, false otherwise.
 */
bool HTSATProcessor::validateModelOutput(const torch::IValue& output) const
{
    if (!output.isGenericDict()) {
        return false;
    }

    auto outputDict = output.toGenericDict();
    const std::vector<std::string> requiredKeys = {"framewise_output", "clipwise_output", "latent_output"};

    for (const auto& key : requiredKeys) {
        if (outputDict.find(key) == outputDict.end()) {
            return false;
        }

        torch::Tensor tensor = outputDict.at(key).toTensor();
        if (tensor.numel() == 0 || !torch::isfinite(tensor).all().item<bool>()) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Prepares the input tensor for model inference.
 * @param audioTensor The input audio tensor.
 * @return The prepared tensor ready for inference.
 */
torch::Tensor HTSATProcessor::prepareInputTensor(const torch::Tensor& audioTensor) const
{
    // Ensure tensor is on the correct device and has the right shape
    torch::Tensor tensor = audioTensor.to(device_);

    // Model expects [batch_size, num_classes] but we might need to adjust
    if (tensor.dim() == 1) {
        tensor = tensor.unsqueeze(0);  // Add batch dimension
    }

    return tensor;
}

/**
 * @brief Extracts structured outputs from the model result.
 * @param output The model output dictionary.
 * @return The structured outputs.
 */
HTSATOutput HTSATProcessor::extractStructuredOutput(const torch::IValue& output) const
{
    auto outputDict = output.toGenericDict();

    HTSATOutput structuredOutput;
    structuredOutput.framewise_output = outputDict.at("framewise_output").toTensor();
    structuredOutput.clipwise_output = outputDict.at("clipwise_output").toTensor();
    structuredOutput.latent_output = outputDict.at("latent_output").toTensor();

    return structuredOutput;
}
