// separationworker.cpp
#include "separationworker.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringList>
#include <QDir>
#include <torch/torch.h>
#include <cmath>
#include <functional>
#include "audio_preprocess_utils.h"

#include <QDebug>

// Utility function for tensor shape debugging (only used in debug builds)

static QString tensorShapeToString(const torch::Tensor& tensor) {
    if (!tensor.defined()) return "undefined";
    QStringList dims;
    for (int64_t dim : tensor.sizes()) {
        dims << QString::number(dim);
    }
    return QString("(%1)").arg(dims.join(", "));
}


// ============================================================================
// Constructor and Public API Methods
// ============================================================================

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
        emitError(QString("Feature file does not exist or is not readable: %1").arg(featurePath));
        return torch::Tensor();
    }

    QFile file(featurePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emitError(QString("Failed to open feature file: %1").arg(featurePath));
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
            emitError(QString("Invalid float value in feature file: %1").arg(part));
            return torch::Tensor();
        }
        values.push_back(val);
    }

    if (values.empty()) {
        emitError("Feature file is empty or invalid format");
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
        emitError("Extractor is not initialized");
        return torch::Tensor();
    }

    if (!validateTensorShape(waveform, {1, m_clipSamples, 1}, "waveform")) {
        return torch::Tensor();
    }

    if (!validateTensorShape(condition, {1, -1}, "condition")) {
        return torch::Tensor();
    }

    try {
        ASPModelInput input{waveform, condition};
        auto output = extractor->process(input);
        if (output) {
            return output->wav_out;
        } else {
            emitError("Extractor process returned empty result");
            return torch::Tensor();
        }
    } catch (const c10::Error& e) {
        emitError(QString("Extractor process error: %1").arg(e.what()));
        return torch::Tensor();
    }
}

