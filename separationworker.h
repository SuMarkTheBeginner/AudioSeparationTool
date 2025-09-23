// SeparationWorker.h
#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#ifndef Q_MOC_RUN
#undef slots
#include <torch/torch.h>
#define slots
#endif
#include "zero_shot_asp_feature_extractor.h"
#include "constants.h"

class SeparationWorker
{
public:
    explicit SeparationWorker();
    ~SeparationWorker();

    // 載入 query 特徵檔
    torch::Tensor loadFeature(const QString& featurePath);

    // 分段呼叫模型 forward
    torch::Tensor processChunk(const torch::Tensor& waveform,
                               const torch::Tensor& condition,
                               ZeroShotASPFeatureExtractor* extractor);

    // Overlap-Add 合併多個 chunk
    torch::Tensor doOverlapAdd(const std::vector<torch::Tensor>& chunks);
    torch::Tensor doOverlapAdd(const QStringList& chunkFilePaths);

    // Synchronous method to process files
    void processFile(const QStringList& filePaths, const QString& featureName);

private:
    void processSingleFile(const QString& audioPath, const QString& featureName);
    float overlapRate;
    int clipSamples;
};
