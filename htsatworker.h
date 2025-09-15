#ifndef HTSATWORKER_H
#define HTSATWORKER_H

#include <QObject>
#include <QStringList>

class HTSATWorker : public QObject
{
    Q_OBJECT

public:
    explicit HTSATWorker(QObject *parent = nullptr);

public slots:
    void generateFeatures(const QStringList& filePaths, const QString& outputFileName);

signals:
    void progressUpdated(int value);
    void finished(const QString& resultFile);
    void error(const QString& errorMessage);

private:
    QString doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName);
};

#endif // HTSATWORKER_H