torch::Tensor SeparationWorker::doOverlapAdd(const std::vector<torch::Tensor>& chunks)
{
    if (chunks.empty()) {
        emitError("No chunks to overlap-add");
        return torch::Tensor();
    }

    try {
        int64_t chunkSize = chunks[0].size(1);
        int64_t step = static_cast<int64_t>(chunkSize * (1.0f - m_overlapRate));
        int64_t totalLength = step * (chunks.size() - 1) + chunkSize;

        torch::Tensor output = torch::zeros({1, totalLength, 1}, torch::kFloat);
        torch::Tensor weight = torch::zeros({totalLength}, torch::kFloat);

        for (size_t i = 0; i < chunks.size(); ++i) {
            const torch::Tensor& chunk = chunks[i];
            int64_t start = i * step;
            int64_t end = start + chunkSize;

            if (chunk.size(1) != chunkSize) {
                emitError("Chunk size mismatch in doOverlapAdd");
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
            torch::Tensor outputSlice = output.slice(1, start, end);
            torch::Tensor chunkSlice = chunk.slice(1, 0, chunkSize);

            // Apply windowing to chunk
            torch::Tensor windowedChunk = chunkSlice * window.unsqueeze(0).unsqueeze(2);

            // Add to output slice
            outputSlice.add_(windowedChunk);

            // Update weight
            weight.slice(0, start, end).add_(window);
        }

        // Normalize by weight to avoid amplitude scaling
        weight = torch::where(weight == 0, torch::ones_like(weight), weight);
        weight = weight.unsqueeze(0).unsqueeze(2);
        output = output / weight;

        return output;
    } catch (const c10::Error& e) {
        emitError(QString("Overlap-add error: %1").arg(e.what()));
        return torch::Tensor();
    }
}

// ============================================================================
// Main Processing Entry Points
// ============================================================================

void SeparationWorker::processFile(const QStringList& filePaths, const QString& featureName)
{
    qDebug() << "SeparationWorker::processFile - Processing" << filePaths.size() << "files for separation using feature:" << featureName;
    qDebug() << "SeparationWorker::processFile - File paths to process:";
    for (const QString& audioPath : filePaths) {
        qDebug() << "  - " << audioPath;
    }

    for (const QString& audioPath : filePaths) {
        processAudioFile(audioPath, featureName);
    }
}

void SeparationWorker::processAudioFile(const QString& audioPath, const QString& featureName)
{
    qDebug() << "SeparationWorker::processAudioFile - Processing individual file:" << audioPath << "with feature:" << featureName;

    QFileInfo audioFileInfo(audioPath);
    if (!audioFileInfo.exists() || !audioFileInfo.isReadable()) {
        emitError(QString("Audio file does not exist or is not readable: %1").arg(audioPath));
        return;
    }

    torch::Tensor waveform = loadAudioWaveform(audioPath, true);
    if (!waveform.defined()) {
        return; // Error already emitted by loadAudioWaveform
    }

    // Determine if audio is stereo and delegate to appropriate processing method
    bool isStereo = (waveform.dim() == 2 && waveform.size(0) == 2);
    ProcessingResult result = isStereo
        ? processStereoAudio(audioPath, featureName)
        : processMonoAudio(audioPath, featureName);

    if (result.success) {
        emit finished(audioPath, featureName, result.separatedAudio);
    } else {
        emitError(result.errorMessage);
    }
}

// ============================================================================
// Core Processing Methods
// ============================================================================

torch::Tensor SeparationWorker::loadAudioWaveform(const QString& audioPath, bool keepOriginalFormat)
{
    torch::Tensor waveform = AudioPreprocessUtils::loadAudio(audioPath, !keepOriginalFormat);
    if (waveform.numel() == 0) {
        emitError(QString("Failed to load audio waveform from: %1").arg(audioPath));
    }
    return waveform;
}

SeparationWorker::ProcessingResult SeparationWorker::processMonoAudio(const QString& audioPath, const QString& featureName)
{
    auto extractor = createAndLoadExtractor();
    if (!extractor) {
        return ProcessingResult("Failed to create and load feature extractor");
    }

    torch::Tensor condition = loadConditioningFeature(featureName);
    if (!condition.defined()) {
        return ProcessingResult("Failed to load conditioning feature");
    }

    torch::Tensor waveform = loadAudioWaveform(audioPath, false);
    if (!waveform.defined()) {
        return ProcessingResult("Failed to load audio waveform");
    }

    if (!validateTensorShape(waveform, {1, waveform.size(1)}, "mono waveform")) {
        return ProcessingResult("Invalid mono waveform shape");
    }

    int64_t originalLength = waveform.size(1);
    auto chunks = createAudioChunks(waveform.squeeze(0));

    auto processedChunks = processAudioChunks(chunks, condition, extractor.get(),
        [this](int progress) { emitProgress(progress); });

    if (processedChunks.empty()) {
        return ProcessingResult("Failed to process audio chunks");
    }

    torch::Tensor result = reconstructAudioFromChunks(processedChunks, originalLength);
    if (!result.defined()) {
        return ProcessingResult("Failed to reconstruct audio from chunks");
    }

    return ProcessingResult(result);
}

SeparationWorker::ProcessingResult SeparationWorker::processStereoAudio(const QString& audioPath, const QString& featureName)
{
    qDebug() << "SeparationWorker::processStereoAudio - Starting stereo separation for:" << audioPath;

    auto extractor = createAndLoadExtractor();
    if (!extractor) {
        return ProcessingResult("Failed to create and load feature extractor");
    }

    torch::Tensor condition = loadConditioningFeature(featureName);
    if (!condition.defined()) {
        return ProcessingResult("Failed to load conditioning feature");
    }

    torch::Tensor waveform = loadAudioWaveform(audioPath, true);
    if (!waveform.defined()) {
        return ProcessingResult("Failed to load stereo audio waveform");
    }

    if (!validateTensorShape(waveform, {2, waveform.size(1)}, "stereo waveform")) {
        return ProcessingResult("Invalid stereo waveform shape");
    }

    qDebug() << "SeparationWorker::processStereoAudio - Successfully loaded stereo waveform, shape:" << tensorShapeToString(waveform);

    auto channels = extractStereoChannels(waveform);
    qDebug() << "SeparationWorker::processStereoAudio - Extracted" << channels.size() << "stereo channels";

    std::vector<torch::Tensor> processedChannels;
    for (size_t i = 0; i < channels.size(); ++i) {
        qDebug() << "SeparationWorker::processStereoAudio - Processing" << channels[i].name << "channel," << "shape:" << tensorShapeToString(channels[i].waveform);
        auto result = processAudioChannel(channels[i], condition, extractor.get());
        if (!result.success) {
            return ProcessingResult(QString("Failed to process %1 channel: %2")
                .arg(channels[i].name).arg(result.errorMessage));
        }
        qDebug() << "SeparationWorker::processStereoAudio - Completed processing" << channels[i].name << "channel," << "result shape:" << tensorShapeToString(result.separatedAudio);
        processedChannels.push_back(result.separatedAudio);
    }

    qDebug() << "SeparationWorker::processStereoAudio - Combining stereo channels";
    torch::Tensor combinedResult = combineStereoChannels(processedChannels[0], processedChannels[1]);
    if (!combinedResult.defined()) {
        return ProcessingResult("Failed to combine stereo channels");
    }

    qDebug() << "SeparationWorker::processStereoAudio - Stereo separation completed, final shape:" << tensorShapeToString(combinedResult);
    return ProcessingResult(combinedResult);
}

std::vector<SeparationWorker::AudioChannel> SeparationWorker::extractStereoChannels(const torch::Tensor& stereoWaveform)
{
    torch::Tensor leftChannel = stereoWaveform.index({0, torch::indexing::Slice()}).clone();
    torch::Tensor rightChannel = stereoWaveform.index({1, torch::indexing::Slice()}).clone();

    return {
        AudioChannel(leftChannel, "left", 0),
        AudioChannel(rightChannel, "right", 50)
    };
}

SeparationWorker::ProcessingResult SeparationWorker::processAudioChannel(const AudioChannel& channel,
                                                                        const torch::Tensor& condition,
                                                                        ZeroShotASPFeatureExtractor* extractor)
{
    auto chunks = createAudioChunks(channel.waveform);
    int64_t originalLength = channel.waveform.size(0);

    auto progressCallback = [this, offset = channel.progressOffset](int progress) {
        emitProgress(offset + progress / 2);
    };

    auto processedChunks = processAudioChunks(chunks, condition, extractor, progressCallback);
    if (processedChunks.empty()) {
        return ProcessingResult(QString("Failed to process %1 channel chunks").arg(channel.name));
    }

    torch::Tensor result = reconstructAudioFromChunks(processedChunks, originalLength);
    if (!result.defined()) {
        return ProcessingResult(QString("Failed to reconstruct %1 channel").arg(channel.name));
    }

    return ProcessingResult(result);
}

std::unique_ptr<ZeroShotASPFeatureExtractor> SeparationWorker::createAndLoadExtractor()
{
    auto extractor = std::make_unique<ZeroShotASPFeatureExtractor>();
    if (!extractor->loadModelFromResource(Constants::ZERO_SHOT_ASP_MODEL_RESOURCE)) {
        if (!extractor->loadModel(Constants::ZERO_SHOT_ASP_MODEL_PATH)) {
            return nullptr;
        }
    }
    return extractor;
}

torch::Tensor SeparationWorker::loadConditioningFeature(const QString& featureName)
{
    QString featurePath = QString("%1/%2.txt").arg(Constants::OUTPUT_FEATURES_DIR).arg(featureName);
    return loadFeature(featurePath);
}

std::vector<torch::Tensor> SeparationWorker::createAudioChunks(const torch::Tensor& waveform)
{
    std::vector<torch::Tensor> chunks;
    int64_t totalSamples = waveform.size(0);
    int64_t step = static_cast<int64_t>(m_clipSamples * (1.0f - m_overlapRate));

    if (step <= 0) {
        emitError("Invalid step size calculated from clipSamples and overlapRate");
        return chunks;
    }

    int64_t pos = 0;
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

        chunk = chunk.unsqueeze(0).unsqueeze(2);
        chunks.push_back(chunk);
        pos += step;
    }

    return chunks;
}

