
#include "QwenProvider.h"
#include "ILLMProvider.h"

#include "Log.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace ai_chat_sdk;

int main()
{
    // 1. 初始化日志
    ai_chat_sdk::log::Settings settings;
    settings.async = false;
    ai_chat_sdk::log::initialize(settings);

    // 2. 获取 API Key
    const char* key = std::getenv("QWEN_API_KEY");

    if (!key || !*key) {
        CHAT_SDK_ERROR("QWEN_API_KEY is missing");
        return 1;
    }

    // 3. 使用基类指针管理 QwenProvider
    std::unique_ptr<ILLMProvider> provider =
        std::make_unique<QwenProvider>();

    std::map<std::string, std::string> config = {
        {"api_key", key},
        {"base_url", "https://maas.qianwenaiapi.com"}
    };

    if (!provider->initModel(config) ||
        !provider->isAvailable()) {
        CHAT_SDK_ERROR("QwenProvider initialization failed");
        return 1;
    }

    CHAT_SDK_INFO("Model initialized: {}", provider->getModelName());

    // 4. 构造消息
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
        {"max_tokens", "1024"}
    };

    try {
        // ========== 测试一：普通聊天 ==========

        std::cout << "\n========== Normal Chat ==========\n";

        std::string answer =
            provider->sendMessage(messages, params);

        if (answer.empty()) {
            CHAT_SDK_ERROR("Qwen normal chat test failed");
            return 1;
        }

        std::cout << answer << '\n';
        std::cout << "=================================\n";

        CHAT_SDK_INFO("Qwen normal chat test passed");


        // ========== 测试二：SSE 流式聊天 ==========

        std::string streamed_answer;
        int done_count = 0;

        std::cout << "\n========== Streaming Chat ==========\n";

        std::string full_answer = provider->sendMessageStream(
            messages,
            params,
            [&](const std::string& content, bool done)
            {
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

        // 3. 验证流式结果
        if (full_answer.empty() ||
            full_answer != streamed_answer ||
            done_count != 1) {
            CHAT_SDK_ERROR("Qwen streaming chat test failed");
            return 1;
        }

        CHAT_SDK_INFO("Qwen streaming chat test passed");
        CHAT_SDK_INFO("Answer length: {} bytes", full_answer.size());

    } catch (const std::exception& e) {
        CHAT_SDK_ERROR("QwenProvider test exception: {}", e.what());
        return 1;
    }

    CHAT_SDK_INFO("All QwenProvider tests passed!");

    return 0;
}
