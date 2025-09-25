#include "htsatworker.h"
#include "audio_preprocess_utils.h"
#include "constants.h"
#include <QDebug>
#include <QFileInfo>
#include <sndfile.h>
#include <memory>

HTSATWorker::HTSATWorker(QObject *parent)
    : QObject(parent)
    , processor_(std::make_unique<HTSATProcessor>(this))
{
    // Connect processor signals to worker signals
    connect(processor_.get(), &HTSATProcessor::errorOccurred,
            this, &HTSATWorker::error);
}

HTSATWorker::~HTSATWorker() = default;

void HTSATWorker::generateFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    if (filePaths.isEmpty()) {
        emit error("No audio files provided");
        return;
    }

    if (outputFileName.isEmpty()) {
        emit error("Output file name is empty");
        return;
    }

    // Load the model
    if (!processor_->loadModelFromResource(Constants::HTSAT_MODEL_RESOURCE)) {
        qDebug() << "Failed to load HTSAT model from resource, trying absolute path...";
        if (!processor_->loadModel(Constants::HTSAT_MODEL_PATH)) {
            emit error("Failed to load HTSAT model from both resource and file path");
            return;
        }
    }

    std::vector<float> avgEmb = doGenerateAudioFeatures(filePaths, outputFileName);
    if (!avgEmb.empty()) {
        emit finished(avgEmb, outputFileName);
    } else {
        emit error("Failed to generate features - no valid embeddings produced");
    }
}

std::vector<float> HTSATWorker::doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    std::vector<std::vector<float>> embeddings = processFilesAndCollectEmbeddings(filePaths, processor_.get());

    if (embeddings.empty()) {
        qDebug() << "No embeddings were successfully generated";
        return {};
    }

    return computeAverageEmbedding(embeddings);
}



std::vector<std::vector<float>> HTSATWorker::processFilesAndCollectEmbeddings(
    const QStringList& filePaths,
    HTSATProcessor* processor)
{
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(filePaths.size());

    int totalFiles = filePaths.size();
    int processedFiles = 0;

    for (int i = 0; i < totalFiles; ++i) {
        const QString& filePath = filePaths[i];

        auto embedding = processSingleFile(filePath, processor);
        if (embedding.has_value()) {
            embeddings.push_back(std::move(embedding.value()));
        }

        processedFiles++;
        int progress = (processedFiles * 100) / totalFiles;
        emit progressUpdated(progress);
    }

    qDebug() << "Successfully processed" << embeddings.size() << "out of" << totalFiles << "files";
    return embeddings;
}

std::optional<std::vector<float>> HTSATWorker::processSingleFile(
    const QString& filePath,
    HTSATProcessor* processor)
{
    // Validate audio file
    int sampleRate, channels;
    if (!validateAudioFile(filePath, sampleRate, channels)) {
        qDebug() << "Audio file validation failed:" << filePath;
        return std::nullopt;
    }

    // Load audio tensor
    torch::Tensor audioTensor = AudioPreprocessUtils::loadAudio(filePath);
    if (audioTensor.numel() == 0) {
        qDebug() << "Failed to load audio tensor:" << filePath;
        return std::nullopt;
    }

    // Process with HTSAT model
    auto result = processor->process(audioTensor);
    if (!result.has_value()) {
        qDebug() << "HTSAT processing failed for file:" << filePath;
        return std::nullopt;
    }

    // Extract latent output from structured result
    return extractLatentOutput(result.value());
}

std::vector<float> HTSATWorker::computeAverageEmbedding(const std::vector<std::vector<float>>& embeddings) const
{
    if (embeddings.empty()) {
        return {};
    }

    size_t embeddingSize = embeddings[0].size();
    if (embeddingSize == 0) {
        return {};
    }

    std::vector<float> avgEmbedding(embeddingSize, 0.0f);

    // Sum all embeddings
    for (const auto& embedding : embeddings) {
        if (embedding.size() != embeddingSize) {
            qWarning() << "Embedding size mismatch in averaging";
            continue;
        }

        for (size_t i = 0; i < embeddingSize; ++i) {
            avgEmbedding[i] += embedding[i];
        }
    }

    // Compute average
    float numEmbeddings = static_cast<float>(embeddings.size());
    for (size_t i = 0; i < embeddingSize; ++i) {
        avgEmbedding[i] /= numEmbeddings;
    }

    return avgEmbedding;
}

bool HTSATWorker::validateAudioFile(const QString& filePath, int& sampleRate, int& channels) const
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        qDebug() << "Audio file does not exist or is not readable:" << filePath;
        return false;
    }

    SF_INFO sfInfo;
    SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfInfo);

    if (!file) {
        qDebug() << "Failed to open audio file:" << filePath;
        return false;
    }

    sampleRate = sfInfo.samplerate;
    channels = sfInfo.channels;

    // Validate audio properties
    if (sampleRate <= 0 || channels <= 0) {
        qDebug() << "Invalid audio properties - sample rate:" << sampleRate << "channels:" << channels;
        sf_close(file);
        return false;
    }

    sf_close(file);
    return true;
}

std::vector<float> HTSATWorker::extractLatentOutput(const HTSATOutput& output) const
{
    // Convert latent output tensor to vector
    auto latentTensor = output.latent_output;
    std::vector<float> result(latentTensor.data_ptr<float>(),
                             latentTensor.data_ptr<float>() + latentTensor.numel());
    return result;
}
