#include "audio_preprocess_utils.h"
#include <sndfile.h>
#include <samplerate.h>
#include <iostream>
#include <QDir>
#include <QDebug>
#include <cmath>

namespace AudioPreprocessUtils {

torch::Tensor loadAudio(const QString& filePath, bool forceMono) {
    qDebug() << "AudioPreprocessUtils::loadAudio - Loading file:" << filePath
             << ", forceMono:" << forceMono;

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

    qDebug() << "AudioPreprocessUtils::loadAudio - Initial tensor shape:"
             << tensor.size(0) << "x" << tensor.size(1);
    qDebug() << "AudioPreprocessUtils::loadAudio - Channels:" << sfinfo.channels << "Frames:" << sfinfo.frames;

    // === Step 1: Convert to mono if requested ===
    if (forceMono) {
        qDebug() << "AudioPreprocessUtils::loadAudio - Converting to mono, original shape: [" << tensor.size(0) << (tensor.dim() > 1 ? "," : "") << (tensor.dim() > 1 ? QString::number(tensor.size(1)) : "") << "]";
        tensor = AudioPreprocessUtils::convertToMono(tensor, sfinfo.channels); // 返回 shape=(frames,1)

        // Validate conversion result
        if (tensor.numel() == 0) {
            qCritical() << "AudioPreprocessUtils::loadAudio - Failed to convert to mono:" << filePath;
            return torch::empty({0});
        }

        // Only squeeze if we have a channel dimension to remove
        if (tensor.dim() == 2 && tensor.size(1) == 1) {
            tensor = tensor.squeeze(1); // shape=(frames,)
            qDebug() << "AudioPreprocessUtils::loadAudio - Converted to mono, new shape:" << tensor.size(0);
        } else {
            qDebug() << "AudioPreprocessUtils::loadAudio - Already mono or unexpected shape after conversion: [" << tensor.size(0) << (tensor.dim() > 1 ? "," : "") << (tensor.dim() > 1 ? QString::number(tensor.size(1)) : "") << "]";
        }
    }

    // === Step 2: Resample if needed ===
    if (sfinfo.samplerate != 32000) {
        qDebug() << "AudioPreprocessUtils::loadAudio - Resampling from"
                 << sfinfo.samplerate << "to 32000 Hz";

        if (tensor.dim() == 1) {
            // Mono case
            tensor = resampleAudio(tensor.unsqueeze(0), sfinfo.samplerate, 32000).squeeze(0);
        } else {
            // Stereo case: resample each channel separately
            std::vector<torch::Tensor> resampledChannels;
            int numChannels = tensor.size(1);
            for (int c = 0; c < numChannels; ++c) {
                torch::Tensor channel = tensor.index({torch::indexing::Slice(), c});
                torch::Tensor resampledChannel = resampleAudio(channel.unsqueeze(0), sfinfo.samplerate, 32000).squeeze(0);
                resampledChannels.push_back(resampledChannel);
            }
            tensor = torch::stack(resampledChannels, 1);
        }
        if (tensor.numel() == 0) {
            qDebug() << "AudioPreprocessUtils::loadAudio - Failed to resample audio:" << filePath;
            return torch::empty({0});
        }
        qDebug() << "AudioPreprocessUtils::loadAudio - Resampled tensor shape:" << tensor.size(0) << "x" << (tensor.dim() > 1 ? tensor.size(1) : 1);
    }

    return tensor; // shape = (frames,) for mono or (frames, channels) for stereo
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
        // Already 1D mono - return as is (frames,)
        qDebug() << "AudioPreprocessUtils::convertToMono - Input already mono 1D, returning as-is";
        return audio;
    } else if (sizes.size() == 2) {
        if (sizes[1] == 1) {
            // Already mono 2D - return as is (frames, 1)
            qDebug() << "AudioPreprocessUtils::convertToMono - Input already mono 2D, returning as-is";
            return audio;
        } else if (sizes[1] == numChannels && numChannels > 1) {
            // Convert stereo/multi-channel to mono
            qDebug() << "AudioPreprocessUtils::convertToMono - Converting from" << numChannels << "channels to mono";
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
    // Enhanced logging for debugging
    qDebug() << "AudioPreprocessUtils::saveToWav - Starting save operation";
    qDebug() << "AudioPreprocessUtils::saveToWav - Input tensor shape: [" << audio.size(0) << (audio.dim() > 1 ? "," : "") << (audio.dim() > 1 ? QString::number(audio.size(1)) : "") << "]";
    qDebug() << "AudioPreprocessUtils::saveToWav - Input tensor numel:" << audio.numel();
    qDebug() << "AudioPreprocessUtils::saveToWav - File path:" << filePath;
    qDebug() << "AudioPreprocessUtils::saveToWav - Sample rate:" << sampleRate;

    // Validate input parameters
    if (filePath.isEmpty()) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Empty file path provided";
        return false;
    }

    if (sampleRate <= 0) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Invalid sample rate:" << sampleRate;
        return false;
    }

    if (sampleRate > 192000) {
        qWarning() << "AudioPreprocessUtils::saveToWav - Sample rate" << sampleRate << "is unusually high, but proceeding";
    }

