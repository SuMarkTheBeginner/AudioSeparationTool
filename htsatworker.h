#ifndef HTSATWORKER_H
#define HTSATWORKER_H

#include <QStringList>
#include <QVector>
#include <vector>
#include "htsatprocessor.h"

class HTSATWorker
{
public:
    explicit HTSATWorker();
    ~HTSATWorker();

    // Synchronous method to generate features
    std::vector<float> generateFeatures(const QStringList& filePaths);

private:
    std::vector<float> doGenerateAudioFeatures(const QStringList& filePaths);
    QVector<std::vector<float>> processFilesAndCollectEmbeddings(const QStringList& filePaths, HTSATProcessor* processor);
    std::vector<float> computeAverageEmbedding(const QVector<std::vector<float>>& embeddings);
};

#endif // HTSATWORKER_H
