#ifndef HTSATWORKER_H
#define HTSATWORKER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <vector>
#include <memory>
#include "htsatprocessor.h"

/**
 * @brief Worker class for HTSAT feature generation in a separate thread.
 *
 * Processes audio files to generate sound features using HTSAT model with improved
 * error handling, progress tracking, and structured output support.
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

    /**
     * @brief Destroys the HTSATWorker and cleans up resources.
     */
    ~HTSATWorker() override;

public slots:
    /**
     * @brief Slot to start generating audio features from a list of files.
     * @param filePaths List of audio file paths to process.
     * @param outputFileName Name for the output feature file.
     */
    void generateFeatures(const QStringList& filePaths, const QString& outputFileName);

signals:
    /**
     * @brief Signal emitted to update progress (0-100).
     * @param value Progress percentage.
     */
    void progressUpdated(int value);

    /**
     * @brief Signal emitted when processing is finished with the average embedding.
     * @param avgEmb The average embedding vector.
     * @param outputFileName Name of the output file.
     */
    void finished(const std::vector<float>& avgEmb, const QString& outputFileName);

    /**
     * @brief Signal emitted when an error occurs.
     * @param errorMessage Description of the error.
     */
    void error(const QString& errorMessage);

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
     * @param filePaths List of audio file paths.
     * @param processor Pointer to initialized HTSATProcessor instance.
     * @return Vector of successfully processed embeddings.
     */
    std::vector<std::vector<float>> processFilesAndCollectEmbeddings(
        const QStringList& filePaths,
        HTSATProcessor* processor
    );

    /**
     * @brief Processes a single audio file and extracts HTSAT features.
     * @param filePath Path to the audio file.
     * @param processor Pointer to the HTSAT processor.
     * @return Optional containing the embedding vector, or nullopt on failure.
     */
    std::optional<std::vector<float>> processSingleFile(
        const QString& filePath,
        HTSATProcessor* processor
    );

    /**
     * @brief Computes the average embedding from a collection of embeddings.
     * @param embeddings Vector of individual embeddings.
     * @return The average embedding.
     */
    std::vector<float> computeAverageEmbedding(const std::vector<std::vector<float>>& embeddings) const;

    /**
     * @brief Validates audio file metadata.
     * @param filePath Path to the audio file.
     * @param sampleRate Output parameter for sample rate.
     * @param channels Output parameter for number of channels.
     * @return True if file is valid, false otherwise.
     */
    bool validateAudioFile(const QString& filePath, int& sampleRate, int& channels) const;

    /**
     * @brief Extracts latent output from HTSAT structured output.
     * @param output The structured HTSAT output.
     * @return Vector containing the latent output data.
     */
    std::vector<float> extractLatentOutput(const HTSATOutput& output) const;

    std::unique_ptr<HTSATProcessor> processor_;  // Owned processor instance
};

#endif // HTSATWORKER_H
