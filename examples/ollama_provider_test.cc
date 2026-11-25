#include "OllamaProvider.h"

#include "Log.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace ai_chat_sdk;

int main()
{
    ai_chat_sdk::log::Settings settings;
    settings.async = false;
    ai_chat_sdk::log::initialize(settings);

    // 通过环境变量指定 Windows Ollama 地址
    const char* endpoint =
        std::getenv("OLLAMA_ENDPOINT");

    if (!endpoint || !*endpoint) {
        CHAT_SDK_ERROR("OLLAMA_ENDPOINT is missing");
        return 1;
    }

    // 创建 Provider
    std::unique_ptr<ILLMProvider> provider =
        std::make_unique<OllamaProvider>();

    std::map<std::string, std::string> config = {
        {"endpoint", endpoint},
        {"model_name", "qwen2.5:7b"},
        {"model_desc", "Local Qwen2.5 7B model"}
    };

    if (!provider->initModel(config) ||
        !provider->isAvailable()) {

        CHAT_SDK_ERROR("Ollama initialization failed");
        return 1;
    }

    CHAT_SDK_INFO("Model initialized: {}", provider->getModelName());

    // 构造聊天消息
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
        {"temperature", "0.7"},
        {"max_tokens", "1024"},
        {"num_ctx", "4096"}
    };

    // ========== 普通聊天测试 ==========

    std::cout << "\n========== Normal Chat ==========\n";

    std::string answer =
        provider->sendMessage(messages, params);

    if (answer.empty()) {
        CHAT_SDK_ERROR("Ollama normal chat test failed");
        return 1;
    }

    std::cout << answer << '\n';
    std::cout << "=================================\n";

    CHAT_SDK_INFO("Ollama normal chat test passed");

    // ========== 流式聊天测试 ==========

    std::string streamed_answer;
    int done_count = 0;

    std::cout << "\n========== Streaming Chat ==========\n";

    std::string full_answer =
        provider->sendMessageStream(
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

    std::cout << "====================================\n";

    if (full_answer.empty() ||
        full_answer != streamed_answer ||
        done_count != 1) {

        CHAT_SDK_ERROR("Ollama streaming chat test failed");
        return 1;
    }

    CHAT_SDK_INFO("Ollama streaming chat test passed");
    CHAT_SDK_INFO("Answer length: {} bytes", full_answer.size());

    CHAT_SDK_INFO("All OllamaProvider tests passed!");

    return 0;
}
