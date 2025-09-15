#include "zero_shot_asp_feature_extractor.h"
#include <torch/script.h>
#include <torch/torch.h>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <algorithm>

/**
 * @brief Constructs the ZeroShotASPFeatureExtractor.
 * @param parent The parent QObject (default is nullptr).
 */
ZeroShotASPFeatureExtractor::ZeroShotASPFeatureExtractor(QObject *parent)
    : QObject(parent), modelLoaded(false)
{
}

/**
 * @brief Loads the TorchScript model from the specified path.
 * @param modelPath Path to the TorchScript model file (e.g., "zero_shot_asp_separation_model.pt").
 * @return True if loading succeeded, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::loadModel(const QString& modelPath)
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
 * @brief Processes waveform and condition tensors to generate separated audio.
 * @param waveform Input waveform tensor of shape (B, N, 1).
 * @param condition Condition embedding tensor of shape (B, 2048).
 * @param outputPath Optional output file path to save the separated audio as 32kHz WAV.
 * @return Separated waveform tensor of shape (B, N, 1), or empty tensor on failure.
 */
torch::Tensor ZeroShotASPFeatureExtractor::separateAudio(const torch::Tensor& waveform, const torch::Tensor& condition, const QString& outputPath)
{
    if (!modelLoaded) {
        emit errorOccurred("Model not loaded.");
        return torch::Tensor();
    }

    try {
        int64_t B = waveform.size(0);
        int64_t N = waveform.size(1);
        int64_t clip_samples = 320000;
        double overlap_rate = 0.5;
        int64_t hop = static_cast<int64_t>(clip_samples * (1.0 - overlap_rate));

        torch::Tensor output;
        if (N <= clip_samples) {
            // Single pass for short audio
            std::vector<torch::jit::IValue> inputs;
            inputs.push_back(waveform);
            inputs.push_back(condition);
            output = model.forward(inputs).toTensor();
        } else {
            // Overlap-Add for long audio
            auto window = torch::hann_window(clip_samples, torch::kFloat32).view({clip_samples, 1});
            output = torch::zeros_like(waveform);
            torch::Tensor weight = torch::zeros_like(waveform);

            for (int64_t start = 0; start < N; start += hop) {
                int64_t end = std::min(start + clip_samples, N);
                torch::Tensor seg = waveform.slice(1, start, end);
                if (seg.size(1) < clip_samples) {
                    // Pad the last segment with zeros
                    torch::Tensor padding = torch::zeros({B, clip_samples - seg.size(1), 1});
                    seg = torch::cat({seg, padding}, 1);
                }
                std::vector<torch::jit::IValue> inputs;
                inputs.push_back(seg);
                inputs.push_back(condition);
                torch::Tensor pred = model.forward(inputs).toTensor();
                pred *= window;
                output.slice(1, start, end) += pred.slice(1, 0, end - start);
                weight.slice(1, start, end) += window.slice(1, 0, end - start);
            }
            output /= (weight + 1e-8);
        }

        emit processingFinished(output);

        // Save to WAV if outputPath is provided
        if (!outputPath.isEmpty()) {
            if (B != 1) {
                emit errorOccurred("Saving WAV is only supported for batch size 1.");
            } else {
                torch::Tensor wav_tensor = output.squeeze(0).squeeze(1); // Shape (N,)
                if (!saveAsWav(wav_tensor, outputPath, 32000)) {
                    emit errorOccurred("Failed to save WAV file.");
                }
            }
        }

        return output;
    } catch (const c10::Error& e) {
        emit errorOccurred(QString("Model inference error: %1").arg(e.what()));
        return torch::Tensor();
    }
}

/**
 * @brief Checks if the model is loaded and ready for inference.
 * @return True if model is loaded, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::isModelLoaded() const
{
    return modelLoaded;
}

/**
 * @brief Saves a waveform tensor as a 32-bit float WAV file at the specified sample rate.
 * @param waveform Tensor of shape (N,) with float32 samples.
 * @param filePath Output file path.
 * @param sampleRate Sample rate in Hz (default 32000).
 * @return True if saving succeeded, false otherwise.
 */
bool ZeroShotASPFeatureExtractor::saveAsWav(const torch::Tensor& waveform, const QString& filePath, int sampleRate)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // WAV header for 32-bit float, mono
    int channels = 1;
    int bitsPerSample = 32;
    int dataSize = waveform.numel() * sizeof(float);
    int fileSize = 36 + dataSize;

    out.writeRawData("RIFF", 4);
    out << fileSize;
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << 16; // Subchunk1Size
    out << (short)3; // AudioFormat (3 for float)
    out << (short)channels;
    out << sampleRate;
    out << (sampleRate * channels * bitsPerSample / 8); // ByteRate
    out << (short)(channels * bitsPerSample / 8); // BlockAlign
    out << (short)bitsPerSample;
    out.writeRawData("data", 4);
    out << dataSize;

    // Write audio data
    auto data = waveform.contiguous().data_ptr<float>();
    out.writeRawData(reinterpret_cast<const char*>(data), dataSize);

    file.close();
    return true;
}
