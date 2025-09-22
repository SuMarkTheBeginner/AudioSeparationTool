#include "htsatprocessor.h"
#include "constants.h"
#include <torch/script.h>
#include <torch/torch.h>
#include <QString>
#include <QDebug>
#include <QResource>
#include <QTemporaryFile>
#include <QFile>
#include <sndfile.h>
#include <samplerate.h>

#include <QDir>

/**
 * @brief Constructs the HTSATProcessor.
 * @param parent The parent QObject (default is nullptr).
 */
HTSATProcessor::HTSATProcessor(QObject *parent)
    : QObject(parent), modelLoaded(false)
{
}

/**
 * @brief Loads the TorchScript model from the specified path.
 * @param modelPath Path to the TorchScript model file (e.g., "htsat_embedding_model.pt").
 * @return True if loading succeeded, false otherwise.
 */
bool HTSATProcessor::loadModel(const QString& modelPath)
{
    // 轉成 native Windows 路徑（反斜線）
    QString nativePath = QDir::toNativeSeparators(modelPath);

    // 用 UTF-8 輸出路徑字串，避免編碼錯誤
    std::string utf8Path = nativePath.toUtf8().constData();

    // Debug 確認
    qDebug() << "Trying to load model from:" << nativePath;

    // 用 ifstream 開啟檔案 (binary mode)
    std::ifstream f(utf8Path, std::ios::binary);
    if (!f.is_open()) {
        qDebug() << "Model file not found or cannot open:" << nativePath;
        return false;
    }

    try {
        model = torch::jit::load(f);
        model.eval();
        modelLoaded = true;
        qDebug() << "Model loaded successfully!";
        return true;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Error loading model: %1").arg(e.what()));
        qDebug() << "Error loading model:" << e.what();
        modelLoaded = false;
        return false;
    }
}

/**
 * @brief Processes a preprocessed audio tensor to generate an embedding.
 * @param audioTensor The preprocessed audio tensor (mono, 32kHz).
 * @return The generated embedding as a vector of floats, or empty vector on failure.
 */
