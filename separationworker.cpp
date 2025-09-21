// separationworker.cpp
#include "separationworker.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <torch/torch.h>
#include <cmath>
#include "audio_preprocess_utils.h"

SeparationWorker::SeparationWorker(QObject* parent)
    : QObject(parent),
      overlapRate(Constants::AUDIO_OVERLAP_RATE),
      clipSamples(Constants::AUDIO_CLIP_SAMPLES)
{
}

torch::Tensor SeparationWorker::loadFeature(const QString& featurePath)
{
    QFileInfo fi(featurePath);
    if (!fi.exists() || !fi.isReadable()) {
        emit error(QString("Feature file does not exist or is not readable: %1").arg(featurePath));
        return torch::Tensor();
    }

    QFile file(featurePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QString("Failed to open feature file: %1").arg(featurePath));
        return torch::Tensor();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    QStringList parts = content.split(' ', Qt::SkipEmptyParts);
    std::vector<float> values;
    bool ok = true;
    for (const QString& part : parts) {
        float val = part.toFloat(&ok);
        if (!ok) {
            emit error(QString("Invalid float value in feature file: %1").arg(part));
            return torch::Tensor();
        }
        values.push_back(val);
    }

    if (values.empty()) {
        emit error("Feature file is empty or invalid format");
        return torch::Tensor();
    }

    torch::Tensor tensor = torch::from_blob(values.data(), {(int64_t)values.size()}, torch::kFloat).clone();
    tensor = tensor.unsqueeze(0);  // Reshape to (1, feature_size)
    return tensor;
}

torch::Tensor SeparationWorker::processChunk(const torch::Tensor& waveform,
                                             const torch::Tensor& condition,
                                             ZeroShotASPFeatureExtractor* extractor)
{
    if (!extractor) {
        emit error("Extractor is not initialized");
        return torch::Tensor();
    }

    if (waveform.dim() != 3 || waveform.size(0) != 1 || waveform.size(2) != 1 || waveform.size(1) != clipSamples) {
        emit error("Invalid waveform shape for processChunk");
        return torch::Tensor();
    }

    if (condition.dim() != 2 || condition.size(0) != 1) {
        emit error("Invalid condition shape for processChunk");
        return torch::Tensor();
    }

    try {
        torch::Tensor output = extractor->forward(waveform, condition);
        return output;
    } catch (const c10::Error& e) {
        emit error(QString("Extractor forward error: %1").arg(e.what()));
        return torch::Tensor();
    }
}

torch::Tensor SeparationWorker::doOverlapAdd(const std::vector<torch::Tensor>& chunks)
{
    if (chunks.empty()) {
        emit error("No chunks to overlap-add");
        return torch::Tensor();
    }

    try {
        int64_t chunkSize = chunks[0].size(1);
        int64_t step = static_cast<int64_t>(chunkSize * (1.0f - overlapRate));
        int64_t totalLength = step * (chunks.size() - 1) + chunkSize;

        torch::Tensor output = torch::zeros({1, totalLength, 1}, torch::kFloat);
        torch::Tensor weight = torch::zeros({totalLength}, torch::kFloat);

        for (size_t i = 0; i < chunks.size(); ++i) {
            const torch::Tensor& chunk = chunks[i];
            int64_t start = i * step;
            int64_t end = start + chunkSize;

            if (chunk.size(1) != chunkSize) {
                emit error("Chunk size mismatch in doOverlapAdd");
                return torch::Tensor();
            }

             // Overlap-add with linear ramp weights
            torch::Tensor window = torch::ones({chunkSize}, torch::kFloat);
            int64_t fadeLength = static_cast<int64_t>(chunkSize * overlapRate);
            if (fadeLength > 0) {
                torch::Tensor fadeIn = torch::linspace(0, 1, fadeLength);
                torch::Tensor fadeOut = torch::linspace(1, 0, fadeLength);
                window.slice(0, 0, fadeLength) = fadeIn;
                window.slice(0, chunkSize - fadeLength, chunkSize) = fadeOut;
            }

            // Get the target slice from output tensor
            torch::Tensor outputSlice = output.slice(1, start, end);  // Shape: [1, chunkSize, 1]
            torch::Tensor chunkSlice = chunk.slice(1, 0, chunkSize);  // Shape: [1, chunkSize, 1]
            
            // Apply windowing to chunk
            torch::Tensor windowedChunk = chunkSlice * window.unsqueeze(0).unsqueeze(2);  // Shape: [1, chunkSize, 1]
            
            // Add to output slice
            outputSlice.add_(windowedChunk);
            
            // Update weight
            weight.slice(0, start, end).add_(window);
        }

        // Normalize by weight to avoid amplitude scaling
        weight = torch::where(weight == 0, torch::ones_like(weight), weight);
        // Reshape weight to match output tensor dimensions for broadcasting
        weight = weight.unsqueeze(0).unsqueeze(2);  // Shape: [totalLength] -> [1, totalLength, 1]
        output = output / weight;

        // Output is already (1, totalLength, 1), no reshape needed

        return output;
    } catch (const c10::Error& e) {
        emit error(QString("Overlap-add error: %1").arg(e.what()));
        return torch::Tensor();
    }
}

