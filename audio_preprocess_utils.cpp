#include "audio_preprocess_utils.h"
#include <sndfile.h>
#include <samplerate.h>
#include <iostream>

namespace AudioPreprocessUtils {

torch::Tensor loadAudio(const QString& filePath) {
    SF_INFO sfinfo;
    SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfinfo);
    if (!file) {
        std::cerr << "Error opening file: " << filePath.toStdString() << " - " << sf_strerror(file) << std::endl;
        return torch::empty({0});
    }

    std::vector<float> audio(sfinfo.frames * sfinfo.channels);
    sf_count_t read = sf_read_float(file, audio.data(), audio.size());
    sf_close(file);

    if (read != audio.size()) {
        std::cerr << "Error reading file: " << filePath.toStdString() << " - read " << read << " samples, expected " << audio.size() << std::endl;
        return torch::empty({0});
    }

    // Create tensor from vector
    torch::Tensor tensor = torch::from_blob(audio.data(), {sfinfo.frames, sfinfo.channels}, torch::kFloat32).clone();

    // Resample to target sample rate if necessary
    if (sfinfo.samplerate != 32000) {
        tensor = resampleAudio(tensor, sfinfo.samplerate, 32000);
        if (tensor.numel() == 0) {
            std::cerr << "Failed to resample audio: " << filePath.toStdString() << std::endl;
            return torch::empty({0});
        }
    }

    return tensor;
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
    if (audio.numel() == 0 || originalSampleRate == targetSampleRate) return audio;

    auto sizes = audio.sizes();
    int64_t total_channels = 1;
    for (size_t i = 1; i < sizes.size(); ++i) total_channels *= sizes[i];

    if (total_channels == 1) {
        // Flatten to 1D for resampling
        torch::Tensor flatAudio = audio.flatten();
        std::vector<float> audioVec(flatAudio.data_ptr<float>(), flatAudio.data_ptr<float>() + flatAudio.numel());

        double ratio = static_cast<double>(targetSampleRate) / originalSampleRate;
        SRC_DATA srcData;
        srcData.data_in = audioVec.data();
        srcData.input_frames = audioVec.size() / total_channels;
        srcData.data_out = new float[static_cast<size_t>((audioVec.size() / total_channels) * ratio + 1) * total_channels];
        srcData.output_frames = static_cast<long>((audioVec.size() / total_channels) * ratio + 1);
        srcData.src_ratio = ratio;

        int error = src_simple(&srcData, SRC_SINC_BEST_QUALITY, total_channels);
        if (error) {
            std::cerr << "Resampling error: " << src_strerror(error) << std::endl;
            delete[] srcData.data_out;
            return torch::empty({0});
        }

        torch::Tensor resampled = torch::from_blob(srcData.data_out, {srcData.output_frames_gen * total_channels}, torch::kFloat32).clone();
        delete[] srcData.data_out;

        // Reshape back
        if (sizes.size() == 2) {
            resampled = resampled.view({srcData.output_frames_gen, sizes[1]});
        }

        return resampled;
    } else {
        // Multi-channel: resample each channel separately
        int64_t frames = sizes[0];
        int64_t channels = total_channels;
        std::vector<torch::Tensor> resampledChannels;
        for (int64_t ch = 0; ch < channels; ++ch) {
            torch::Tensor channelData = audio.index({torch::indexing::Slice(), ch});
            torch::Tensor resampledChannel = resampleAudio(channelData.unsqueeze(1), originalSampleRate, targetSampleRate);
            if (resampledChannel.numel() == 0) {
                std::cerr << "Failed to resample channel " << ch << std::endl;
                return torch::empty({0});
            }
            resampledChannels.push_back(resampledChannel.squeeze(1));
        }
        torch::Tensor resampled = torch::stack(resampledChannels, 1);
        return resampled;
    }
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

} // namespace AudioPreprocessUtils
