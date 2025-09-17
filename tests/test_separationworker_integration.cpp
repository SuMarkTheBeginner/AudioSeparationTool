#include <QtTest/QtTest>
#include "../separationworker.h"
#include "../zero_shot_asp_feature_extractor.h"

class TestSeparationWorkerIntegration : public QObject
{
    Q_OBJECT

private slots:
    void testLoadModel();
    void testProcessAudioInChunks();
};

void TestSeparationWorkerIntegration::testLoadModel()
{
    SeparationWorker worker;
    bool loaded = worker.loadModel();
    QVERIFY(loaded);
}

void TestSeparationWorkerIntegration::testProcessAudioInChunks()
{
    SeparationWorker worker;

    // Prepare dummy audio data (320000 samples of zeros)
    std::vector<float> audioData(320000, 0.0f);

    // Prepare dummy embedding (2048 floats of 0.1f)
    std::vector<float> embedding(2048, 0.1f);

    std::vector<torch::Tensor> chunks = worker.processAudioInChunks(audioData, embedding);
    QVERIFY(!chunks.empty());

    // Check that each chunk tensor has expected shape
    for (const auto& chunk : chunks) {
        QVERIFY(chunk.dim() == 1);
        QVERIFY(chunk.size(0) <= 320000);
    }
}

QTEST_MAIN(TestSeparationWorkerIntegration)

#include "test_separationworker_integration.moc"
