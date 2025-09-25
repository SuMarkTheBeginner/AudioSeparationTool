#include "audioserializer.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include "constants.h"
#include "logger.h"

AudioSerializer::AudioSerializer(QObject* parent)
    : QObject(parent)
{
    LOG_INFO("AudioSerializer: Initialized audio serialization handler");
}

AudioSerializer::~AudioSerializer()
{
    LOG_DEBUG("AudioSerializer: Destroying serializer");
}

bool AudioSerializer::saveWavToFile(const torch::Tensor& waveform, const QString& filePath, int sampleRate)
{
    LOG_DEBUG(QString("AudioSerializer: Saving WAV to: %1").arg(filePath));

    // Ensure output directory exists
    QFileInfo fi(filePath);
    QDir dir(fi.absolutePath());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(QString("AudioSerializer: Failed to create output directory: %1").arg(dir.absolutePath()));
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("AudioSerializer: Failed to open file for writing: %1").arg(filePath));
        return false;
    }

    // Normalize tensor shape for consistent WAV format
    torch::Tensor normalizedTensor = normalizeTensorShape(waveform);
    if (normalizedTensor.numel() == 0) {
        LOG_ERROR("AudioSerializer: Failed to normalize tensor shape");
        return false;
    }

    // Extract audio properties
    short channels = static_cast<short>(normalizedTensor.size(1));
    int64_t numSamples = normalizedTensor.numel();
    const float* data = normalizedTensor.data_ptr<float>();

    LOG_DEBUG(QString("AudioSerializer: Writing WAV - channels: %1, samples: %2").arg(channels).arg(numSamples));

    // Write WAV header
    if (!writeWavHeader(file, channels, sampleRate, numSamples)) {
        LOG_ERROR("AudioSerializer: Failed to write WAV header");
        return false;
    }

    // Write audio data
    if (!writeWavData(file, data, numSamples)) {
        LOG_ERROR("AudioSerializer: Failed to write WAV data");
        return false;
    }

    file.close();
    LOG_INFO(QString("AudioSerializer: Successfully saved WAV file: %1").arg(filePath));
    return true;
}

torch::Tensor AudioSerializer::loadWavFromFile(const QString& filePath)
{
    LOG_DEBUG(QString("AudioSerializer: Loading WAV from: %1").arg(filePath));

    // TODO: Implement WAV loading logic
    // This would require reading the WAV header and extracting audio data
    // For now, return empty tensor to indicate not implemented
    LOG_WARNING("AudioSerializer: WAV loading not yet implemented");
    return torch::Tensor();
}

bool AudioSerializer::saveEmbeddingToFile(const std::vector<float>& embedding, const QString& filePath)
{
    LOG_DEBUG(QString("AudioSerializer: Saving embedding to: %1").arg(filePath));

    // Ensure output directory exists
    QFileInfo fi(filePath);
    QDir dir(fi.absolutePath());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(QString("AudioSerializer: Failed to create output directory: %1").arg(dir.absolutePath()));
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("AudioSerializer: Failed to open embedding file for writing: %1").arg(filePath));
        return false;
    }

    QTextStream out(&file);
    for (size_t i = 0; i < embedding.size(); ++i) {
        out << embedding[i];
        if (i < embedding.size() - 1) {
            out << " ";
        }
    }
    out << "\n";
    file.close();

    LOG_INFO(QString("AudioSerializer: Successfully saved embedding file: %1").arg(filePath));
    return true;
}

std::vector<float> AudioSerializer::loadEmbeddingFromFile(const QString& filePath)
{
    LOG_DEBUG(QString("AudioSerializer: Loading embedding from: %1").arg(filePath));

    std::vector<float> embedding;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("AudioSerializer: Failed to open embedding file for reading: %1").arg(filePath));
        return embedding;
    }

    QTextStream in(&file);
    QString line = in.readLine();
    QStringList values = line.split(" ", Qt::SkipEmptyParts);

    for (const QString& value : values) {
        bool ok;
        float val = value.toFloat(&ok);
        if (ok) {
            embedding.push_back(val);
        } else {
            LOG_WARNING(QString("AudioSerializer: Ignoring invalid float value in embedding file: %1").arg(value));
        }
    }

    file.close();
    LOG_INFO(QString("AudioSerializer: Successfully loaded embedding with %1 values").arg(embedding.size()));
    return embedding;
}

torch::Tensor AudioSerializer::normalizeTensorShape(const torch::Tensor& waveform)
{
    // Handle different tensor shapes for WAV compatibility
    if (waveform.dim() == 1) {
        // Single channel, mono audio - convert to (frames, 1)
        LOG_DEBUG("AudioSerializer: Converting 1D tensor to 2D mono format");
        return waveform.unsqueeze(1);
    } else if (waveform.dim() == 2) {
        // Already in (frames, channels) format
        LOG_DEBUG("AudioSerializer: Using 2D tensor as-is");
        return waveform;
    } else if (waveform.dim() == 3 && waveform.size(0) == 1 && waveform.size(2) == 1) {
        // 3D tensor with shape (1, frames, 1) - squeeze to (frames, 1)
        LOG_DEBUG("AudioSerializer: Converting 3D tensor (1, frames, 1) to 2D");
        return waveform.squeeze(0);
    } else {
        LOG_ERROR(QString("AudioSerializer: Unsupported tensor shape - dim: %1, shape: %2x%3x%4")
                 .arg(waveform.dim())
                 .arg(waveform.size(0))
                 .arg(waveform.dim() > 1 ? waveform.size(1) : 0)
                 .arg(waveform.dim() > 2 ? waveform.size(2) : 0));
        return torch::Tensor(); // Empty tensor indicates error
    }
}

bool AudioSerializer::writeWavHeader(QFile& file, short channels, int sampleRate, int64_t numSamples)
{
    // Write WAV header (RIFF format)
    int64_t dataSize = numSamples * sizeof(float);
    int64_t totalSize = 36 + dataSize;

    // RIFF header
    file.write("RIFF", 4);
    int fileSizePos = file.pos();
    file.write((char*)&totalSize, 4);
    file.write("WAVE", 4);

    // Format chunk
    file.write("fmt ", 4);
    int fmtSize = 16;
    file.write((char*)&fmtSize, 4);
    short format = 3;  // IEEE float
    file.write((char*)&format, 2);
    file.write((char*)&channels, 2);
    file.write((char*)&sampleRate, 4);
    int byteRate = sampleRate * channels * 4;
    file.write((char*)&byteRate, 4);
    short blockAlign = channels * 4;
    file.write((char*)&blockAlign, 2);
    short bitsPerSample = 32;
    file.write((char*)&bitsPerSample, 2);

    // Data chunk header
    file.write("data", 4);
    file.write((char*)&dataSize, 4);

    return true;
}

bool AudioSerializer::writeWavData(QFile& file, const float* data, int64_t numSamples)
{
    // Write audio data (little endian float32)
    int64_t bytesWritten = file.write((const char*)data, numSamples * sizeof(float));
    if (bytesWritten != numSamples * sizeof(float)) {
        LOG_ERROR(QString("AudioSerializer: Failed to write all audio data. Expected: %1 bytes, wrote: %2")
                 .arg(numSamples * sizeof(float)).arg(bytesWritten));
        return false;
    }

    return true;
}
