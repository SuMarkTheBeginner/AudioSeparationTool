#ifndef SEPARATIONWORKER_H
#define SEPARATIONWORKER_H

#include <QObject>
#include <QString>

class SeparationWorker : public QObject
{
    Q_OBJECT

public:
    explicit SeparationWorker(QObject *parent = nullptr);

public slots:
    void processFile(const QString& audioPath, const QString& featureName);

signals:
    void progressUpdated(int value);
    void finished(const QStringList& separatedFiles);
    void error(const QString& errorMessage);

private:
    QStringList doProcessAndSaveSeparatedChunks(const QString& audioPath, const QString& featureName);
};

#endif // SEPARATIONWORKER_H
