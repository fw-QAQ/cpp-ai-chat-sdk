#include "ChatSDK.h"

#include "Log.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

using namespace ai_chat_sdk;


// ========== 测试辅助函数 ==========

void check(
    bool condition,
    const std::string& name
) {
    if (!condition) {
        throw std::runtime_error(name + " failed");
    }

    CHAT_SDK_INFO("{} passed", name);
}


// ========== 构建模型配置 ==========

std::vector<std::shared_ptr<Config>>
makeConfigs()
{
    auto ollamaConfig =
        std::make_shared<OllamaConfig>();

    ollamaConfig->model_name = "qwen2.5:7b";

    ollamaConfig->model_desc =
        "本地 Qwen2.5 7B 模型";

    ollamaConfig->temperature = 0.7;

    ollamaConfig->max_tokens = 256;

    // 优先读取环境变量
    const char* endpoint =
        std::getenv("OLLAMA_ENDPOINT");

    ollamaConfig->endpoint =
        endpoint ? endpoint : "http://192.168.88.1:11434";

    return {ollamaConfig};
}


int main()
{
    // ========== 1. 初始化日志 ==========

    ai_chat_sdk::log::Settings settings;

    settings.async = false;

    ai_chat_sdk::log::initialize(settings);


    // ========== 2. 创建临时数据库 ==========

    char path[] = "/tmp/chat_sdk_test_XXXXXX";

    int fd = mkstemp(path);

    if (fd == -1) {
        CHAT_SDK_ERROR("Failed to create test database");
        return 1;
    }

    close(fd);

    std::string dbName = path;

    std::string sessionId;

    std::vector<Message> savedHistory;

    try {

        // ====================================
        // 第一阶段：SDK 初始化与完整聊天测试
        // ====================================

        {
            ChatSDK sdk(dbName);

            // ---------- 未初始化状态 ----------

            check(
                sdk.createSession("qwen2.5:7b").empty(),
                "Reject uninitialized SDK"
            );


            // ---------- 初始化 SDK ----------

            auto configs = makeConfigs();

            check(
                sdk.initModels(configs),
                "Initialize ChatSDK"
            );


            // ---------- 获取可用模型 ----------

            auto models = sdk.getAvailableModels();

            check(
                models.size() == 1 &&
                models[0]._name == "qwen2.5:7b",
                "Get available models"
            );

            std::cout
                << "\n========== Available Models ==========\n";

            for (const auto& model : models) {

                std::cout
                    << "Name: " << model._name << '\n';

                std::cout
                    << "Description: " << model._desc << '\n';
            }


            // ---------- 创建会话 ----------

            sessionId =
                sdk.createSession("qwen2.5:7b");

            check(
                !sessionId.empty(),
                "Create session"
            );

            check(
                sdk.createSession("unknown-model").empty(),
                "Reject unavailable model"
            );


            // ---------- 普通聊天第一轮 ----------

            std::cout
                << "\n========== Normal Chat ==========\n";

            std::string question1 =
                "请记住我的名字叫张三，只回答一句话。";

            std::cout
                << "[user] " << question1 << '\n';

            std::string response1 =
                sdk.sendMessage(
                    sessionId,
                    question1
                );

            check(
                !response1.empty(),
                "First normal response"
            );

            std::cout
                << "[assistant] " << response1 << '\n';


            // ---------- 普通聊天第二轮 ----------

            std::string question2 =
                "我叫什么名字？请简短回答。";

            std::cout
                << "[user] " << question2 << '\n';

            std::string response2 =
                sdk.sendMessage(
                    sessionId,
                    question2
                );

            check(
                !response2.empty(),
                "Second normal response"
            );

            std::cout
                << "[assistant] " << response2 << '\n';


            // ---------- 检查历史消息 ----------

            auto session = sdk.getSession(sessionId);

            check(
                session != nullptr &&
                session->messages.size() == 4,
                "Normal chat history"
            );

            check(
                session->messages[0].role == "user" &&
                session->messages[0].content == question1 &&

                session->messages[1].role == "assistant" &&
                session->messages[1].content == response1 &&

                session->messages[2].role == "user" &&
                session->messages[2].content == question2 &&

                session->messages[3].role == "assistant" &&
                session->messages[3].content == response2,

                "Normal chat message content"
            );


            // ---------- 流式聊天 ----------

            std::cout
                << "\n========== Stream Chat ==========\n";

            std::string question3 =
                "请用三句话介绍 C++ 中的 RAII。";

            std::cout
                << "[user] " << question3 << '\n';

            std::cout
                << "[assistant] " << std::flush;

            std::string streamResponse;

            int doneCount = 0;

            bool savedWhenDone = false;

            sdk.sendMessageStream(
                sessionId,
                question3,

                [&](const std::string& chunk, bool done) {

                    if (!chunk.empty()) {

                        streamResponse += chunk;

                        std::cout
                            << chunk
                            << std::flush;
                    }

                    if (done) {
    ++doneCount;

    auto current = sdk.getSession(sessionId);

    CHAT_SDK_INFO("[DEBUG] Completion callback triggered");

    CHAT_SDK_INFO("[DEBUG] Session exists: {}",
        current != nullptr);

    CHAT_SDK_INFO("[DEBUG] Stream response bytes: {}",
        streamResponse.size());

    if (current) {
        CHAT_SDK_INFO("[DEBUG] Message count: {}",
            current->messages.size());

        if (!current->messages.empty()) {

            const auto& lastMessage =
                current->messages.back();

            CHAT_SDK_INFO("[DEBUG] Last message role: {}",
                lastMessage.role);

            CHAT_SDK_INFO("[DEBUG] Last message bytes: {}",
                lastMessage.content.size());

            CHAT_SDK_INFO("[DEBUG] Content matches: {}",
                lastMessage.content == streamResponse);

            CHAT_SDK_INFO("[DEBUG] Saved content: [{}]",
                lastMessage.content);

            CHAT_SDK_INFO("[DEBUG] Stream content: [{}]",
                streamResponse);
        }
    }

    savedWhenDone =
        current != nullptr &&
        current->messages.size() == 6 &&
        current->messages.back().content == streamResponse;

    std::cout << '\n';
}
                }
            );

            check(
                !streamResponse.empty(),
                "Stream response"
            );

            check(
                doneCount == 1,
                "Stream completion callback"
            );

            check(
                savedWhenDone,
                "Message saved before completion callback"
            );


            // ---------- 检查流式聊天历史 ----------

            session = sdk.getSession(sessionId);

            check(
                session != nullptr &&
                session->messages.size() == 6,
                "Stream chat history"
            );

            check(
                session->messages[4].role == "user" &&
                session->messages[4].content == question3 &&

                session->messages[5].role == "assistant" &&
                session->messages[5].content == streamResponse,

                "Stream message consistency"
            );

            savedHistory = session->messages;

            check(
                sdk.getSessionList().size() == 1,
                "Get session list"
            );

        } // ChatSDK 析构，数据库连接关闭


        // ====================================
        // 第二阶段：重新构造 ChatSDK
        // ====================================

        {
            ChatSDK sdk(dbName);

            auto session =
                sdk.getSession(sessionId);

            check(
                session != nullptr &&
                session->messages.size() == 6,
                "Restore persistent chat history"
            );

            for (size_t i = 0; i < savedHistory.size(); ++i) {

                const auto& restored =
                    session->messages[i];

                const auto& original =
                    savedHistory[i];

                check(
                    restored.id == original.id &&
                    restored.role == original.role &&
                    restored.content == original.content &&
                    restored.timestamp == original.timestamp,

                    "Restore message " + std::to_string(i)
                );
            }

            std::cout
                << "\n========== Restored History ==========\n";

            for (const auto& message : session->messages) {

                std::cout
                    << "[" << message.role << "] "
                    << message.content << '\n';
            }

            check(
                sdk.deleteSession(sessionId),
                "Delete persistent session"
            );
        }


        // ====================================
        // 第三阶段：验证删除已持久化
        // ====================================

        {
            ChatSDK sdk(dbName);

            check(
                sdk.getSessionList().empty(),
                "Verify persistent deletion"
            );
        }

        std::remove(dbName.c_str());

        CHAT_SDK_INFO("All ChatSDK tests passed!");

    } catch (const std::exception& e) {

        CHAT_SDK_ERROR("ChatSDK test failed: {}", e.what());

        CHAT_SDK_ERROR("Test database: {}", dbName);

        return 1;
    }

    return 0;
}
