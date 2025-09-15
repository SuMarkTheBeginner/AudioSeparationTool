#include "htsatworker.h"
#include "resourcemanager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>

HTSATWorker::HTSATWorker(QObject *parent)
    : QObject(parent)
{
}

void HTSATWorker::generateFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    QString result = doGenerateAudioFeatures(filePaths, outputFileName);
    if (!result.isEmpty()) {
        emit finished(result);
    } else {
        emit error("Failed to generate features");
    }
}

QString HTSATWorker::doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName)
{
    ResourceManager* rm = ResourceManager::instance();

    // Load model if not loaded
    if (!rm->loadModel("/models/htsat_embedding_model.pt")) {
        qDebug() << "Failed to load HTSAT model";
        return QString();
    }

    QVector<std::vector<float>> embeddings;
    int totalFiles = filePaths.size();
    for (int i = 0; i < totalFiles; ++i) {
        const QString& filePath = filePaths[i];
        std::vector<float> embedding = rm->getHTSATProcessor()->processAudio(filePath);
        if (embedding.empty()) {
            qDebug() << "Failed to process file:" << filePath;
            continue;
        }
        embeddings.append(embedding);
        int progress = (i + 1) * 100 / totalFiles;
        emit progressUpdated(progress);
    }

    if (embeddings.isEmpty()) {
        qDebug() << "No embeddings generated.";
        return QString();
    }

    // Define output folder
    QString outputFolder = "output_features";
    QDir dir(outputFolder);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create output folder:" << outputFolder;
            return QString();
        }
    }

    // Generate timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    // Construct base name
    QString baseName = QFileInfo(outputFileName).baseName();
    if (baseName.isEmpty()) {
        baseName = "output";
    }

    // Construct full output file path with timestamp
    QString finalOutputFileName = outputFolder + "/" + baseName + "_" + timestamp + ".txt";

    // Compute average embedding
    std::vector<float> avg_emb(embeddings[0].size(), 0.0f);
    for (const auto& emb : embeddings) {
        for (size_t i = 0; i < emb.size(); ++i) {
            avg_emb[i] += emb[i];
        }
    }
    for (size_t i = 0; i < avg_emb.size(); ++i) {
        avg_emb[i] /= embeddings.size();
    }

    // Save averaged embedding to file
    QFile file(finalOutputFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open output file:" << finalOutputFileName;
        return QString();
    }

    QTextStream out(&file);
    for (size_t i = 0; i < avg_emb.size(); ++i) {
        out << avg_emb[i];
        if (i < avg_emb.size() - 1) out << " ";
    }
    out << "\n";
    file.close();

    qDebug() << "Averaged embedding saved to:" << finalOutputFileName;

    return finalOutputFileName;
}
