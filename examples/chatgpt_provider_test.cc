
#include "../sdk/include/ChatGPTProvider.h"

#include "Log.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace ai_chat_sdk;

int main()
{
    // 初始化日志
    ai_chat_sdk::log::Settings settings;
    settings.async = false;
    ai_chat_sdk::log::initialize(settings);

    // 从环境变量读取 API Key
    const char* key = std::getenv("OPENAI_API_KEY");

    if (!key || !*key) {
        CHAT_SDK_ERROR("OPENAI_API_KEY is missing");
        return 1;
    }

    // 基类指针调用 GPTProvider
    std::unique_ptr<ILLMProvider> provider =
        std::make_unique<ChatGPTProvider>();

    std::map<std::string, std::string> config = {
        {"api_key", key},
        {"endpoint", "https://api.hopeplay.online"}
    };

    if (!provider->initModel(config) ||
        !provider->isAvailable()) {
        CHAT_SDK_ERROR("ChatGPTProvider initialization failed");
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

    // Responses API 参数
    std::map<std::string, std::string> params = {
        {"max_output_tokens", "512"}
    };

    std::string answer =
        provider->sendMessage(messages, params);

    if (answer.empty()) {
        CHAT_SDK_ERROR("ChatGPTProvider test failed");
        return 1;
    }

    std::cout << "\n========== GPT Answer ==========\n";
    std::cout << answer << '\n';
    std::cout << "================================\n";


    CHAT_SDK_INFO("ChatGPTProvider integration test passed");
    // ========== GPT 流式聊天测试 ==========

std::string streamed_answer;
int done_count = 0;

std::cout << "\n========== GPT Streaming ==========\n";

std::string full_answer = provider->sendMessageStream(
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

std::cout << "===================================\n";

if (full_answer.empty() ||
    full_answer != streamed_answer ||
    done_count != 1) {
    CHAT_SDK_ERROR("ChatGPTProvider streaming test failed");
    return 1;
}

CHAT_SDK_INFO("ChatGPTProvider streaming test passed");
CHAT_SDK_INFO("Answer length: {} bytes", full_answer.size());

CHAT_SDK_INFO("All ChatGPTProvider tests passed!");

    return 0;
}
