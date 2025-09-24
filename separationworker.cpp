// separationworker.cpp
#include "separationworker.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <torch/torch.h>
#include <cmath>
#include "audio_preprocess_utils.h"

QString sizesToString(const std::vector<int64_t>& sizes) {
    QStringList list;
    for (int64_t s : sizes) list << QString::number(s);
    return list.join(" ");
}

QString sizesToString(const c10::IntArrayRef& sizes) {
    QStringList list;
    for (size_t i = 0; i < sizes.size(); ++i) list << QString::number(sizes[i]);
    return list.join(" ");
}

SeparationWorker::SeparationWorker(QObject* parent)
    : QObject(parent),
      m_overlapRate(Constants::AUDIO_OVERLAP_RATE),
      m_clipSamples(Constants::AUDIO_CLIP_SAMPLES)
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

    if (waveform.dim() != 3 || waveform.size(0) != 1 || waveform.size(2) != 1 || waveform.size(1) != m_clipSamples) {
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
    qDebug() << "Debug: Starting doOverlapAdd with" << chunks.size() << "chunks";

    if (chunks.empty()) {
        emit error("No chunks to overlap-add");
        return torch::Tensor();
    }

    try {
        int64_t chunkSize = chunks[0].size(1);
        int64_t step = static_cast<int64_t>(chunkSize * (1.0f - m_overlapRate));
        int64_t totalLength = step * (chunks.size() - 1) + chunkSize;

        qDebug() << "Debug: Overlap-add params - chunkSize:" << chunkSize << "step:" << step << "totalLength:" << totalLength;
        qDebug() << "Debug: First chunk shape:" << sizesToString(chunks[0].sizes());

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
            int64_t fadeLength = static_cast<int64_t>(chunkSize * m_overlapRate);
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

    // Emit signal to delete the chunk file after loading it as tensor
    emit deleteChunkFile(path);
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
    qDebug() << "Debug: Starting mono processing for" << audioPath;

    QFileInfo audioFileInfo(audioPath);
    if (!audioFileInfo.exists() || !audioFileInfo.isReadable()) {
        emit error(QString("Audio file does not exist or is not readable: %1").arg(audioPath));
        return;
    }

    // Load audio once - let loadAudio handle stereo detection and conversion
    torch::Tensor waveform = AudioPreprocessUtils::loadAudio(audioPath, false); // Keep original format
    if (waveform.numel() == 0) {
        emit error(QString("Failed to load audio waveform from: %1").arg(audioPath));
        return;
    }

    qDebug() << "Debug: Loaded waveform shape:" << sizesToString(waveform.sizes());

    // Check if input is stereo
    bool isStereo = (waveform.dim() == 2 && waveform.size(1) == 2);
    if (isStereo) {
        qDebug() << "Debug: File is stereo, delegating to stereo processing";
        processSingleFileStereo(audioPath, featureName);
        return;
    }

    qDebug() << "Debug: Confirmed mono audio processing";

    // Original mono processing
    qDebug() << "Debug: Creating ZeroShotASPFeatureExtractor for mono processing";
    ZeroShotASPFeatureExtractor extractor;
    qDebug() << "Debug: Loading model from resource...";
    if (!extractor.loadModelFromResource(Constants::ZERO_SHOT_ASP_MODEL_RESOURCE)) {
        qDebug() << "Failed to load ZeroShotASP model from resource, trying absolute path...";
        if (!extractor.loadModel(Constants::ZERO_SHOT_ASP_MODEL_PATH)) {
            emit error("Failed to load separation model");
            return;
        }
    }
    qDebug() << "Debug: Model loaded successfully for mono processing";

    // Load feature tensor
    QString featurePath = QString("%1/%2.txt").arg(Constants::OUTPUT_FEATURES_DIR).arg(featureName);
    torch::Tensor condition = loadFeature(featurePath);
    if (!condition.defined() || condition.numel() == 0) {
        emit error(QString("Failed to load feature tensor: %1").arg(featurePath));
        return;
    }

    // Convert to proper 1D format for mono processing
    if (waveform.dim() == 2 && waveform.size(1) == 1) {
        waveform = waveform.squeeze(1); // Convert from (frames, 1) to (frames,)
        qDebug() << "Debug: Squeezed mono waveform to 1D, new shape:" << sizesToString(waveform.sizes());
    } else if (waveform.dim() != 1) {
        emit error("Loaded waveform tensor must be 1D for mono processing");
        return;
    }

    AudioPreprocessUtils::saveToWav(waveform, Constants::TEMP_SEGMENTS_DIR + "/mono.wav");

    int64_t originalLength = waveform.size(0);
    int64_t totalSamples = originalLength;
    int64_t step = static_cast<int64_t>(m_clipSamples * (1.0f - m_overlapRate));
    if (step <= 0) {
        emit error("Invalid step size calculated from clipSamples and overlapRate");
        return;
    }

    int64_t pos = 0;
    int chunkIndex = 0;
    std::vector<torch::Tensor> processedChunks;

    while (pos < totalSamples) {
        int64_t endPos = pos + m_clipSamples;
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

        // Direct save tensor in vector
        processedChunks.push_back(processedChunk);

        // Update progress
        int progress = static_cast<int>(100.0 * (pos + m_clipSamples) / totalSamples);
        if (progress > 100) progress = 100;
        emit progressUpdated(progress);

        pos += step;
        chunkIndex++;
    }

    // Unload model to free memory before overlap-add
    qDebug() << "Debug: Unloading model after chunk processing";
    extractor.unloadModel();
    qDebug() << "Debug: Model unloaded successfully";

    // Do overlap-add with tensor vector
    try {
        qDebug() << "Debug: About to do overlap-add for mono processing";
        qDebug() << "Debug: Number of processed chunks:" << processedChunks.size();
        if (!processedChunks.empty()) {
            qDebug() << "Debug: First processed chunk shape:" << sizesToString(processedChunks[0].sizes());
        }

        torch::Tensor finalTensor = doOverlapAdd(processedChunks);
        if (!finalTensor.defined() || finalTensor.numel() == 0) {
            emit error("Overlap-add failed");
            return;
        }

        qDebug() << "Debug: Overlap-add result shape:" << sizesToString(finalTensor.sizes());
        qDebug() << "Debug: Original length:" << originalLength;

        // Trim the final tensor to match the original input length
        if (finalTensor.size(1) > originalLength) {
            finalTensor = finalTensor.slice(1, 0, originalLength);
            qDebug() << "Debug: Trimmed final tensor to original length, new shape:" << sizesToString(finalTensor.sizes());
        }

        emit separationFinished(audioPath, featureName, finalTensor);
    } catch (const c10::Error& e) {
        emit error(QString("Final overlap-add error: %1").arg(e.what()));
        return;
    }
}

void SeparationWorker::processSingleFileStereo(const QString& audioPath, const QString& featureName)
{
    qDebug() << "Debug: Starting stereo processing for" << audioPath;
    qDebug() << "Debug: Creating ZeroShotASPFeatureExtractor for stereo processing";
    ZeroShotASPFeatureExtractor extractor;
    qDebug() << "Debug: Loading model from resource for stereo...";
    if (!extractor.loadModelFromResource(Constants::ZERO_SHOT_ASP_MODEL_RESOURCE)) {
        qDebug() << "Failed to load ZeroShotASP model from resource, trying absolute path...";
        if (!extractor.loadModel(Constants::ZERO_SHOT_ASP_MODEL_PATH)) {
            emit error("Failed to load separation model");
            return;
        }
    }
    qDebug() << "Debug: Model loaded successfully for stereo processing";

    // Load feature tensor
    QString featurePath = QString("%1/%2.txt").arg(Constants::OUTPUT_FEATURES_DIR).arg(featureName);
    torch::Tensor condition = loadFeature(featurePath);
    if (!condition.defined() || condition.numel() == 0) {
        emit error(QString("Failed to load feature tensor: %1").arg(featurePath));
        return;
    }

    // Load stereo audio waveform tensor from file (assumed to be WAV)
    torch::Tensor waveform = AudioPreprocessUtils::loadAudio(audioPath, false); // Keep stereo
    if (waveform.numel() == 0) {
        emit error(QString("Failed to load audio waveform from: %1").arg(audioPath));
        return;
    }

    qDebug() << "Debug: Loaded stereo waveform shape:" << sizesToString(waveform.sizes());

    if (waveform.dim() != 2 || waveform.size(1) != 2) {
        emit error("Input must be stereo (2 channels)");
        return;
    }

    int64_t originalLength = waveform.size(0);
    qDebug() << "Debug: Original length:" << originalLength;

    // Extract left and right channels
    torch::Tensor leftChannel = waveform.index({torch::indexing::Slice(), 0}).clone();  // (frames,)
    torch::Tensor rightChannel = waveform.index({torch::indexing::Slice(), 1}).clone(); // (frames,)

    qDebug() << "Debug: Left channel shape:" << sizesToString(leftChannel.sizes());
    qDebug() << "Debug: Right channel shape:" << sizesToString(rightChannel.sizes());

    // Process left channel
    emit progressUpdated(0); // Reset progress
    qDebug() << "Debug: Processing left channel";
    torch::Tensor leftResult = processChannel(leftChannel, condition, featureName + "_left", &extractor, 0);
    if (!leftResult.defined()) {
        emit error("Failed to process left channel");
        return;
    }
    qDebug() << "Debug: Left result shape:" << sizesToString(leftResult.sizes());

    // Process right channel
    qDebug() << "Debug: Processing right channel";
    torch::Tensor rightResult = processChannel(rightChannel, condition, featureName + "_right", &extractor, 50);
    if (!rightResult.defined()) {
        emit error("Failed to process right channel");
        return;
    }
    qDebug() << "Debug: Right result shape:" << sizesToString(rightResult.sizes());

    // Unload model to free memory
    qDebug() << "Debug: Unloading model after stereo channel processing";
    extractor.unloadModel();
    qDebug() << "Debug: Model unloaded successfully for stereo";

    qDebug() << "Debug: About to combine channels";
    try {
        torch::Tensor leftSqueezed = leftResult.squeeze().contiguous();
        torch::Tensor rightSqueezed = rightResult.squeeze().contiguous();
        qDebug() << "Debug: Left squeezed shape:" << sizesToString(leftSqueezed.sizes());
        qDebug() << "Debug: Right squeezed shape:" << sizesToString(rightSqueezed.sizes());

        // Combine results back into stereo format (frames, 2)
        torch::Tensor finalStereo = torch::cat({leftSqueezed.unsqueeze(1), rightSqueezed.unsqueeze(1)}, 1); // (frames, 2)

        qDebug() << "Debug: Final stereo before trim:" << sizesToString(finalStereo.sizes());

        if (finalStereo.size(0) > originalLength) {
            finalStereo = finalStereo.slice(0, 0, originalLength);
        }

        qDebug() << "Debug: Final stereo after trim:" << sizesToString(finalStereo.sizes());

        qDebug() << "Debug: Emitting separationFinished for stereo";
        emit separationFinished(audioPath, featureName, finalStereo); // Emit as (frames, 2) for stereo
    } catch (const c10::Error& e) {
        emit error(QString("Error combining stereo channels: %1").arg(e.what()));
        return;
    }
}

torch::Tensor SeparationWorker::processChannel(const torch::Tensor& channelWaveform,
                                               const torch::Tensor& condition,
                                               const QString& channelFeatureName,
                                               ZeroShotASPFeatureExtractor* extractor,
                                               int progressOffset)
{
    int64_t originalLength = channelWaveform.size(0);
    int64_t totalSamples = originalLength;
    int64_t step = static_cast<int64_t>(m_clipSamples * (1.0f - m_overlapRate));
    if (step <= 0) {
        emit error("Invalid step size calculated from clipSamples and overlapRate");
        return torch::Tensor();
    }

    int64_t pos = 0;
    int chunkIndex = 0;
    std::vector<torch::Tensor> processedChunks;

    while (pos < totalSamples) {
        int64_t endPos = pos + m_clipSamples;
        torch::Tensor chunk;
        if (endPos <= totalSamples) {
            chunk = channelWaveform.slice(0, pos, endPos);
        } else {
            // Pad last chunk with zeros if needed
            int64_t padSize = endPos - totalSamples;
            torch::Tensor tail = channelWaveform.slice(0, pos, totalSamples);
            torch::Tensor padding = torch::zeros({padSize}, torch::kFloat);
            chunk = torch::cat({tail, padding}, 0);
        }

        // Reshape chunk to (1, clipSamples, 1)
        chunk = chunk.unsqueeze(0).unsqueeze(2);

        torch::Tensor processedChunk = processChunk(chunk, condition, extractor);
        if (!processedChunk.defined() || processedChunk.numel() == 0) {
            emit error("Processing chunk failed");
            return torch::Tensor();
        }

        // Direct save tensor in vector
        processedChunks.push_back(processedChunk);

        // Update progress for channel processing
        int progress = static_cast<int>(progressOffset + 50.0 * (pos + m_clipSamples) / totalSamples);
        if (progress > 100) progress = 100;
        emit progressUpdated(progress);

        pos += step;
        chunkIndex++;
    }

    // Do overlap-add with tensor vector
    try {
        qDebug() << "Debug: About to do overlap-add for channel processing";
        qDebug() << "Debug: Channel - Number of processed chunks:" << processedChunks.size();
        qDebug() << "Debug: Channel - Original length:" << originalLength;
        if (!processedChunks.empty()) {
            qDebug() << "Debug: Channel - First processed chunk shape:" << sizesToString(processedChunks[0].sizes());
        }

        torch::Tensor result = doOverlapAdd(processedChunks);
        if (!result.defined() || result.numel() == 0) {
            emit error("Overlap-add failed for channel");
            return torch::Tensor();
        }

        qDebug() << "Debug: Channel - Overlap-add result shape:" << sizesToString(result.sizes());

        // Trim or pad the result to match the original input length exactly
        if (result.size(1) > originalLength) {
            result = result.slice(1, 0, originalLength);
            qDebug() << "Debug: Channel - Trimmed result to original length, new shape:" << sizesToString(result.sizes());
        } else if (result.size(1) < originalLength) {
            // Pad with zeros to match length
            int64_t padSize = originalLength - result.size(1);
            torch::Tensor padding = torch::zeros({1, padSize, 1}, torch::kFloat);
            result = torch::cat({result, padding}, 1);
            qDebug() << "Debug: Channel - Padded result to original length, new shape:" << sizesToString(result.sizes());
        }

        return result;
    } catch (const c10::Error& e) {
        emit error(QString("Channel overlap-add error: %1").arg(e.what()));
        return torch::Tensor();
    }
}
