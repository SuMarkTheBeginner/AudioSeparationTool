#include "audio_preprocess_utils.h"
#include <sndfile.h>
#include <samplerate.h>
#include <iostream>
#include <QDir>
#include <QDebug>
#include <cmath>

namespace AudioPreprocessUtils {

torch::Tensor loadAudio(const QString& filePath) {
    qDebug() << "AudioPreprocessUtils::loadAudio - Loading file:" << filePath;

    SF_INFO sfinfo;
    SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfinfo);
    if (!file) {
        qDebug() << "AudioPreprocessUtils::loadAudio - Error opening file:" 
                 << filePath << "-" << sf_strerror(file);
        return torch::empty({0});
    }

    std::vector<float> audio(sfinfo.frames * sfinfo.channels);
    sf_count_t read = sf_read_float(file, audio.data(), audio.size());
    sf_close(file);

    if (read != audio.size()) {
        qDebug() << "AudioPreprocessUtils::loadAudio - Error reading file:" << filePath;
        return torch::empty({0});
    }

    // 建立 tensor (frames, channels)
    torch::Tensor tensor = torch::from_blob(
        audio.data(), {sfinfo.frames, sfinfo.channels}, torch::kFloat32
    ).clone();

    qDebug() << "AudioPreprocessUtils::loadAudio - Loaded tensor shape:"
             << tensor.size(0) << "x" << tensor.size(1);

    // === Step 1: Convert to mono using function ===
    tensor = AudioPreprocessUtils::convertToMono(tensor, sfinfo.channels); // 返回 shape=(frames,1)
    tensor = tensor.squeeze(1); // shape=(frames,)
    qDebug() << "AudioPreprocessUtils::loadAudio - Converted to mono, new shape:" << tensor.size(0);

    // === Step 2: Resample if needed ===
    if (sfinfo.samplerate != 32000) {
        qDebug() << "AudioPreprocessUtils::loadAudio - Resampling from"
                 << sfinfo.samplerate << "to 32000 Hz";

        tensor = resampleAudio(tensor.unsqueeze(0), sfinfo.samplerate, 32000).squeeze(0);
        if (tensor.numel() == 0) {
            qDebug() << "AudioPreprocessUtils::loadAudio - Failed to resample audio:" << filePath;
            return torch::empty({0});
        }
        qDebug() << "AudioPreprocessUtils::loadAudio - Resampled tensor length:" << tensor.size(0);
    }

    return tensor; // shape = (frames)
}



torch::Tensor normalizeAudio(const torch::Tensor& audio, float targetMax) {
    if (audio.numel() == 0) return audio;

    torch::Tensor absAudio = torch::abs(audio);
    float maxVal = absAudio.max().item<float>();
    if (maxVal == 0.0f) return audio;

    float scale = targetMax / maxVal;
    return audio * scale;
}

torch::Tensor resampleAudio(const torch::Tensor& audio, int originalSampleRate, int targetSampleRate) {
    if (audio.numel() == 0 || originalSampleRate == targetSampleRate) {
        return audio.clone();
    }

    // 確保輸入是 1D
    torch::Tensor flatAudio = audio.flatten().contiguous();  // (frames)
    int64_t frames = flatAudio.size(0);

    std::vector<float> inputVec(flatAudio.data_ptr<float>(),
                                flatAudio.data_ptr<float>() + frames);

    double ratio = static_cast<double>(targetSampleRate) / originalSampleRate;
    long outputFrames = static_cast<long>(frames * ratio + 1);

    std::vector<float> outputVec(outputFrames);

    SRC_DATA srcData;
    srcData.data_in = inputVec.data();
    srcData.input_frames = frames;
    srcData.data_out = outputVec.data();
    srcData.output_frames = outputFrames;
    srcData.src_ratio = ratio;

    int error = src_simple(&srcData, SRC_SINC_BEST_QUALITY, 1);
    if (error) {
        std::cerr << "Resampling error: " << src_strerror(error) << std::endl;
        return torch::empty({0});
    }

    // 輸出 tensor (1D)
    torch::Tensor resampled = torch::from_blob(
        outputVec.data(),
        {srcData.output_frames_gen},
        torch::kFloat32
    ).clone();

    return resampled;
}

