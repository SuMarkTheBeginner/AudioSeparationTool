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
    torch::NoGradGuard no_grad;
    try {
        // 1️⃣ 先載入 TorchScript 模型
        model = torch::jit::load(modelPath.toStdString());
        model.eval();

        // 🚫 不使用 CUDA，強制 CPU
        model.to(torch::kCPU);
        qDebug() << "[HTSAT] Model loaded on CPU (forced)";

        modelLoaded = true;
        return true;
    }
    catch (const c10::Error& e) {
        emit errorOccurred(QString("Error loading model: %1").arg(e.what()));
        modelLoaded = false;
        return false;
    }
    catch (const std::exception& e) {
        emit errorOccurred(QString("Standard exception: %1").arg(e.what()));
        modelLoaded = false;
        return false;
    }
    catch (...) {
        emit errorOccurred("Unknown error loading model");
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
        qDebug() <<"?";
        
        emit errorOccurred("Model not loaded.");
        return {};
    }

    // Check for empty tensor
    if (audioTensor.numel() == 0) {
        qDebug() <<"?";
        emit errorOccurred("Input tensor is empty");
        return {};
    }

    // Check for NaN or infinite values
    if (!torch::isfinite(audioTensor).all().item<bool>()) {
        qDebug() <<"?";
        emit errorOccurred("Input tensor contains invalid values (NaN or infinite)");
        return {};
    }

    // 🚫 強制 CPU
    torch::Tensor tensor = audioTensor.squeeze(1).unsqueeze(0).to(torch::kCPU);

    qDebug() <<"reshape " << tensor.size(0) << "x" << tensor.size(1);
    std::vector<torch::jit::IValue> inputs = {tensor};

    torch::NoGradGuard no_grad;
    try {
        
        auto output_dict = model.forward(inputs).toGenericDict();
        printf("forward pass\n");
        torch::Tensor output = output_dict.at("latent_output").toTensor();

        // Output shape: (1, 2048)
        std::vector<float> embedding(output.data_ptr<float>(), output.data_ptr<float>() + output.numel());
        
        printf("output_dict pass\n");
        emit processingFinished(embedding);
        printf("output_dict emit pass\n");
        return embedding;
    } catch (const c10::Error& e) {
        printf("output_dict dead\n");
        
        emit errorOccurred(QString("Model inference error: %1").arg(e.what()));
        return {};
    } catch (const std::exception& e) {
        qDebug()<<"?"<<e.what();
        emit errorOccurred(QString("Processing error: %1").arg(e.what()));
        return {};
    }
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

    torch::NoGradGuard no_grad;
    try {
        model = torch::jit::load(tempFile.fileName().toStdString());
        model.eval();

        // 🚫 強制 CPU
        model.to(torch::kCPU);
        qDebug() << "[HTSAT] Model loaded on CPU (forced, from resource)";
        
        modelLoaded = true;
        
        QFile::remove(tempFile.fileName());
        
        
        return true;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Error loading model from resource: %1").arg(e.what()));
        modelLoaded = false;
        
        QFile::remove(tempFile.fileName());
        
        return false;
    }
}

bool HTSATProcessor::isModelLoaded() const
{
    return modelLoaded;
}
