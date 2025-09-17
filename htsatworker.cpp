#include "htsatworker.h"
#include "resourcemanager.h"
#include "audio_preprocess_utils.h"
#include <QDebug>
#include <vector>
#include "constants.h"
#include <sndfile.h>

HTSATWorker::HTSATWorker(QObject *parent)
    : QObject(parent)
{
}

void HTSATWorker::generateFeatures(const QStringList& filePaths, const QString& outputFileName) {
    std::vector<float> avg_emb = doGenerateAudioFeatures(filePaths, outputFileName);
    if (!avg_emb.empty()) {
        emit finished(avg_emb, outputFileName);
    } else {
        emit error("Failed to generate features");
    }
}

std::vector<float> HTSATWorker::doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    HTSATProcessor processor;
    if (!processor.loadModel(Constants::HTSAT_MODEL_PATH)) {
        qDebug() << "Failed to load HTSAT model";
        return std::vector<float>();
    }

    QVector<std::vector<float>> embeddings = processFilesAndCollectEmbeddings(filePaths, &processor);
    if (embeddings.isEmpty()) {
        return std::vector<float>();
    }

    std::vector<float> avg_emb = computeAverageEmbedding(embeddings);
    return avg_emb;
}



QVector<std::vector<float>> HTSATWorker::processFilesAndCollectEmbeddings(const QStringList& filePaths, HTSATProcessor* processor)
{
    QVector<std::vector<float>> embeddings;
    int totalFiles = filePaths.size();
    for (int i = 0; i < totalFiles; ++i) {
        const QString& filePath = filePaths[i];

        // Get sample rate and channels
        SF_INFO sfinfo;
        SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfinfo);
        if (!file) {
            qDebug() << "Failed to open file for info:" << filePath;
            continue;
        }
        int sampleRate = sfinfo.samplerate;
        int channels = sfinfo.channels;
        sf_close(file);

        // Load audio tensor
        torch::Tensor audioTensor = AudioPreprocessUtils::loadAudio(filePath);
        if (audioTensor.numel() == 0) {
            qDebug() << "Failed to load audio:" << filePath;
            continue;
        }

        // Convert to mono if needed
        if (channels > 1) {
            audioTensor = AudioPreprocessUtils::convertToMono(audioTensor, channels);
        }

        // Resample if needed
        if (sampleRate != Constants::AUDIO_SAMPLE_RATE) {
            audioTensor = AudioPreprocessUtils::resampleAudio(audioTensor, sampleRate, Constants::AUDIO_SAMPLE_RATE);
        }

        // Process tensor
        std::vector<float> embedding = processor->processTensor(audioTensor);
        if (embedding.empty()) {
            qDebug() << "Failed to process tensor for file:" << filePath;
            continue;
        }
        embeddings.append(embedding);
        int progress = (i + 1) * 100 / totalFiles;
        emit progressUpdated(progress);
    }
    return embeddings;
}

std::vector<float> HTSATWorker::computeAverageEmbedding(const QVector<std::vector<float>>& embeddings)
{
    std::vector<float> avg_emb(embeddings[0].size(), 0.0f);
    for (const auto& emb : embeddings) {
        for (size_t i = 0; i < emb.size(); ++i) {
            avg_emb[i] += emb[i];
        }
    }
    for (size_t i = 0; i < avg_emb.size(); ++i) {
        avg_emb[i] /= embeddings.size();
    }
    return avg_emb;
}
