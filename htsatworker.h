#ifndef HTSATWORKER_H
#define HTSATWORKER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <vector>
#include "htsatprocessor.h"

/**
 * @brief Worker class for HTSAT feature generation in a separate thread.
 *
 * Processes audio files to generate sound features using HTSAT model.
 */
class HTSATWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the HTSATWorker.
     * @param parent The parent QObject (default is nullptr).
     */
    explicit HTSATWorker(QObject *parent = nullptr);


public slots:
    /**
     * @brief Slot to start generating audio features from a list of files.
     * @param filePaths List of audio file paths to process.
     * @param outputFileName Name for the output feature file.
     */
    void generateFeatures(const QStringList& filePaths, const QString& outputFileName);


signals:
    void progressUpdated(int value); ///< Signal emitted to update progress (0-100)
    void finished(const std::vector<float>& avgEmb, const QString& outputFileName); ///< Signal emitted when processing is finished with the average embedding
    void error(const QString& errorMessage); ///< Signal emitted when an error occurs

private:
    /**
     * @brief Generates audio features from the provided file paths.
     * @param filePaths List of audio file paths.
     * @param outputFileName Name for the output file.
     * @return The average embedding as a vector of floats.
     */
    std::vector<float> doGenerateAudioFeatures(const QStringList& filePaths, const QString& outputFileName);

    /**
     * @brief Batch processes multiple audio files to extract HTSAT embeddings.
     *
     * This method iterates through a list of audio files, processes each one through the HTSAT
     * (Hierarchical Token-Semantic Audio Transformer) model to extract feature embeddings, and
     * collects all successfully processed embeddings into a vector. The method handles audio
     * loading, preprocessing, and error management gracefully - if individual files fail to process,
     * they are skipped while others continue to be processed.
     *
     * Processing pipeline for each audio file:
     * - Extracts audio metadata (sample rate, channels) using libsndfile
     * - Loads raw audio tensor using AudioPreprocessUtils (1D tensor)
     * - Reshapes tensor to (frames, 1) for HTSAT model input requirements
     * - Runs inference through HTSATProcessor to generate embedding vector
     * - Performs basic validation on extracted embeddings
     * - Accumulates valid embeddings for averaging
     * - Issues progress updates for UI feedback
     *
     * The method supports processing of both mono and stereo audio files, though the HTSAT
     * model operates on single-channel representations. Stereo files may be downmixed or
     * processed per channel depending on AudioPreprocessUtils implementation.
     *
     * @param filePaths StringList of absolute paths to audio files (WAV/MP3 formats supported)
     * @param processor Pointer to initialized HTSATProcessor instance with loaded model
     * @return QVector containing one embedding vector (std::vector<float>) per successfully
     *         processed audio file. Empty strings at indices indicate processing failures
     *         which were skipped. Returns empty QVector if no files processed successfully.
     *
     * @note Fails gracefully - individual file processing errors don't halt the entire batch.
     * @note Embedding vectors are of fixed size determined by HTSAT model architecture.
     * @note Progress updates emitted incrementally: current_file_index/total_files * 100.
     * @note Memory usage scales with number of files and audio file sizes.
     * @note Thread-safe: can be called from worker thread context.
     * @note Processor pointer must remain valid throughout the operation.
     *
     * @see HTSATProcessor::processTensor, AudioPreprocessUtils::loadAudio, computeAverageEmbedding
     */
    QVector<std::vector<float>> processFilesAndCollectEmbeddings(const QStringList& filePaths, HTSATProcessor* processor);

    /**
     * @brief Computes the average embedding from a collection of embeddings.
     * @param embeddings Vector of individual embeddings.
     * @return The average embedding.
     */
    std::vector<float> computeAverageEmbedding(const QVector<std::vector<float>>& embeddings);
};

#endif // HTSATWORKER_H