    // Validate tensor
    if (audio.numel() == 0) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Empty audio tensor, cannot save to WAV:" << filePath;
        return false;
    }

    // Check for invalid tensor values (NaN or Inf)
    if (torch::isnan(audio).any().item<bool>()) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Audio tensor contains NaN values, cannot save to WAV:" << filePath;
        return false;
    }

    if (torch::isinf(audio).any().item<bool>()) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Audio tensor contains infinite values, cannot save to WAV:" << filePath;
        return false;
    }

    // Ensure audio is 2D (frames, channels) with proper validation
    torch::Tensor audio2d;
    int64_t numFrames = 0;
    int64_t numChannels = 0;

    try {
        if (audio.dim() == 1) {
            // Mono audio: (frames,) -> (frames, 1)
            numFrames = audio.size(0);
            numChannels = 1;
            audio2d = audio.unsqueeze(1);
            qDebug() << "AudioPreprocessUtils::saveToWav - Converted 1D tensor to 2D:" << numFrames << "x" << numChannels;
        } else if (audio.dim() == 2) {
            // Stereo or multi-channel audio: (frames, channels)
            numFrames = audio.size(0);
            numChannels = audio.size(1);

            // Validate channel count
            if (numChannels < 1 || numChannels > 64) {
                qCritical() << "AudioPreprocessUtils::saveToWav - Invalid channel count:" << numChannels << "(must be 1-64)";
                return false;
            }

            audio2d = audio;
            qDebug() << "AudioPreprocessUtils::saveToWav - Using 2D tensor:" << numFrames << "x" << numChannels;
        } else {
            qCritical() << "AudioPreprocessUtils::saveToWav - Audio tensor must be 1D or 2D, got" << audio.dim() << "D tensor:" << filePath;
            return false;
        }

        // Validate tensor dimensions
        if (numFrames == 0) {
            qCritical() << "AudioPreprocessUtils::saveToWav - Zero frames in audio tensor:" << filePath;
            return false;
        }

        if (numFrames > static_cast<int64_t>(INT_MAX)) {
            qCritical() << "AudioPreprocessUtils::saveToWav - Too many frames (" << numFrames << "), exceeds INT_MAX:" << filePath;
            return false;
        }

    } catch (const std::exception& e) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Exception during tensor validation:" << e.what() << "for file:" << filePath;
        return false;
    }

    // Prepare libsndfile info with validation
    SF_INFO sfinfo;
    std::memset(&sfinfo, 0, sizeof(SF_INFO));

    sfinfo.samplerate = sampleRate;
    sfinfo.channels = static_cast<int>(numChannels);
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    // Validate sfinfo
    if (sfinfo.channels != numChannels) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Channel count mismatch: expected" << numChannels << "got" << sfinfo.channels;
        return false;
    }

    // Create directory if it doesn't exist
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "AudioPreprocessUtils::saveToWav - Failed to create directory:" << dir.absolutePath();
            return false;
        }
        qDebug() << "AudioPreprocessUtils::saveToWav - Created directory:" << dir.absolutePath();
    }

    // Open file for writing
    SNDFILE* file = sf_open(filePath.toStdString().c_str(), SFM_WRITE, &sfinfo);
    if (!file) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Failed to open WAV file for writing:" << filePath << "- Error:" << sf_strerror(nullptr);
        return false;
    }

    qDebug() << "AudioPreprocessUtils::saveToWav - Successfully opened WAV file for writing";

    // Convert float tensor to vector<float> with memory safety
    std::vector<float> audioVec;
    try {
        audioVec.resize(audio2d.numel());

        if (audioVec.empty()) {
            qCritical() << "AudioPreprocessUtils::saveToWav - Failed to allocate memory for audio vector";
            sf_close(file);
            return false;
        }

        // Copy tensor data to vector
        std::memcpy(audioVec.data(), audio2d.data_ptr<float>(), audio2d.numel() * sizeof(float));

        qDebug() << "AudioPreprocessUtils::saveToWav - Prepared audio vector with" << audioVec.size() << "samples";

    } catch (const std::bad_alloc& e) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Memory allocation failed:" << e.what() << "for file:" << filePath;
        sf_close(file);
        return false;
    } catch (const std::exception& e) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Exception during data preparation:" << e.what() << "for file:" << filePath;
        sf_close(file);
        return false;
    }

    // Write audio data
    sf_count_t written = 0;
    try {
        written = sf_write_float(file, audioVec.data(), audioVec.size());

        if (written < 0) {
            qCritical() << "AudioPreprocessUtils::saveToWav - Error writing audio data:" << sf_strerror(file) << "for file:" << filePath;
            sf_close(file);
            return false;
        }

        qDebug() << "AudioPreprocessUtils::saveToWav - Successfully wrote" << written << "samples";

    } catch (const std::exception& e) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Exception during file writing:" << e.what() << "for file:" << filePath;
        sf_close(file);
        return false;
    }

    // Close file
    int closeResult = sf_close(file);
    if (closeResult != 0) {
        qWarning() << "AudioPreprocessUtils::saveToWav - Warning: sf_close returned non-zero value:" << closeResult << "for file:" << filePath;
    }

    // Final validation
    if (written != static_cast<sf_count_t>(audioVec.size())) {
        qCritical() << "AudioPreprocessUtils::saveToWav - Failed to write all samples to WAV file:" << filePath;
        qCritical() << "AudioPreprocessUtils::saveToWav - Expected to write:" << audioVec.size() << "samples, actually wrote:" << written;
        return false;
    }

    qDebug() << "AudioPreprocessUtils::saveToWav - Successfully saved WAV file:" << filePath;
    qDebug() << "AudioPreprocessUtils::saveToWav - Final stats - Frames:" << numFrames << "Channels:" << numChannels << "Sample rate:" << sampleRate;

    return true;
}

torch::Tensor applyHannWindow(const torch::Tensor& chunk) {
    if (chunk.numel() == 0) return chunk;

    int64_t length = chunk.size(0);
    torch::Tensor window = torch::hann_window(length, torch::kFloat32);
    return chunk * window;
}

} // namespace AudioPreprocessUtils
