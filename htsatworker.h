#ifndef HTSATWORKER_H
#define HTSATWORKER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <vector>
#include "htsatprocessor.h"

class HTSATWorker : public QObject
{
    Q_OBJECT

public:
    explicit HTSATWorker(QObject *parent = nullptr);
    

public slots:
    void generateFeatures(const QStringList& filePaths, const QString& outputFileName);
    

signals:
    void progressUpdated(int value);
    void finished(const std::vector<float>& avgEmb, const QString& outputFileName);
    void error(const QString& errorMessage);

private:
    std::vector<float> doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName);
    QVector<std::vector<float>> processFilesAndCollectEmbeddings(const QStringList& filePaths, HTSATProcessor* processor);
    std::vector<float> computeAverageEmbedding(const QVector<std::vector<float>>& embeddings);
};

#endif // HTSATWORKER_H
