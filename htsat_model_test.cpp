#include <torch/script.h>
#include <torch/torch.h>
#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

torch::jit::script::Module loadTorchModel(const std::string& wpath) {

    return torch::jit::load(wpath);
}

void testHTSATModel(const std::string& modelWPath) {
    try {
        auto model = loadTorchModel(modelWPath);
        model.eval();
        model.to(torch::kCPU);

        std::wcout << L"[HTSAT] Model loaded successfully from: " << modelWPath.c_str() << std::endl;

        torch::Tensor example_input = torch::randn({1, 320000});
        std::vector<torch::jit::IValue> inputs = {example_input};

        auto output_dict = model.forward(inputs).toGenericDict();

        auto clipwise = output_dict.at("clipwise_output").toTensor();
        auto framewise = output_dict.at("framewise_output").toTensor();

        std::cout << "[HTSAT] Clipwise shape: [";
        for (auto d : clipwise.sizes()) std::cout << d << " ";
        std::cout << "]\n";

        std::cout << "[HTSAT] Framewise shape: [";
        for (auto d : framewise.sizes()) std::cout << d << " ";
        std::cout << "]\n";

        if (output_dict.contains("latent_output")) {
            auto latent = output_dict.at("latent_output").toTensor();
            std::cout << "[HTSAT] Latent shape: [";
            for (auto d : latent.sizes()) std::cout << d << " ";
            std::cout << "]\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "[HTSAT] Exception: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::string model_path = "C:/models/htsat_embedding_model.pt"; // ✅ 用 wstring，UTF-16
    testHTSATModel(model_path);
    std::cout << "[HTSAT] Test completed." << std::endl;
    return 0;
}
