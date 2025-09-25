#include "zero_shot_asp_feature_extractor.h"
#include "constants.h"
#include <QFileInfo>
#include <QResource>
#include <QTemporaryFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <memory>
#include <optional>
#include <vector>
#include <string>

// Constants for model configuration
namespace {
    const QString TEMP_MODEL_PREFIX = "asp_model_";
    const int64_t EXPECTED_CONDITION_DIM = 2048;
    const int64_t EXPECTED_WAVEFORM_CHANNELS = 1;
    const torch::Device DEFAULT_DEVICE = torch::kCPU;
}

/**
 * @brief Constructs the ZeroShotASPFeatureExtractor.
 * @param parent The parent QObject (default is nullptr).
 */
ZeroShotASPFeatureExtractor::ZeroShotASPFeatureExtractor(QObject* parent)
    : QObject(parent)
    , device_(Constants::DEFAULT_DEVICE)
    , modelLoaded_(false)
{
}

/**
 * @brief Destroys the ZeroShotASPFeatureExtractor and cleans up resources.
 */
ZeroShotASPFeatureExtractor::~ZeroShotASPFeatureExtractor() = default;

/**
 * @brief Loads the TorchScript model from the specified path.
 * @param modelPath Path to the model file.
 * @return True if loaded successfully, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::loadModel(const QString& modelPath)
{
    if (modelPath.isEmpty()) {
        emit error("Model path is empty");
        return false;
    }

    QFileInfo fileInfo(modelPath);
    if (!fileInfo.exists()) {
        emit error(QString("Model file does not exist: %1").arg(modelPath));
        return false;
    }

    if (!fileInfo.isReadable()) {
        emit error(QString("Model file is not readable: %1").arg(modelPath));
        return false;
    }

    torch::NoGradGuard no_grad;
    try {
        model_ = torch::jit::load(modelPath.toStdString());
        model_.eval();
        model_.to(device_);

        modelLoaded_ = true;
        qDebug() << "[ZeroShotASP] Model loaded successfully on" << device_.str().c_str();
        return true;
    }
    catch (const c10::Error& e) {
        emit error(QString("Failed to load model from %1: %2").arg(modelPath, e.what()));
        modelLoaded_ = false;
        return false;
    }
    catch (const std::exception& e) {
        emit error(QString("Standard exception loading model: %1").arg(e.what()));
        modelLoaded_ = false;
        return false;
    }
    catch (...) {
        emit error("Unknown error occurred while loading model");
        modelLoaded_ = false;
        return false;
    }
}

/**
 * @brief Performs forward pass through the model with structured input/output.
 * @param input The structured model input containing waveform and condition.
 * @return Optional containing the structured output, or nullopt on failure.
 */
std::optional<ASPModelOutput> ZeroShotASPFeatureExtractor::process(const ASPModelInput& input)
{
    if (!modelLoaded_) {
        emit error("Model not loaded");
        return std::nullopt;
    }

    // Validate input
    if (!validateInput(input)) {
        return std::nullopt;
    }

    // Prepare input for inference
    ASPModelInput preparedInput = prepareInput(input);
    std::vector<torch::jit::IValue> inputs = {preparedInput.waveform, preparedInput.condition};

    torch::NoGradGuard no_grad;
    try {
        torch::Tensor output = model_.forward(inputs).toTensor();

        if (!validateOutput(output)) {
            emit error("Model output validation failed");
            return std::nullopt;
        }

        ASPModelOutput structuredOutput = extractStructuredOutput(output);
        emit finished(structuredOutput);
        return structuredOutput;
    }
    catch (const c10::Error& e) {
        emit error(QString("Model inference error: %1").arg(e.what()));
        return std::nullopt;
    }
    catch (const std::exception& e) {
        emit error(QString("Processing error: %1").arg(e.what()));
        return std::nullopt;
    }
    catch (...) {
        emit error("Unknown error occurred during processing");
        return std::nullopt;
    }
}

/**
 * @brief Unloads the model to free memory.
 */
void ZeroShotASPFeatureExtractor::unloadModel()
{
    model_ = torch::jit::script::Module();
    modelLoaded_ = false;
    qDebug() << "[ZeroShotASP] Model unloaded";
}

