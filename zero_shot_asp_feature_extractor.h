// ZeroShotASPFeatureExtractor.h
#pragma once

#ifndef Q_MOC_RUN
#undef slots
#endif
#include <torch/script.h>
#include <torch/autograd.h>
#ifndef Q_MOC_RUN
#define slots
#endif
#include <QString>
#include <QObject>

/**
 * @brief Feature extractor for zero-shot audio source separation using TorchScript model.
 *
 * Loads and runs a PyTorch model for audio separation based on given conditions.
 */
class ZeroShotASPFeatureExtractor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the ZeroShotASPFeatureExtractor.
     * @param parent The parent QObject (default is nullptr).
     */
    ZeroShotASPFeatureExtractor(QObject* parent = nullptr);
    ~ZeroShotASPFeatureExtractor() = default;

    /**
     * @brief Loads the TorchScript model from the specified path.
     * @param modelPath Path to the model file.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief Performs forward pass through the model.
     * @param waveform Input waveform tensor of shape (1, clip_samples, 1).
     * @param condition Condition tensor of shape (1, 2048).
     * @return Separated waveform tensor of shape (1, clip_samples, 1).
     */
    torch::Tensor forward(const torch::Tensor& waveform,
                          const torch::Tensor& condition);

    /**
     * @brief Unloads the model to free memory.
     */
    void unloadModel();

    /**
     * @brief Loads the model from a resource path.
     * @param resourcePath Path to the resource containing the model.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadModelFromResource(const QString& resourcePath);

signals:
    void finished(const torch::Tensor& output); ///< Signal emitted when processing is finished
    void error(const QString& errorMessage);    ///< Signal emitted when an error occurs

private:
    torch::jit::script::Module model; ///< The loaded TorchScript model
    bool modelLoaded;                 ///< Flag indicating if the model is loaded
};
