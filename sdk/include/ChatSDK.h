#pragma once

#include "common.h"
#include "LLMManager.h"
#include "SessionManager.h"

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <map>

namespace ai_chat_sdk {

class ChatSDK {
public:

    // 初始化 SDK，同时指定数据库路径
    explicit ChatSDK(
        const std::string& dbName = "chatDB.db"
    );

    // ========== 模型管理 ==========

    bool initModels(
        const std::vector<std::shared_ptr<Config>>& configs
    );

    std::vector<ModelInfo> getAvailableModels();

    // ========== 会话管理 ==========

    std::string createSession(
        const std::string& modelName
    );

    std::shared_ptr<Session> getSession(
        const std::string& sessionId
    );

    std::vector<std::string> getSessionList();

    bool deleteSession(
        const std::string& sessionId
    );

    // ========== 聊天接口 ==========

    // 普通响应
    std::string sendMessage(
        const std::string& sessionId,
        const std::string& message
    );

    // 流式响应
    void sendMessageStream(
        const std::string& sessionId,
        const std::string& message,
        std::function<void(const std::string&, bool)> callback
    );

private:

    // ========== Provider 管理 ==========

    void registerAllProvider(
        const std::vector<std::shared_ptr<Config>>& configs
    );

    void initProviders(
        const std::vector<std::shared_ptr<Config>>& configs
    );

    bool apiProviderInit(
        const std::string& modelName,
        const std::shared_ptr<ApiConfig>& apiConfig
    );

    bool ollamaProviderInit(
        const std::string& modelName,
        const std::shared_ptr<OllamaConfig>& ollamaConfig
    );

private:

    // SDK 是否完成初始化
    bool _initialized = false;

    // 模型配置
    std::unordered_map<
        std::string,
        std::shared_ptr<Config>
    > _configs;

    // 模型管理
    LLMManager _llmManager;

    // 会话管理
    SessionManager _sessionManager;
};

} // namespace ai_chat_sdk
