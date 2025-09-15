#include "separationworker.h"
#include "resourcemanager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <torch/script.h>
#include <torch/torch.h>

SeparationWorker::SeparationWorker(QObject *parent)
    : QObject(parent)
{
}

void SeparationWorker::processFile(const QString& audioPath, const QString& featureName)
{
    QStringList results = doProcessAndSaveSeparatedChunks(audioPath, featureName);
    if (!results.isEmpty()) {
        emit finished(results);
    } else {
        emit error("Failed to process file");
    }
}

QStringList SeparationWorker::doProcessAndSaveSeparatedChunks(const QString& audioPath, const QString& featureName)
{
    QStringList separatedFiles;

    ResourceManager* rm = ResourceManager::instance();

    // Load ZeroShotASP model if not loaded
    if (!rm->loadZeroShotASPModel("/models/zero_shot_asp_separation_model.pt")) {
        qWarning() << "Failed to load ZeroShotASP model";
        return separatedFiles;
    }

    // Find the feature file
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        qWarning() << "Output features folder does not exist:" << outputFolder;
        return separatedFiles;
    }

    QStringList featureFiles = dir.entryList(QStringList() << "*.txt", QDir::Files);
    QString featureFile;
    for (const QString& file : featureFiles) {
        if (file.startsWith(featureName + "_") || file == featureName + ".txt") {
            featureFile = dir.absoluteFilePath(file);
            break;
        }
    }

    if (featureFile.isEmpty()) {
        qWarning() << "Feature file not found for:" << featureName;
        return separatedFiles;
    }

    // Load the embedding
    QFile file(featureFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open feature file:" << featureFile;
        return separatedFiles;
    }

    QTextStream in(&file);
    QString line = in.readLine();
    QStringList parts = line.split(" ", Qt::SkipEmptyParts);
    std::vector<float> embedding;
    for (const QString& part : parts) {
        embedding.push_back(part.toFloat());
    }
    file.close();

    if (embedding.size() != 2048) {
        qWarning() << "Invalid embedding size:" << embedding.size() << "expected 2048";
        return separatedFiles;
    }

    // Read and resample audio to 32kHz mono
    std::vector<float> audioData = rm->readAndResampleAudio(audioPath);
    if (audioData.empty()) {
        return separatedFiles;
    }

    const int64_t chunkSize = 320000;  // 10 seconds at 32kHz
    QFileInfo fi(audioPath);
    QString baseName = fi.baseName();

    // Collect separated chunks
    std::vector<torch::Tensor> separatedChunks;

    int totalChunks = (audioData.size() + chunkSize - 1) / chunkSize; // Ceiling division
    int chunkIndex = 0;

    for (size_t start = 0; start < audioData.size(); start += chunkSize) {
        size_t end = std::min(start + chunkSize, audioData.size());
        std::vector<float> chunk(audioData.begin() + start, audioData.begin() + end);
        size_t originalLength = chunk.size();

        // Pad the chunk with zeros to make it chunkSize
        if (chunk.size() < chunkSize) {
            chunk.resize(chunkSize, 0.0f);
        }

        // Convert to tensor and add batch dimension
        torch::Tensor waveform = torch::from_blob(chunk.data(), {1, chunkSize, 1}, torch::kFloat32).clone();
        torch::Tensor condition = torch::from_blob(embedding.data(), {1, 2048}, torch::kFloat32).clone();

        // Separate audio
        torch::Tensor separated = rm->separateAudio(waveform, condition);
        if (separated.numel() == 0) {
            qWarning() << "Separation failed for chunk starting at" << start;
            continue;
        }

        // Trim to original length
        torch::Tensor separatedTrimmed = separated.squeeze(0).squeeze(1).slice(0, 0, originalLength);
        separatedChunks.push_back(separatedTrimmed);

        chunkIndex++;
        int progress = chunkIndex * 100 / totalChunks;
        emit progressUpdated(progress);
    }

    if (separatedChunks.empty()) {
        qWarning() << "No separated chunks generated for file:" << audioPath;
        return separatedFiles;
    }

    // Combine all separated chunks into one tensor
    torch::Tensor combinedSeparated = torch::cat(separatedChunks, 0);

    // Create output directory
    QDir separatedDir("separated_chunks");
    if (!separatedDir.exists()) {
        separatedDir.mkpath(".");
    }

    // Save combined separated audio
    QString separatedFile = QString("separated_chunks/separated_%1.wav").arg(baseName);
    if (rm->saveWav(combinedSeparated, separatedFile, 32000)) {
        separatedFiles.append(separatedFile);
    }

    return separatedFiles;
}
