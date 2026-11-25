#include "ChatSDK.h"

#include <bite_scaffold/log.h>

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <exception>

using namespace ai_chat_sdk;


// 获取环境变量
std::string getEnv(const char* name)
{
    const char* value = std::getenv(name);

    return value ? value : "";
}


// 构建模型配置
std::vector<std::shared_ptr<Config>> makeConfigs()
{
    std::vector<std::shared_ptr<Config>> configs;

    // ========== DeepSeek ==========

    auto deepseek = std::make_shared<ApiConfig>();

    deepseek->model_name = "deepseek-flash";
    deepseek->model_desc = "DeepSeek";
    deepseek->api_key = getEnv("DEEPSEEK_API_KEY");
    deepseek->endpoint = "https://api.deepseek.com";
    deepseek->temperature = 0.7;
    deepseek->max_tokens = 256;

    configs.push_back(deepseek);


    // ========== ChatGPT ==========

    auto gpt = std::make_shared<ApiConfig>();

    gpt->model_name = "gpt-6-sol";
    gpt->model_desc = "ChatGPT";
    gpt->api_key = getEnv("OPENAI_API_KEY");
    gpt->endpoint = "https://api.hopeplay.online";
    gpt->group = getEnv("HOPEPLAY_GROUP");
    gpt->temperature = 0.7;
    gpt->max_tokens = 256;

    configs.push_back(gpt);


    // ========== Qwen ==========

    auto qwen = std::make_shared<ApiConfig>();

    qwen->model_name = "qwen3.8-flash";
    qwen->model_desc = "Qwen";
    qwen->api_key = getEnv("QWEN_API_KEY");
    qwen->endpoint = "https://maas.qianwenaiapi.com";
    qwen->temperature = 0.7;
    qwen->max_tokens = 256;

    configs.push_back(qwen);

    return configs;
}


// ========== 单模型测试 ==========

bool testModel(
    ChatSDK& sdk,
    const std::string& modelName
)
{
    std::cout
        << "\n========================================\n"
        << "Testing model: " << modelName << '\n'
        << "========================================\n";

    // 1. 创建会话
    std::string sessionId = sdk.createSession(modelName);

    if (sessionId.empty())
    {
        ERR("Failed to create session: {}", modelName);
        return false;
    }

    INF("Create session passed");


    // 2. 普通聊天
    std::string question1 =
        "请用一句话介绍什么是epoll。";

    std::cout << "\n========== Normal Chat ==========\n";

    std::cout << "[user] " << question1 << '\n';

    std::string response = sdk.sendMessage(
        sessionId,
        question1
    );

    if (response.empty())
    {
        ERR("Normal response failed: {}", modelName);
        return false;
    }

    std::cout << "[assistant] " << response << '\n';

    INF("Normal response passed");


    // 3. 验证普通聊天历史
    auto session = sdk.getSession(sessionId);

    if (!session ||
        session->messages.size() != 2 ||
        session->messages[0].role != "user" ||
        session->messages[0].content != question1 ||
        session->messages[1].role != "assistant" ||
        session->messages[1].content != response)
    {
        ERR("Normal chat history failed: {}", modelName);
        return false;
    }

    INF("Normal chat history passed");


    // 4. 流式聊天
    std::string question2 =
        "它与select相比有什么优势？请简短回答。";

    std::cout << "\n========== Stream Chat ==========\n";

    std::cout << "[user] " << question2 << '\n';
    std::cout << "[assistant] " << std::flush;

    std::string streamResponse;

    int doneCount = 0;

    bool savedWhenDone = false;

    sdk.sendMessageStream(
        sessionId,
        question2,

        [&](const std::string& chunk, bool done)
        {
            if (!chunk.empty())
            {
                streamResponse += chunk;

                std::cout << chunk << std::flush;
            }

            if (done)
            {
                ++doneCount;

                // 验证完成回调执行前，
                // 助手消息是否已经成功保存
                auto current = sdk.getSession(sessionId);

                savedWhenDone =
                    current != nullptr &&
                    current->messages.size() == 4 &&
                    current->messages.back().role == "assistant" &&
                    current->messages.back().content
                        == streamResponse;

                std::cout << '\n';
            }
        }
    );

    if (streamResponse.empty())
    {
        ERR("Stream response failed: {}", modelName);
        return false;
    }

    INF("Stream response passed");

    if (doneCount != 1)
    {
        ERR("Invalid completion callback count: {}",
            doneCount);

        return false;
    }

    INF("Stream completion callback passed");

    if (!savedWhenDone)
    {
        ERR("Message saved before callback failed");
        return false;
    }

    INF("Message saved before completion callback passed");


    // 5. 验证完整历史消息
    session = sdk.getSession(sessionId);

    if (!session ||
        session->messages.size() != 4 ||
        session->messages[2].role != "user" ||
        session->messages[2].content != question2 ||
        session->messages[3].role != "assistant" ||
        session->messages[3].content != streamResponse)
    {
        ERR("Stream chat history failed: {}", modelName);
        return false;
    }

    INF("Stream chat history passed");


    // 6. 删除测试会话
    if (!sdk.deleteSession(sessionId))
    {
        ERR("Delete session failed: {}", modelName);
        return false;
    }

    if (sdk.getSession(sessionId) != nullptr)
    {
        ERR("Deleted session still exists: {}", modelName);
        return false;
    }

    INF("Delete session passed");

    INF("All tests passed for model: {}", modelName);

    return true;
}


int main()
{
    // 1. 初始化日志
    bitelog::log_settings settings;

    settings.async = false;

    bitelog::bitelog_init(settings);


    // 2. 创建 SDK
    ChatSDK sdk(":memory:");


    // 3. 初始化模型
    auto configs = makeConfigs();

    if (!sdk.initModels(configs))
    {
        ERR("ChatSDK initialization failed");
        return 1;
    }


    // 4. 输出可用模型
    auto models = sdk.getAvailableModels();

    std::cout << "\n========== Available Models ==========\n";

    for (const auto& model : models)
    {
        std::cout
            << "Name: " << model._name << '\n'
            << "Description: " << model._desc << "\n\n";
    }


    // 5. 依次测试三个模型
    std::vector<std::string> modelNames = {
        "deepseek-flash",
        "gpt-6-sol",
        "qwen3.8-flash"
    };

    std::vector<std::string> passedModels;
    std::vector<std::string> failedModels;

    for (const auto& modelName : modelNames)
    {
        try
        {
            if (testModel(sdk, modelName))
            {
                passedModels.push_back(modelName);
            }
            else
            {
                failedModels.push_back(modelName);
            }
        }
        catch (const std::exception& e)
        {
            ERR("Model {} test exception: {}",
                modelName, e.what());

            failedModels.push_back(modelName);
        }
    }


    // 6. 输出测试报告
    std::cout << "\n========== Test Summary ==========\n";

    std::cout << "Passed: " << passedModels.size() << '\n';

    for (const auto& name : passedModels)
    {
        std::cout << "[PASS] " << name << '\n';
    }

    std::cout << "Failed: " << failedModels.size() << '\n';

    for (const auto& name : failedModels)
    {
        std::cout << "[FAIL] " << name << '\n';
    }

    if (failedModels.empty())
    {
        INF("All multi-model tests passed!");
        return 0;
    }

    ERR("Some model tests failed");

    return 1;
}
