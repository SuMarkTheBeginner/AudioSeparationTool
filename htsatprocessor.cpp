#include "htsatprocessor.h"
#include "constants.h"
#include <torch/script.h>
#include <torch/torch.h>
#include <QString>
#include <QDebug>
#include <sndfile.h>
#include <samplerate.h>

/**
 * @brief Constructs the HTSATProcessor.
 * @param parent The parent QObject (default is nullptr).
 */
HTSATProcessor::HTSATProcessor(QObject *parent)
    : QObject(parent), modelLoaded(false)
{
}

/**
 * @brief Loads the TorchScript model from the specified path.
 * @param modelPath Path to the TorchScript model file (e.g., "htsat_embedding_model.pt").
 * @return True if loading succeeded, false otherwise.
 */
bool HTSATProcessor::loadModel(const QString& modelPath)
{
    try {
        model = torch::jit::load(modelPath.toStdString());
        model.eval();
        modelLoaded = true;
        return true;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Error loading model: %1").arg(e.what()));
        modelLoaded = false;
    }
}

/**
 * @brief Processes a preprocessed audio tensor to generate an embedding.
 * @param audioTensor The preprocessed audio tensor (mono, 32kHz).
 * @return The generated embedding as a vector of floats, or empty vector on failure.
 */
std::vector<float> HTSATProcessor::processTensor(const torch::Tensor& audioTensor)
{
    if (!modelLoaded) {
        emit errorOccurred("Model not loaded.");
        return {};
    }

    // The model expects input shape (B, T) with T = AUDIO_CLIP_SAMPLES (10 seconds at 32kHz)
    const int64_t expectedLength = Constants::AUDIO_CLIP_SAMPLES;

    // Assume audioTensor is (frames, channels), convert to (1, frames) assuming mono
    torch::Tensor reshaped = audioTensor.squeeze(1).unsqueeze(0); // (1, frames)

    torch::Tensor tensor;

    if (reshaped.size(1) == expectedLength) {
        tensor = reshaped;
    } else if (reshaped.size(1) < expectedLength) {
        // Pad with zeros
        tensor = torch::constant_pad_nd(reshaped, {0, expectedLength - reshaped.size(1)}, 0);
    } else {
        // Truncate
        tensor = reshaped.narrow(1, 0, expectedLength);
    }

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor);

    try {
        torch::Tensor output = model.forward(inputs).toTensor();
        // Output shape: (1, 2048)
        std::vector<float> embedding(output.data_ptr<float>(), output.data_ptr<float>() + output.numel());
        emit processingFinished(embedding);
        return embedding;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Model inference error: %1").arg(e.what()));
        return {};
    }
}

/**
 * @brief Processes an audio file to generate an embedding.
 * @param audioPath Path to the audio file (WAV format).
 * @return The generated embedding as a vector of floats, or empty vector on failure.
 */
std::vector<float> HTSATProcessor::processAudio(const QString& audioPath)
{
    // Deprecated: Use processTensor instead
    emit errorOccurred("processAudio is deprecated. Use processTensor instead.");
    return {};
}

/**
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool HTSATProcessor::isModelLoaded() const
{
    return modelLoaded;
}
