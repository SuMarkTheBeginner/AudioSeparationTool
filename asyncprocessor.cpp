#include "asyncprocessor.h"
#include <QThread>
#include <QMetaObject>
#include "htsatworker.h"
#include "separationworker.h"
#include "audioserializer.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include "constants.h"
#include "logger.h"

/**
 * @brief Constructs the AsyncProcessor and initializes thread/worker management.
 * @param serializer AudioSerializer instance for result saving (owned externally).
 * @param parent Optional parent QObject.
 */
AsyncProcessor::AsyncProcessor(AudioSerializer* serializer, QObject* parent)
    : QObject(parent)
    , m_serializer(serializer)
{
    Logger::log(Logger::Level::Info, "Initializing AsyncProcessor");
    initializeThreadsAndWorkers();
}

/**
 * @brief Destructor - safely cleans up all threads and workers.
 */
AsyncProcessor::~AsyncProcessor()
{
    Logger::log(Logger::Level::Info, "Shutting down AsyncProcessor");
    cleanupThreadsAndWorkers();
}

/**
 * @brief Initialize threads and worker objects.
 * Sets up HTSAT and separation workers in separate threads with proper signal connections.
 */
void AsyncProcessor::initializeThreadsAndWorkers()
{
    Logger::log(Logger::Level::Debug, "Setting up HTSAT thread and worker");

    // Initialize HTSAT thread and worker
    m_htsatThread = new QThread(this);
    m_htsatWorker = new HTSATWorker();
    m_htsatWorker->moveToThread(m_htsatThread);

    connect(m_htsatThread, &QThread::finished, m_htsatWorker, &QObject::deleteLater);
    connect(this, &AsyncProcessor::processingStarted, m_htsatWorker, [this]() {
        Logger::log(Logger::Level::Debug, "HTSAT processing started signal received");
    });

    // Connect HTSAT worker signals to AsyncProcessor signals
    connect(m_htsatWorker, &HTSATWorker::progressUpdated, this, &AsyncProcessor::processingProgress);
    connect(m_htsatWorker, &HTSATWorker::finished, this, [this](const std::vector<float>& avgEmb, const QString& outputFileName) {
        Logger::log(Logger::Level::Debug, "HTSAT worker finished, saving embedding");

        QString outputFolder = Constants::OUTPUT_FEATURES_DIR;
        QDir dir(outputFolder);
        if (!dir.exists() && !dir.mkpath(".")) {
            m_isProcessing = false;
            emit processingError("Failed to create output folder");
            Logger::log(Logger::Level::Error, "Failed to create HTSAT output folder");
            return;
        }

        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString baseName = QFileInfo(outputFileName).baseName();
        if (baseName.isEmpty()) {
            baseName = "output";
        }

        QString filePath = outputFolder + "/" + baseName + "_" + timestamp + ".txt";
        int counter = 1;
        while (QFile::exists(filePath)) {
            filePath = outputFolder + "/" + baseName + "_" + timestamp + "_" + QString::number(counter) + ".txt";
            ++counter;
        }

        if (m_serializer->saveEmbeddingToFile(avgEmb, filePath)) {
            m_isProcessing = false;
            QStringList results;
            results << filePath;
            emit processingFinished(results);
            Logger::log(Logger::Level::Info, QString("HTSAT processing completed successfully: %1").arg(filePath));
        } else {
            m_isProcessing = false;
            emit processingError("Failed to save embedding file");
            Logger::log(Logger::Level::Error, "Failed to save HTSAT embedding file");
        }
    });

    connect(m_htsatWorker, &HTSATWorker::error, this, [this](const QString& error) {
        Logger::log(Logger::Level::Error, QString("HTSAT worker error: %1").arg(error));
        m_isProcessing = false;
        emit processingError(error);
    });

    // Setup direct signal connection for HTSAT processing (similar to ResourceManager)
    connect(this, &AsyncProcessor::processingStarted, m_htsatWorker, [this]() {
        // This connection will be established when startFeatureGeneration is called
    }, Qt::QueuedConnection);

    m_htsatThread->start();

    Logger::log(Logger::Level::Debug, "Setting up separation thread and worker");

    // Initialize separation thread and worker
    m_separationThread = new QThread(this);
    m_separationWorker = new SeparationWorker();
    m_separationWorker->moveToThread(m_separationThread);

    connect(m_separationThread, &QThread::finished, m_separationWorker, &QObject::deleteLater);

    // Connect separation worker signals
    connect(m_separationWorker, &SeparationWorker::progressUpdated, this, &AsyncProcessor::processingProgress);
    connect(m_separationWorker, &SeparationWorker::finished, this, &AsyncProcessor::handleFinalResult);
    connect(m_separationWorker, &SeparationWorker::error, this, [this](const QString& error) {
        Logger::log(Logger::Level::Error, QString("Separation worker error: %1").arg(error));
        m_isProcessing = false;
        emit processingError(error);
    });

    // Direct signal connection for separation processing
    connect(this, &AsyncProcessor::processingStarted, m_separationWorker, [this]() {
        // This connection will be established when startAudioSeparation is called
    }, Qt::QueuedConnection);

    m_separationThread->start();

    Logger::log(Logger::Level::Info, "AsyncProcessor threads initialized successfully");
}

