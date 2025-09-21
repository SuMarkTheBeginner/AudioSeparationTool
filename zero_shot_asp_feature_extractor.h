// ZeroShotASPFeatureExtractor.h
#pragma once

#ifndef Q_MOC_RUN
#undef slots
#endif
#include <torch/script.h>
#ifndef Q_MOC_RUN
#define slots
#endif
#include <QString>
#include <QObject>

class ZeroShotASPFeatureExtractor : public QObject
{
    Q_OBJECT

public:
    ZeroShotASPFeatureExtractor(QObject* parent = nullptr);
    ~ZeroShotASPFeatureExtractor() = default;

    // 載入 TorchScript 模型
    bool loadModel(const QString& modelPath);

    // forward 計算
    // waveform: (1, clip_samples, 1)
    // condition: (1, 2048)
    // return: separated waveform tensor (1, clip_samples, 1)
    torch::Tensor forward(const torch::Tensor& waveform,
                          const torch::Tensor& condition);

    // 卸載模型以釋放記憶體
    void unloadModel();

    // 從資源載入模型
    bool loadModelFromResource(const QString& resourcePath);

signals:
    void finished(const torch::Tensor& output);
    void error(const QString& errorMessage);

private:
    torch::jit::script::Module model;
    bool modelLoaded;
};
