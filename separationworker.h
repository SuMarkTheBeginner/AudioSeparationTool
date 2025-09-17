// SeparationWorker.h
#pragma once

#include <QObject>
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

class SeparationWorker : public QObject
{
    Q_OBJECT
public:
    explicit SeparationWorker(QObject* parent = nullptr);
    ~SeparationWorker() = default;

    // 載入 query 特徵檔
    torch::Tensor loadFeature(const QString& featurePath);

    // 分段呼叫模型 forward
    torch::Tensor processChunk(const torch::Tensor& waveform,
                               const torch::Tensor& condition,
                               ZeroShotASPFeatureExtractor* extractor);

    // Overlap-Add 合併多個 chunk
    torch::Tensor doOverlapAdd(const std::vector<torch::Tensor>& chunks);
    torch::Tensor doOverlapAdd(const QStringList& chunkFilePaths);

signals:
    // 單個 chunk 完成
    void chunkReady(const QString& audioPath,
                    const QString& featureName,
                    const torch::Tensor& chunkData);

    // 單檔案所有 chunk OLA 完成
    void separationFinished(const QString& audioPath,
                            const QString& featureName,
                            const torch::Tensor& finalTensor);
    void progressUpdated(int value);
    void error(const QString& errorMessage);


public slots:
    // Qt Slot：處理 ResourceManager 的請求
    void processFile(const QStringList& filePaths, const QString& featureName);

private:
    void processSingleFile(const QString& audioPath, const QString& featureName);
    float overlapRate;
    int clipSamples;
};