/**
 * @brief Safely clean up threads and workers.
 * Stops threads and waits for completion before deletion.
 */
void AsyncProcessor::cleanupThreadsAndWorkers()
{
    Logger::log(Logger::Level::Debug, "Cleaning up AsyncProcessor threads and workers");

    // Clean up HTSAT thread
    if (m_htsatThread) {
        m_htsatThread->quit();
        m_htsatThread->wait();
        m_htsatThread = nullptr;
        m_htsatWorker = nullptr;
    }

    // Clean up separation thread
    if (m_separationThread) {
        m_separationThread->quit();
        m_separationThread->wait();
        m_separationThread = nullptr;
        m_separationWorker = nullptr;
    }

    Logger::log(Logger::Level::Info, "AsyncProcessor cleanup completed");
}

/**
 * @brief Start asynchronous generation of audio features using HTSAT.
 * @param filePaths List of audio file paths to process.
 * @param outputFileName The name for the output feature file.
 */
void AsyncProcessor::startFeatureGeneration(const QStringList& filePaths, const QString& outputFileName)
{
    if (m_isProcessing) {
        Logger::log(Logger::Level::Warning, "Cannot start HTSAT processing - already processing");
        return;
    }

    Logger::log(Logger::Level::Info, QString("Starting HTSAT feature generation for %1 files").arg(filePaths.size()));
    m_isProcessing = true;

    // Disconnect any previous connection and establish new one for HTSAT
    disconnect(this, &AsyncProcessor::processingStarted, m_htsatWorker, nullptr);
    disconnect(this, &AsyncProcessor::processingStarted, m_separationWorker, nullptr);
    connect(this, &AsyncProcessor::processingStarted, m_htsatWorker, [this, filePaths, outputFileName]() {
        QMetaObject::invokeMethod(m_htsatWorker, "generateFeatures",
                                  Qt::QueuedConnection,
                                  Q_ARG(QStringList, filePaths),
                                  Q_ARG(QString, outputFileName));
    });

    // Trigger the processing
    emit processingStarted();
}

/**
 * @brief Start asynchronous audio separation using the specified feature.
 * @param filePaths List of audio file paths to separate.
 * @param featureName The name of the feature file to use for separation.
 */
void AsyncProcessor::startAudioSeparation(const QStringList& filePaths, const QString& featureName)
{
    if (m_isProcessing) {
        Logger::log(Logger::Level::Warning, "Cannot start separation processing - already processing");
        return;
    }

    Logger::log(Logger::Level::Info, QString("Starting audio separation for %1 files using feature: %2")
                .arg(filePaths.size()).arg(featureName));
    m_isProcessing = true;

    // Disconnect any previous connection and establish new one for separation
    disconnect(this, &AsyncProcessor::processingStarted, m_htsatWorker, nullptr);
    disconnect(this, &AsyncProcessor::processingStarted, m_separationWorker, nullptr);
    connect(this, &AsyncProcessor::processingStarted, m_separationWorker, [this, filePaths, featureName]() {
        QMetaObject::invokeMethod(m_separationWorker, "processFile",
                                  Qt::QueuedConnection,
                                  Q_ARG(QStringList, filePaths),
                                  Q_ARG(QString, featureName));
    });

    // Trigger the processing
    emit processingStarted();
}

/**
 * @brief Check if any processing is currently running.
 * @return True if processing is active, false otherwise.
 */
bool AsyncProcessor::isProcessing() const
{
    return m_isProcessing;
}

/**
 * @brief Handle the final separated audio result.
 *
 * Saves the separated audio as a WAV file and emits completion signals.
 * Output filename follows convention: {original_filename}_{feature_name}.wav
 *
 * @param audioPath Path to the original input audio file.
 * @param featureName Name of the separation feature used.
 * @param finalTensor Separated audio tensor (channels, samples) at 32kHz.
 */
void AsyncProcessor::handleFinalResult(const QString& audioPath,
                                     const QString& featureName,
                                     const torch::Tensor& finalTensor)
{
    Logger::log(Logger::Level::Debug, QString("Handling final result for %1 with feature %2").arg(audioPath).arg(featureName));

    QString outputName = QFileInfo(audioPath).baseName() + "_" + featureName + ".wav";
    QString outputPath = Constants::SEPARATED_RESULT_DIR + "/" + outputName;

    Logger::log(Logger::Level::Debug, QString("Saving separation result to: %1").arg(outputPath));

    bool saveResult = m_serializer->saveWavToFile(finalTensor, outputPath);
    if (saveResult) {
        Logger::log(Logger::Level::Info, QString("Successfully saved separation result: %1").arg(outputPath));
    } else {
        Logger::log(Logger::Level::Error, QString("Failed to save separation result: %1").arg(outputPath));
    }

    m_isProcessing = false;
    QStringList results;
    results << outputPath;
    emit separationProcessingFinished(results);
}
