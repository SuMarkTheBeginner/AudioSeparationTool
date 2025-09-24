// ZeroShotASPFeatureExtractor.cpp
#include "zero_shot_asp_feature_extractor.h"
#include <QFileInfo>
#include <torch/script.h>
#include <QResource>
#include <QTemporaryFile>

ZeroShotASPFeatureExtractor::ZeroShotASPFeatureExtractor(QObject* parent)
    : QObject(parent), modelLoaded(false)
{
}

bool ZeroShotASPFeatureExtractor::loadModel(const QString& modelPath)
{
    QFileInfo fi(modelPath);
    if (!fi.exists()) {
        emit error("Model file does not exist: " + modelPath);
        return false;
    }

    torch::NoGradGuard no_grad;
    try {
        model = torch::jit::load(modelPath.toStdString());
        modelLoaded = true;
        return true;
    } catch (const c10::Error& e) {
        emit error("Failed to load model: " + QString::fromStdString(e.what()));
        modelLoaded = false;
        return false;
    }
}

torch::Tensor ZeroShotASPFeatureExtractor::forward(const torch::Tensor& waveform,
                                                    const torch::Tensor& condition)
{
    if (!modelLoaded) {
        emit error("Model not loaded");
        return torch::Tensor();
    }

    // Check input shapes
    if (waveform.dim() != 3 || waveform.size(0) != 1 || waveform.size(2) != 1) {
        emit error("Invalid waveform tensor shape");
        return torch::Tensor();
    }

    if (condition.dim() != 2 || condition.size(0) != 1 || condition.size(1) != 2048) {
        emit error("Invalid condition tensor shape");
        return torch::Tensor();
    }

    torch::NoGradGuard no_grad;
    try {
        std::vector<torch::jit::IValue> inputs = {waveform, condition};
        torch::Tensor output = model.forward(inputs).toTensor();
        emit finished(output);
        return output;
    } catch (const c10::Error& e) {
        emit error("Forward pass error: " + QString::fromStdString(e.what()));
        return torch::Tensor();
    }
}

void ZeroShotASPFeatureExtractor::unloadModel()
{
    model = torch::jit::script::Module();
    modelLoaded = false;
}

bool ZeroShotASPFeatureExtractor::loadModelFromResource(const QString& resourcePath)
{
    QResource resource(resourcePath);
    if (!resource.isValid()) {
        emit error("Invalid resource path: " + resourcePath);
        return false;
    }

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    
    if (!tempFile.open()) {
        emit error("Failed to create temporary file for model");
        return false;
    }

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

    torch::NoGradGuard no_grad;
    try {
        model = torch::jit::load(tempFile.fileName().toStdString());
        modelLoaded = true;
        
        QFile::remove(tempFile.fileName());
        
        qDebug() << "Successfully loaded ZeroShotASP model from resource:" << resourcePath;
        return true;
    } catch (const c10::Error& e) {
        emit error("Failed to load model: " + QString::fromStdString(e.what()));
        modelLoaded = false;
        
        QFile::remove(tempFile.fileName());
        
        return false;
    }
}
