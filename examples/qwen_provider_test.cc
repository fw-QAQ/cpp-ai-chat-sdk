
#include "QwenProvider.h"
#include "ILLMProvider.h"

#include <bite_scaffold/log.h>

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
    bitelog::log_settings settings;
    settings.async = false;
    bitelog::bitelog_init(settings);

    // 2. 获取 API Key
    const char* key = std::getenv("QWEN_API_KEY");

    if (!key || !*key) {
        ERR("QWEN_API_KEY is missing");
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
        ERR("QwenProvider initialization failed");
        return 1;
    }

    INF("Model initialized: {}", provider->getModelName());

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
            ERR("Qwen normal chat test failed");
            return 1;
        }

        std::cout << answer << '\n';
        std::cout << "=================================\n";

        INF("Qwen normal chat test passed");


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
            ERR("Qwen streaming chat test failed");
            return 1;
        }

        INF("Qwen streaming chat test passed");
        INF("Answer length: {} bytes", full_answer.size());

    } catch (const std::exception& e) {
        ERR("QwenProvider test exception: {}", e.what());
        return 1;
    }

    INF("All QwenProvider tests passed!");

    return 0;
}
