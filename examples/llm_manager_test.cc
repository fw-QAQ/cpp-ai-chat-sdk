#include "LLMManager.h"

#include "DeepSeekProvider.h"
#include "ChatGPTProvider.h"
#include "QwenProvider.h"
#include "OllamaProvider.h"

#include <bite_scaffold/log.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>

using namespace ai_chat_sdk;

int main()
{
    // ========== 1. 初始化日志 ==========

    bitelog::log_settings settings;
    settings.async = false;
    bitelog::bitelog_init(settings);


    // ========== 2. 创建模型管理器 ==========

    LLMManager manager;


    // ========== 3. 注册四种模型 ==========

    if (!manager.registerProvider(
            "deepseek-flash",
            std::make_unique<DeepSeekProvider>()) ||

        !manager.registerProvider(
            "gpt-6-sol",
            std::make_unique<ChatGPTProvider>()) ||

        !manager.registerProvider(
            "qwen3.8-flash",
            std::make_unique<QwenProvider>()) ||

        !manager.registerProvider(
            "qwen2.5:7b",
            std::make_unique<OllamaProvider>())) {

        ERR("Provider registration failed");
        return 1;
    }

    INF("All providers registered successfully");


    // ========== 4. 测试初始状态 ==========

    if (!manager.getAvailableModel().empty()) {
        ERR("Uninitialized models should not be available");
        return 1;
    }

    if (manager.isModelAvailable("unknown-model")) {
        ERR("Unknown model availability test failed");
        return 1;
    }

    INF("Initial model state test passed");


    // ========== 5. 初始化 DeepSeek ==========

    const char* deepseek_key =
        std::getenv("DEEPSEEK_API_KEY");

    if (deepseek_key && *deepseek_key) {
        if (!manager.initModel(
                "deepseek-flash",
                {
                    {"api_key", deepseek_key},
                    {"base_url", "https://api.deepseek.com"}
                })) {
            return 1;
        }
    }


    // ========== 6. 初始化 ChatGPT ==========

    const char* openai_key =
        std::getenv("OPENAI_API_KEY");

    if (openai_key && *openai_key) {
        if (!manager.initModel(
                "gpt-6-sol",
                {
                    {"api_key", openai_key},
                    {"endpoint", "https://api.hopeplay.online"}
                })) {
            return 1;
        }
    }


    // ========== 7. 初始化 Qwen ==========

    const char* qwen_key =
        std::getenv("QWEN_API_KEY");

    if (qwen_key && *qwen_key) {
        if (!manager.initModel(
                "qwen3.8-flash",
                {
                    {"api_key", qwen_key},
                    {"base_url",
                     "https://maas.qianwenaiapi.com"}
                })) {
            return 1;
        }
    }


    // ========== 8. 初始化 Ollama ==========

    const char* ollama_endpoint =
        std::getenv("OLLAMA_ENDPOINT");

    if (!ollama_endpoint || !*ollama_endpoint) {
        ollama_endpoint =
            "http://192.168.88.1:11434";
    }

    if (!manager.initModel(
            "qwen2.5:7b",
            {
                {"endpoint", ollama_endpoint},
                {"model_name", "qwen2.5:7b"},
                {"model_desc", "Local Qwen2.5 7B"}
            })) {
        return 1;
    }


    // ========== 9. 获取可用模型列表 ==========

    auto models = manager.getAvailableModel();

    std::cout << "\n========== Available Models ==========\n";

    for (const auto& model : models) {
        std::cout << "Model: " << model._name << '\n';
        std::cout << "Description: " << model._desc << '\n';
        std::cout << "Available: "
                  << std::boolalpha
                  << model._isAvailable << '\n';

        std::cout << "--------------------------------------\n";
    }

    INF("Available model count: {}", models.size());


    // ========== 10. 构造聊天消息 ==========

    std::vector<Message> messages;

    messages.emplace_back(
        "system",
        "You are a helpful assistant. Answer briefly."
    );

    messages.emplace_back(
        "user",
        "用一句话解释 C++ 中的 RAII。"
    );

    std::map<std::string, std::string> params = {
        {"max_tokens", "512"},
        {"max_output_tokens", "512"},
        {"num_ctx", "4096"}
    };


    // ========== 11. 测试不存在的模型 ==========

    if (!manager.sendMessage(
            "unknown-model",
            messages,
            params).empty()) {

        ERR("Unknown model routing test failed");
        return 1;
    }

    INF("Unknown model routing test passed");


    // ========== 12. 测试模型路由 ==========

    for (const auto& model : models) {

        if (!manager.isModelAvailable(model._name)) {
            ERR("Model availability state inconsistent");
            return 1;
        }

        std::cout << "\n========== "
                  << model._name
                  << " ==========\n";

        std::string answer = manager.sendMessage(
            model._name,
            messages,
            params
        );

        if (answer.empty()) {
            ERR("Model {} routing test failed",
                model._name);

            return 1;
        }

        std::cout << answer << '\n';

        INF("Model {} routing test passed",
            model._name);
    }


    // ========== 13. 测试流式模型路由 ==========

    std::string streamed_answer;
    int done_count = 0;

    std::cout << "\n========== Ollama Streaming ==========\n";

    std::string full_answer =
        manager.sendMessageStream(
            "qwen2.5:7b",
            messages,
            params,
            [&](const std::string& content, bool done) {

                if (done) {
                    ++done_count;
                    std::cout << "\n[Stream finished]\n";
                    return;
                }

                streamed_answer += content;
                std::cout << content << std::flush;
            }
        );

    if (full_answer.empty() ||
        full_answer != streamed_answer ||
        done_count != 1) {

        ERR("LLMManager streaming routing test failed");
        return 1;
    }

    INF("LLMManager streaming routing test passed");

    INF("All LLMManager tests passed!");

    return 0;
}