std::vector<float> HTSATProcessor::processTensor(const torch::Tensor& audioTensor)
{
    if (!modelLoaded) {
        qDebug() << "HTSATProcessor::processTensor - Model not loaded";
        emit errorOccurred("Model not loaded.");
        return {};
    }

    // Enhanced logging for debugging
    qDebug() << "HTSATProcessor::processTensor - Input tensor info:";
    qDebug() << "  - Shape: [" << audioTensor.size(0) << "," << audioTensor.size(1) << "]";
    auto dtype = audioTensor.dtype();
    QString dtypeName;

    if (dtype == torch::kFloat32) dtypeName = "float32";
    else if (dtype == torch::kFloat64) dtypeName = "float64";
    else if (dtype == torch::kInt32) dtypeName = "int32";
    else if (dtype == torch::kInt64) dtypeName = "int64";
    else dtypeName = "unknown";

    qDebug() << "  - Dtype:" << dtypeName;
    c10::DeviceType dev_type = audioTensor.device().type();
    QString dev_name = QString::fromStdString(c10::DeviceTypeName(dev_type));
    qDebug() << "  - Device:" << dev_name;
    qDebug() << "  - Numel:" << audioTensor.numel();

    // Check for empty tensor
    if (audioTensor.numel() == 0) {
        qDebug() << "HTSATProcessor::processTensor - Empty input tensor";
        emit errorOccurred("Input tensor is empty");
        return {};
    }

    // Check for invalid values
    auto minVal = audioTensor.min().item<float>();
    auto maxVal = audioTensor.max().item<float>();
    auto meanVal = audioTensor.mean().item<float>();

    qDebug() << "HTSATProcessor::processTensor - Value range:";
    qDebug() << "  - Min:" << minVal;
    qDebug() << "  - Max:" << maxVal;
    qDebug() << "  - Mean:" << meanVal;

    // Check for NaN or infinite values
    if (!torch::isfinite(audioTensor).all().item<bool>()) {
        qDebug() << "HTSATProcessor::processTensor - Tensor contains NaN or infinite values";
        emit errorOccurred("Input tensor contains invalid values (NaN or infinite)");
        return {};
    }

    // The model expects input shape (B, T) with T = AUDIO_CLIP_SAMPLES (10 seconds at 32kHz)
    const int64_t expectedLength = Constants::AUDIO_CLIP_SAMPLES;

    // Assume audioTensor is (frames, channels), convert to (1, frames) assuming mono
    torch::Tensor reshaped = audioTensor.squeeze(1).unsqueeze(0); // (1, frames)

    qDebug() << "HTSATProcessor::processTensor - After reshaping:";
    qDebug() << "  - Shape: [" << reshaped.size(0) << "," << reshaped.size(1) << "]";

    torch::Tensor tensor;

    if (reshaped.size(1) == expectedLength) {
        tensor = reshaped;
        qDebug() << "HTSATProcessor::processTensor - Using tensor as-is (correct length)";
    } else if (reshaped.size(1) < expectedLength) {
        // Pad with zeros
        int64_t paddingSize = expectedLength - reshaped.size(1);
        tensor = torch::constant_pad_nd(reshaped, {0, paddingSize}, 0);
        qDebug() << "HTSATProcessor::processTensor - Padded tensor with" << paddingSize << "zeros";
    } else {
        // Truncate
        tensor = reshaped.narrow(1, 0, expectedLength);
        qDebug() << "HTSATProcessor::processTensor - Truncated tensor to" << expectedLength << "samples";
    }

    qDebug() << "HTSATProcessor::processTensor - Final tensor shape: [" << tensor.size(0) << "," << tensor.size(1) << "]";

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor);

    try {
        qDebug() << "HTSATProcessor::processTensor - Starting model inference...";
        auto output_dict = model.forward(inputs).toGenericDict();
        torch::Tensor output = output_dict.at("latent_output").toTensor();

        qDebug() << "HTSATProcessor::processTensor - Model inference successful";
        qDebug() << "HTSATProcessor::processTensor - Output shape: [" << output.size(0) << "," << output.size(1) << "]";

        // Output shape: (1, 2048)
        std::vector<float> embedding(output.data_ptr<float>(), output.data_ptr<float>() + output.numel());
        qDebug() << "HTSATProcessor::processTensor - Embedding size:" << embedding.size();

        emit processingFinished(embedding);
        return embedding;
    } catch (const c10::Error& e) {
        qDebug() << "HTSATProcessor::processTensor - Model inference error:" << e.what();
        emit errorOccurred(QString("Model inference error: %1").arg(e.what()));
        return {};
    } catch (const std::exception& e) {
        qDebug() << "HTSATProcessor::processTensor - Standard exception:" << e.what();
        emit errorOccurred(QString("Processing error: %1").arg(e.what()));
        return {};
    }
}

/**
 * @brief Processes an audio file to generate an embedding.
 * @param audioPath Path to the audio file (WAV format).
 * @return The generated embedding as a vector of floats, or empty vector on failure.
 */
std::vector<float> HTSATProcessor::processAudio(const QString& audioPath)
{
    // Deprecated: Use processTensor instead
    emit errorOccurred("processAudio is deprecated. Use processTensor instead.");
    return {};
}

/**
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool HTSATProcessor::isModelLoaded() const
{
    return modelLoaded;
}


bool HTSATProcessor::loadModelFromResource(const QString& resourcePath)
{
    QResource resource(resourcePath);
    if (!resource.isValid()) {
        emit errorOccurred(QString("Invalid resource path: %1").arg(resourcePath));
        return false;
    }

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    
    if (!tempFile.open()) {
        emit errorOccurred("Failed to create temporary file for model");
        return false;
    }

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

    try {
        model = torch::jit::load(tempFile.fileName().toStdString());
        model.eval();
        modelLoaded = true;
        
        QFile::remove(tempFile.fileName());
        
        qDebug() << "Successfully loaded model from resource:" << resourcePath;
        return true;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Error loading model from resource: %1").arg(e.what()));
        modelLoaded = false;
        
        QFile::remove(tempFile.fileName());
        
        return false;
    }
}