torch::Tensor SeparationWorker::doOverlapAdd(const QStringList& chunkFilePaths)
{
    std::vector<torch::Tensor> chunks;
    for (const QString& path : chunkFilePaths) {
    torch::Tensor chunk = AudioPreprocessUtils::loadAudio(path); // 1D (frames,)
    if (chunk.numel() == 0) {
        emit error(QString("Failed to load chunk from: %1").arg(path));
        return torch::Tensor();
    }

    // Apply Hann window directly
    chunk = AudioPreprocessUtils::applyHannWindow(chunk); // still 1D

    // reshape to (1, frames, 1) for model
    chunk = chunk.unsqueeze(0).unsqueeze(2);

    chunks.push_back(chunk);
    }
    return doOverlapAdd(chunks);
}

void SeparationWorker::processFile(const QStringList& filePaths, const QString& featureName)
{
    for (const QString& audioPath : filePaths) {
        processSingleFile(audioPath, featureName);
    }
}

void SeparationWorker::processSingleFile(const QString& audioPath, const QString& featureName)
{
    ZeroShotASPFeatureExtractor extractor;
    if (!extractor.loadModelFromResource(Constants::ZERO_SHOT_ASP_MODEL_RESOURCE)) {
        qDebug() << "Failed to load ZeroShotASP model from resource, trying absolute path...";
        if (!extractor.loadModel(Constants::ZERO_SHOT_ASP_MODEL_PATH)) {
            emit error("Failed to load separation model");
            return;
        }
    }

    QFileInfo audioFileInfo(audioPath);
    if (!audioFileInfo.exists() || !audioFileInfo.isReadable()) {
        emit error(QString("Audio file does not exist or is not readable: %1").arg(audioPath));
        return;
    }

    // Load feature tensor
    QString featurePath = QString("%1/%2.txt").arg(Constants::OUTPUT_FEATURES_DIR).arg(featureName);
    torch::Tensor condition = loadFeature(featurePath);
    if (!condition.defined() || condition.numel() == 0) {
        emit error(QString("Failed to load feature tensor: %1").arg(featurePath));
        return;
    }

    // Load audio waveform tensor from file (assumed to be WAV)
    torch::Tensor waveform = AudioPreprocessUtils::loadAudio(audioPath);
    if (waveform.numel() == 0) {
        emit error(QString("Failed to load audio waveform from: %1").arg(audioPath));
        return;
    }

    AudioPreprocessUtils::saveToWav(waveform, Constants::TEMP_SEGMENTS_DIR + "/mono.wav");

    if (waveform.dim() != 1) {
        emit error("Loaded waveform tensor must be 1D");
        return;
    }

    int64_t totalSamples = waveform.size(0);
    int64_t step = static_cast<int64_t>(clipSamples * (1.0f - overlapRate));
    if (step <= 0) {
        emit error("Invalid step size calculated from clipSamples and overlapRate");
        return;
    }

    int64_t pos = 0;
    int chunkIndex = 0;
    QStringList chunkFilePaths;

    while (pos < totalSamples) {
        int64_t endPos = pos + clipSamples;
        torch::Tensor chunk;
        if (endPos <= totalSamples) {
            chunk = waveform.slice(0, pos, endPos);
        } else {
            // Pad last chunk with zeros if needed
            int64_t padSize = endPos - totalSamples;
            torch::Tensor tail = waveform.slice(0, pos, totalSamples);
            torch::Tensor padding = torch::zeros({padSize}, torch::kFloat);
            chunk = torch::cat({tail, padding}, 0);
        }

        // Reshape chunk to (1, clipSamples, 1)
        chunk = chunk.unsqueeze(0).unsqueeze(2);

        torch::Tensor processedChunk = processChunk(chunk, condition, &extractor);
        if (!processedChunk.defined() || processedChunk.numel() == 0) {
            emit error("Processing chunk failed");
            return;
        }

        // Save chunk to file immediately, do not store in RAM vector
        QString chunkFilePath = QString("%1/%2_chunk_%3.wav").arg(Constants::TEMP_SEGMENTS_DIR).arg(featureName).arg(chunkIndex);
        emit chunkReady(chunkFilePath, featureName, processedChunk);
        chunkFilePaths.append(chunkFilePath);

        // Update progress
        int progress = static_cast<int>(100.0 * (pos + clipSamples) / totalSamples);
        if (progress > 100) progress = 100;
        emit progressUpdated(progress);

        pos += step;
        chunkIndex++;
    }

    // Unload model to free memory before overlap-add
    extractor.unloadModel();

    // Load chunk files and do overlap-add incrementally
    try {
        torch::Tensor finalTensor = doOverlapAdd(chunkFilePaths);
        if (!finalTensor.defined() || finalTensor.numel() == 0) {
            emit error("Overlap-add failed");
            return;
        }

        emit separationFinished(audioPath, featureName, finalTensor);
    } catch (const c10::Error& e) {
        emit error(QString("Final overlap-add error: %1").arg(e.what()));
        return;
    }
}