/**
 * @brief Loads the model from a resource path.
 * @param resourcePath Path to the resource containing the model.
 * @return True if loaded successfully, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::loadModelFromResource(const QString& resourcePath)
{
    if (resourcePath.isEmpty()) {
        emit error("Resource path is empty");
        return false;
    }

    QResource resource(resourcePath);
    if (!resource.isValid()) {
        emit error(QString("Invalid resource path: %1").arg(resourcePath));
        return false;
    }

    // Create temporary file for the model
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + "/" + TEMP_MODEL_PREFIX + "XXXXXX.pt");
    tempFile.setAutoRemove(false);

    if (!tempFile.open()) {
        emit error("Failed to create temporary file for model");
        return false;
    }

    // Write resource data to temporary file
    QByteArray data = resource.uncompressedData();
    if (data.isEmpty()) {
        emit error("Resource data is empty");
        return false;
    }

    qint64 bytesWritten = tempFile.write(data);
    if (bytesWritten != data.size()) {
        emit error("Failed to write complete model data to temporary file");
        tempFile.close();
        return false;
    }

    tempFile.close();

    // Load the model from temporary file
    bool success = loadModel(tempFile.fileName());

    // Clean up temporary file
    QFile::remove(tempFile.fileName());

    if (success) {
        qDebug() << "[ZeroShotASP] Model loaded successfully from resource";
    }

    return success;
}

/**
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::isModelLoaded() const
{
    return modelLoaded_;
}

/**
 * @brief Gets the device the model is running on.
 * @return The device (CPU or CUDA).
 */
torch::Device ZeroShotASPFeatureExtractor::getDevice() const
{
    return device_;
}

/**
 * @brief Validates the model input structure.
 * @param input The input to validate.
 * @return True if input is valid, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::validateInput(const ASPModelInput& input) const
{
    // Validate waveform tensor
    if (input.waveform.numel() == 0) {
        emit error("Waveform tensor is empty");
        return false;
    }

    if (input.waveform.dim() != 3) {
        emit error("Waveform tensor must be 3-dimensional (B, T, C)");
        return false;
    }

    if (input.waveform.size(2) != EXPECTED_WAVEFORM_CHANNELS) {
        emit error(QString("Waveform tensor must have %1 channel(s), got %2")
                  .arg(EXPECTED_WAVEFORM_CHANNELS).arg(input.waveform.size(2)));
        return false;
    }

    // Validate condition tensor
    if (input.condition.numel() == 0) {
        emit error("Condition tensor is empty");
        return false;
    }

    if (input.condition.dim() != 2) {
        emit error("Condition tensor must be 2-dimensional (B, embedding_dim)");
        return false;
    }

    if (input.condition.size(1) != EXPECTED_CONDITION_DIM) {
        emit error(QString("Condition tensor must have embedding dimension %1, got %2")
                  .arg(EXPECTED_CONDITION_DIM).arg(input.condition.size(1)));
        return false;
    }

    // Validate batch size consistency
    if (input.waveform.size(0) != input.condition.size(0)) {
        emit error("Batch size mismatch between waveform and condition tensors");
        return false;
    }

    // Validate tensor data
    if (!torch::isfinite(input.waveform).all().item<bool>()) {
        emit error("Waveform tensor contains invalid values (NaN or infinite)");
        return false;
    }

    if (!torch::isfinite(input.condition).all().item<bool>()) {
        emit error("Condition tensor contains invalid values (NaN or infinite)");
        return false;
    }

    return true;
}

/**
 * @brief Prepares the input tensors for model inference.
 * @param input The input to prepare.
 * @return The prepared input ready for inference.
 */
ASPModelInput ZeroShotASPFeatureExtractor::prepareInput(const ASPModelInput& input) const
{
    ASPModelInput preparedInput;

    // Ensure tensors are on the correct device
    preparedInput.waveform = input.waveform.to(device_);
    preparedInput.condition = input.condition.to(device_);

    return preparedInput;
}

/**
 * @brief Validates the model output structure.
 * @param output The output to validate.
 * @return True if output is valid, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::validateOutput(const torch::Tensor& output) const
{
    if (output.numel() == 0) {
        emit error("Model output tensor is empty");
        return false;
    }

    if (output.dim() != 3) {
        emit error("Model output tensor must be 3-dimensional (B, T, C)");
        return false;
    }

    if (!torch::isfinite(output).all().item<bool>()) {
        emit error("Model output tensor contains invalid values (NaN or infinite)");
        return false;
    }

    return true;
}

/**
 * @brief Extracts structured output from the model result.
 * @param output The raw model output tensor.
 * @return The structured output.
 */
ASPModelOutput ZeroShotASPFeatureExtractor::extractStructuredOutput(const torch::Tensor& output) const
{
    ASPModelOutput structuredOutput;
    structuredOutput.wav_out = output;
    return structuredOutput;
}