std::vector<torch::Tensor> SeparationWorker::processAudioChunks(const std::vector<torch::Tensor>& chunks,
                                                               const torch::Tensor& condition,
                                                               ZeroShotASPFeatureExtractor* extractor,
                                                               std::function<void(int)> progressCallback)
{
    std::vector<torch::Tensor> processedChunks;
    int64_t totalChunks = static_cast<int64_t>(chunks.size());

    for (int64_t i = 0; i < totalChunks; ++i) {
        torch::Tensor processedChunk = processChunk(chunks[i], condition, extractor);
        if (!processedChunk.defined()) {
            return {}; // Error already emitted by processChunk
        }
        processedChunks.push_back(processedChunk);

        if (progressCallback) {
            int progress = static_cast<int>(100.0 * (i + 1) / totalChunks);
            progressCallback(progress);
        }
    }

    return processedChunks;
}

torch::Tensor SeparationWorker::reconstructAudioFromChunks(const std::vector<torch::Tensor>& processedChunks,
                                                          int64_t originalLength)
{
    torch::Tensor result = doOverlapAdd(processedChunks);
    if (!result.defined()) {
        return torch::Tensor(); // Error already emitted by doOverlapAdd
    }

    // Trim the result to match the original input length exactly
    result = result.slice(1, 0, originalLength);

    return result;
}

torch::Tensor SeparationWorker::combineStereoChannels(const torch::Tensor& leftChannel,
                                                     const torch::Tensor& rightChannel)
{
    try {
        torch::Tensor leftSqueezed = leftChannel.squeeze().contiguous();
        torch::Tensor rightSqueezed = rightChannel.squeeze().contiguous();

        torch::Tensor finalStereo = torch::cat({leftSqueezed.unsqueeze(1), rightSqueezed.unsqueeze(1)}, 1);

        return finalStereo;
    } catch (const c10::Error& e) {
        emitError(QString("Error combining stereo channels: %1").arg(e.what()));
        return torch::Tensor();
    }
}

// ============================================================================
// Utility Methods
// ============================================================================

bool SeparationWorker::validateTensorShape(const torch::Tensor& tensor,
                                          const std::vector<int64_t>& expectedSizes,
                                          const QString& tensorName)
{
    if (!tensor.defined()) {
        emitError(QString("%1 tensor is not defined").arg(tensorName));
        return false;
    }

    if (tensor.dim() != static_cast<int64_t>(expectedSizes.size())) {
        emitError(QString("%1 tensor has wrong number of dimensions: expected %2, got %3")
            .arg(tensorName).arg(expectedSizes.size()).arg(tensor.dim()));
        return false;
    }

    for (size_t i = 0; i < expectedSizes.size(); ++i) {
        if (expectedSizes[i] >= 0 && tensor.size(i) != expectedSizes[i]) {
            emitError(QString("%1 tensor has wrong size at dimension %2: expected %3, got %4")
                .arg(tensorName).arg(i).arg(expectedSizes[i]).arg(tensor.size(i)));
            return false;
        }
    }

    return true;
}

void SeparationWorker::emitError(const QString& message)
{
    emit error(message);
}

void SeparationWorker::emitProgress(int progress)
{
    emit progressUpdated(progress);
}
