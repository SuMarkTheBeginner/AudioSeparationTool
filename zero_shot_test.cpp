#include <torch/script.h>
#include <torch/torch.h>
#include <iostream>
#include <stdexcept>
#include <string>

void testZeroShotModel(const std::string& modelPath) {
    try {
        torch::jit::script::Module model = torch::jit::load(modelPath);
        model.eval();
        model.to(torch::kCPU);
        std::cout << "[Zero-Shot] Model loaded successfully from: " << modelPath << std::endl;

        if (model.hasattr("freq_bins")) {
            auto freq_bins = model.attr("freq_bins").toTensor();
            std::cout << "[Zero-Shot] freq_bins: " << freq_bins.item<int64_t>() << std::endl;
        } else {
            throw std::runtime_error("Missing freq_bins attribute");
        }

        torch::Tensor waveform = torch::randn({1, 320000, 1});
        torch::Tensor cond = torch::randn({1, 2048});
        std::vector<torch::jit::IValue> inputs = {waveform, cond};

        auto output = model.forward(inputs).toTensor();
        std::cout << "[Zero-Shot] Output shape: [";
        for (auto d : output.sizes()) std::cout << d << " ";
        std::cout << "]" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Zero-Shot] Exception: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::string model_path = "path/to/zero_shot_model.pt"; // TODO: 改成你的模型路徑
    testZeroShotModel(model_path);
    std::cout << "[Zero-Shot] Test completed." << std::endl;
    return 0;
}
