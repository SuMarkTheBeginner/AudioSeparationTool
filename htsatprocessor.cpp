#include "htsatprocessor.h"
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
        return false;
    }
}

/**
 * @brief Reads and resamples audio file to 32kHz.
 * @param audioPath Path to the audio file.
 * @return Resampled audio data as vector of floats.
 */
std::vector<float> HTSATProcessor::readAndResampleAudio(const QString& audioPath)
{
    std::vector<float> resampledData;

    SF_INFO sfinfo;
    SNDFILE* infile = sf_open(audioPath.toStdString().c_str(), SFM_READ, &sfinfo);
    if (!infile) {
        emit errorOccurred(QString("Failed to open audio file: %1").arg(audioPath));
        return resampledData;
    }

    // Read original audio data
    std::vector<float> inputData(sfinfo.frames * sfinfo.channels);
    sf_count_t numRead = sf_readf_float(infile, inputData.data(), sfinfo.frames);
    sf_close(infile);

    if (numRead != sfinfo.frames) {
        emit errorOccurred(QString("Failed to read complete audio data from: %1").arg(audioPath));
        return resampledData;
    }

    // Convert to mono if needed by averaging channels
    if (sfinfo.channels > 1) {
        std::vector<float> monoData(sfinfo.frames);
        for (sf_count_t i = 0; i < sfinfo.frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < sfinfo.channels; ++ch) {
                sum += inputData[i * sfinfo.channels + ch];
            }
            monoData[i] = sum / sfinfo.channels;
        }
        inputData = std::move(monoData);
    }

    // Resample to 32000 Hz if needed
    if (sfinfo.samplerate != 32000) {
        double srcRatio = 32000.0 / sfinfo.samplerate;
        size_t outputFrames = static_cast<size_t>(inputData.size() * srcRatio) + 1;
        resampledData.resize(outputFrames);

        SRC_DATA srcData;
        srcData.data_in = inputData.data();
        srcData.input_frames = static_cast<long>(inputData.size());
        srcData.data_out = resampledData.data();
        srcData.output_frames = static_cast<long>(outputFrames);
        srcData.src_ratio = srcRatio;
        srcData.end_of_input = 1;

        int error = src_simple(&srcData, SRC_SINC_BEST_QUALITY, 1);
        if (error) {
            emit errorOccurred(QString("Resampling error: %1").arg(src_strerror(error)));
            resampledData.clear();
            return resampledData;
        }

        resampledData.resize(srcData.output_frames_gen);
    } else {
        resampledData = std::move(inputData);
    }

    return resampledData;
}

/**
 * @brief Prepares the tensor for model input.
 * @param audioData The resampled audio data.
 * @return The prepared tensor.
 */
torch::Tensor HTSATProcessor::prepareTensor(const std::vector<float>& audioData)
{
    // The model expects input shape (B, T) with T = 320000 (10 seconds at 32kHz)
    // If audioData is shorter, pad with zeros; if longer, truncate
    const int64_t expectedLength = 320000;
    std::vector<float> buffer(expectedLength, 0.0f);

    size_t copyLength = std::min(static_cast<size_t>(expectedLength), audioData.size());
    std::copy(audioData.begin(), audioData.begin() + copyLength, buffer.begin());

    // Create tensor from buffer
    torch::Tensor tensor = torch::from_blob(buffer.data(), {1, expectedLength}, torch::kFloat32).clone();

    return tensor;
}

/**
 * @brief Processes an audio file to generate an embedding.
 * @param audioPath Path to the audio file (WAV format).
 * @return The generated embedding as a vector of floats, or empty vector on failure.
 */
std::vector<float> HTSATProcessor::processAudio(const QString& audioPath)
{
    if (!modelLoaded) {
        emit errorOccurred("Model not loaded.");
        return {};
    }

    std::vector<float> audioData = readAndResampleAudio(audioPath);
    if (audioData.empty()) {
        return {};
    }

    torch::Tensor inputTensor = prepareTensor(audioData);

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(inputTensor);

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
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool HTSATProcessor::isModelLoaded() const
{
    return modelLoaded;
}