torch::Tensor convertToMono(const torch::Tensor& audio, int numChannels) {
    if (audio.numel() == 0) return audio;

    auto sizes = audio.sizes();
    if (sizes.size() == 1) {
        // Assume 1D is mono
        return audio.unsqueeze(1);
    } else if (sizes.size() == 2) {
        if (sizes[1] == 1) {
            return audio;
        } else if (sizes[1] == numChannels && numChannels > 1) {
            return torch::mean(audio, 1, true);
        } else {
            std::cerr << "Audio tensor shape mismatch for convertToMono: expected channels " << numChannels << ", got " << sizes[1] << std::endl;
            return torch::empty({0});
        }
    } else {
        std::cerr << "Audio tensor shape mismatch for convertToMono: expected 1D or 2D, got " << sizes.size() << "D" << std::endl;
        return torch::empty({0});
    }
}

torch::Tensor extractChannel(const torch::Tensor& audio, int channelIndex, int numChannels) {
    if (audio.numel() == 0 || channelIndex < 0 || channelIndex >= numChannels) return torch::empty({0});

    // Assume audio is (frames, channels)
    auto sizes = audio.sizes();
    if (sizes.size() != 2 || sizes[1] != numChannels) {
        std::cerr << "Audio tensor shape mismatch for extractChannel" << std::endl;
        return torch::empty({0});
    }

    return audio.index({torch::indexing::Slice(), channelIndex}).unsqueeze(1);
}

torch::Tensor trimSilence(const torch::Tensor& audio, float threshold) {
    if (audio.numel() == 0) return audio;

    torch::Tensor absAudio = torch::abs(audio);
    torch::Tensor aboveThreshold = absAudio > threshold;

    // Find first and last true indices
    auto aboveVec = aboveThreshold.flatten().to(torch::kBool);
    auto indices = torch::nonzero(aboveVec).flatten();

    if (indices.numel() == 0) return torch::empty({0});

    int64_t start = indices[0].item<int64_t>();
    int64_t end = indices[-1].item<int64_t>() + 1;

    return audio.slice(0, start, end);
}

bool saveToWav(const torch::Tensor& audio, const QString& filePath, int sampleRate) {
    if (audio.numel() == 0) {
        std::cerr << "Empty audio tensor, cannot save to WAV: " << filePath.toStdString() << std::endl;
        return false;
    }

    // Ensure audio is 2D (frames, channels)
    torch::Tensor audio2d;
    if (audio.dim() == 1) {
        audio2d = audio.unsqueeze(1);
    } else if (audio.dim() == 2) {
        audio2d = audio;
    } else {
        std::cerr << "Audio tensor must be 1D or 2D to save to WAV: " << filePath.toStdString() << std::endl;
        return false;
    }

    SF_INFO sfinfo;
    sfinfo.samplerate = sampleRate;
    sfinfo.channels = static_cast<int>(audio2d.size(1));
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_WRITE, &sfinfo);
    if (!file) {
        std::cerr << "Failed to open WAV file for writing: " << filePath.toStdString() << " - " << sf_strerror(file) << std::endl;
        return false;
    }

    // Convert float tensor to vector<float>
    std::vector<float> audioVec(audio2d.numel());
    std::memcpy(audioVec.data(), audio2d.data_ptr<float>(), audio2d.numel() * sizeof(float));

    sf_count_t written = sf_write_float(file, audioVec.data(), audioVec.size());
    sf_close(file);

    if (written != static_cast<sf_count_t>(audioVec.size())) {
        std::cerr << "Failed to write all samples to WAV file: " << filePath.toStdString() << std::endl;
        return false;
    }

    return true;
}

torch::Tensor applyHannWindow(const torch::Tensor& chunk) {
    if (chunk.numel() == 0) return chunk;

    int64_t length = chunk.size(0);
    torch::Tensor window = torch::hann_window(length, torch::kFloat32);
    return chunk * window;
}

} // namespace AudioPreprocessUtils

