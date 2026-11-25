#include "ChatSDK.h"

#include "DeepSeekProvider.h"
#include "ChatGPTProvider.h"
#include "QwenProvider.h"
#include "OllamaProvider.h"

#include <bite_scaffold/log.h>

#include <memory>
#include <utility>
#include <unordered_set>

namespace ai_chat_sdk {

ChatSDK::ChatSDK(const std::string& dbName)
    : _sessionManager(dbName)
{
    INF("ChatSDK created");
}
bool ChatSDK::initModels(
    const std::vector<std::shared_ptr<Config>>& configs
) {
    if (_initialized) {
        ERR("ChatSDK has already been initialized");
        return false;
    }

    if (configs.empty()) {
        ERR("Model configs cannot be empty");
        return false;
    }

    // 检查配置合法性及重复模型名称
    std::unordered_set<std::string> modelNames;

    for (const auto& config : configs) {

        if (!config || config->model_name.empty()) {
            ERR("Invalid model config");
            return false;
        }

        if (!modelNames.insert(config->model_name).second) {
            ERR("Duplicate model config: {}",
                config->model_name);

            return false;
        }
    }

    // 第一阶段：注册模型 Provider
    registerAllProvider(configs);

    // 第二阶段：初始化模型 Provider
    initProviders(configs);

    // 至少有一个模型成功初始化
    _initialized = !_configs.empty();

    if (!_initialized) {
        ERR("No model initialized successfully");
        return false;
    }

    INF("ChatSDK initialized, available models: {}",
        _configs.size());

    return true;
}

void ChatSDK::registerAllProvider(
    const std::vector<std::shared_ptr<Config>>& configs
) {
    for (const auto& config : configs) {

        const std::string& modelName =
            config->model_name;

        std::unique_ptr<ILLMProvider> provider;

        // API 模型
        if (std::dynamic_pointer_cast<ApiConfig>(config)) {

            if (modelName == "deepseek-flash") {

                provider =
                    std::make_unique<DeepSeekProvider>();

            } else if (modelName == "gpt-6-sol") {

                provider =
                    std::make_unique<ChatGPTProvider>();

            } else if (modelName == "qwen3.8-flash") {

                provider =
                    std::make_unique<QwenProvider>();

            } else {

                ERR("Unsupported API model: {}", modelName);

                continue;
            }

        // Ollama 模型
        } else if (
            std::dynamic_pointer_cast<OllamaConfig>(config)
        ) {

            provider =
                std::make_unique<OllamaProvider>();

        } else {

            ERR("Unsupported config type: {}", modelName);

            continue;
        }

        // 将 Provider 的所有权交给 LLMManager
        if (!_llmManager.registerProvider(
                modelName,
                std::move(provider))) {

            ERR("Failed to register provider: {}", modelName);

            continue;
        }

        INF("Registered provider: {}", modelName);
    }
}

void ChatSDK::initProviders(
    const std::vector<std::shared_ptr<Config>>& configs
) {
    for (const auto& config : configs) {

        bool success = false;

        // 判断是否为 API 模型配置
        if (auto apiConfig =
                std::dynamic_pointer_cast<ApiConfig>(config)) {

            success = apiProviderInit(
                config->model_name,
                apiConfig
            );

        // 判断是否为 Ollama 模型配置
        } else if (auto ollamaConfig =
                       std::dynamic_pointer_cast<OllamaConfig>(
                           config)) {

            success = ollamaProviderInit(
                config->model_name,
                ollamaConfig
            );

        } else {

            ERR("Unknown config type: {}",
                config->model_name);

            continue;
        }

        if (!success) {
            ERR("Failed to initialize model: {}",
                config->model_name);
        }
    }
}

bool ChatSDK::apiProviderInit(
    const std::string& modelName,
    const std::shared_ptr<ApiConfig>& apiConfig
) {
    // 1. 参数检查
    if (modelName.empty() || !apiConfig) {

        ERR("Invalid API model config");

        return false;
    }

    if (apiConfig->api_key.empty()) {

        ERR("API key is required for model: {}", modelName);

        return false;
    }

    // 2. 构建初始化参数
    std::map<std::string, std::string> modelParams;

    modelParams["api_key"] = apiConfig->api_key;

    modelParams["model_name"] = modelName;

    if (!apiConfig->model_desc.empty()) {
        modelParams["model_desc"] = apiConfig->model_desc;
    }

    if (!apiConfig->endpoint.empty()) {

        if (modelName == "deepseek-flash") {

            modelParams["base_url"] =
                apiConfig->endpoint;

        } else {

            modelParams["endpoint"] =
                apiConfig->endpoint;
        }
    }

    if (!apiConfig->group.empty()) {
        modelParams["group"] = apiConfig->group;
    }

    // 3. 调用 LLMManager 初始化模型
    if (!_llmManager.initModel(
            modelName,
            modelParams)) {

        ERR("Failed to initialize API model: {}", modelName);

        return false;
    }

    // 4. 保存成功初始化的配置
    _configs[modelName] =
        std::make_shared<ApiConfig>(*apiConfig);

    INF("API model initialized: {}", modelName);

    return true;
}

bool ChatSDK::ollamaProviderInit(
    const std::string& modelName,
    const std::shared_ptr<OllamaConfig>& ollamaConfig
) {
    // 1. 检查参数
    if (modelName.empty() || !ollamaConfig) {

        ERR("Invalid Ollama config");

        return false;
    }

    if (ollamaConfig->endpoint.empty()) {

        ERR("Ollama endpoint is required");

        return false;
    }

    // 2. 构建模型参数
    std::map<std::string, std::string> modelParams;

    modelParams["model_name"] = modelName;

    modelParams["endpoint"] = ollamaConfig->endpoint;

    if (!ollamaConfig->model_desc.empty()) {
        modelParams["model_desc"] =
            ollamaConfig->model_desc;
    }

    // 3. 初始化模型
    if (!_llmManager.initModel(
            modelName,
            modelParams)) {

        ERR("Failed to initialize Ollama model: {}", modelName);

        return false;
    }

    // 4. 保存模型配置
    _configs[modelName] =
        std::make_shared<OllamaConfig>(*ollamaConfig);

    INF("Ollama model initialized: {}", modelName);

    return true;
}

std::string ChatSDK::createSession(
    const std::string& modelName
) {
    if (!_initialized) {
        ERR("ChatSDK is not initialized");
        return "";
    }

    // 检查模型是否完成初始化
    if (!_llmManager.isModelAvailable(modelName) ||
        _configs.find(modelName) == _configs.end()) {

        ERR("Model is not available: {}", modelName);

        return "";
    }

    // 调用 SessionManager 创建会话
    std::string sessionId =
        _sessionManager.createSession(modelName);

    if (sessionId.empty()) {

        ERR("Failed to create session");

        return "";
    }

    INF("Created session: {}, model: {}",
        sessionId, modelName);

    return sessionId;
}

std::shared_ptr<Session> ChatSDK::getSession(
    const std::string& sessionId
) {
    return _sessionManager.getSession(sessionId);
}

std::vector<std::string> ChatSDK::getSessionList()
{
    return _sessionManager.getSessionList();
}

bool ChatSDK::deleteSession(
    const std::string& sessionId
) {
    bool result =
        _sessionManager.deleteSession(sessionId);

    if (result) {
        INF("Deleted session: {}", sessionId);
    }

    return result;
}

std::vector<ModelInfo> ChatSDK::getAvailableModels()
{
    return _llmManager.getAvailableModel();
}

std::string ChatSDK::sendMessage(
    const std::string& sessionId,
    const std::string& message
) {
    // ========== 1. 检查 SDK 状态 ==========

    if (!_initialized) {
        ERR("ChatSDK is not initialized");
        return "";
    }

    if (message.empty()) {
        ERR("Message cannot be empty");
        return "";
    }


    // ========== 2. 获取会话 ==========

    auto session =
        _sessionManager.getSession(sessionId);

    if (!session) {
        ERR("Session not found: {}", sessionId);
        return "";
    }

    const std::string modelName =
        session->model_name;


    // ========== 3. 获取模型配置 ==========

    auto it = _configs.find(modelName);

    if (it == _configs.end() ||
        !_llmManager.isModelAvailable(modelName)) {

        ERR("Model is not available: {}", modelName);

        return "";
    }


    // ========== 4. 构建请求参数 ==========

    std::map<std::string, std::string> requestParams;

    requestParams["temperature"] =
        std::to_string(it->second->temperature);

    requestParams["max_tokens"] =
        std::to_string(it->second->max_tokens);


    // ========== 5. 保存用户消息 ==========

    // 构造 Message 对象。
    // role 为 "user"，content 为 message。
    // 调用 _sessionManager.addMessage() 保存消息。
    // 如果保存失败，直接返回空字符串。
    Message userMessage("user",message);
    if(!_sessionManager.addMessage(sessionId, userMessage))
    {
        ERR("Failed to save user message");
        return "";
    }


    // ========== 6. 获取历史消息 ==========

    // 调用 _sessionManager.getSessionHistory()
    // 获取完整的 vector<Message>。
    std::vector<Message> history =
        _sessionManager.getSessionHistory(sessionId);

    if (history.empty()) {
        ERR("Failed to get session history");
        return "";
    }



    // 调用 _llmManager.sendMessage()。
    // 参数分别为：
    // modelName、history、requestParams
    std::string response =
        _llmManager.sendMessage(
            modelName,
            history,
            requestParams);

    if (response.empty()) {
        ERR("Failed to send message to model");
        return "";
    }


    // 使用模型返回的 response 构造 Message。
    // role 为 "assistant"。
    // 调用 _sessionManager.addMessage() 保存。
    Message assistantMessage("assistant",response);
    if(!_sessionManager.addMessage(sessionId, assistantMessage))
    {
        ERR("Failed to save assistant message");
        return "";
    }


    // ========== 9. 返回模型回复 ==========

    // 输出成功日志并返回 response。
    INF("Received response from model: {}", response);
    return response;
}

void ChatSDK::sendMessageStream(
    const std::string& sessionId,
    const std::string& message,
    std::function<void(const std::string&, bool)> callback
) {
    // ========== 1. 检查 SDK 状态 ==========

    if (!_initialized) {
        ERR("ChatSDK is not initialized");
        return;
    }

    if (message.empty() || !callback) {
        ERR("Invalid message or callback");
        return;
    }


    // ========== 2. 获取会话 ==========

    auto session =
        _sessionManager.getSession(sessionId);

    if (!session) {
        ERR("Session not found: {}", sessionId);
        return;
    }

    const std::string modelName =
        session->model_name;


    // ========== 3. 获取模型配置 ==========

    auto it = _configs.find(modelName);

    if (it == _configs.end() ||
        !_llmManager.isModelAvailable(modelName)) {

        ERR("Model is not available: {}", modelName);

        return;
    }


    // ========== 4. 构建请求参数 ==========

    std::map<std::string, std::string> requestParams;

    requestParams["temperature"] =
        std::to_string(it->second->temperature);

    requestParams["max_tokens"] =
        std::to_string(it->second->max_tokens);


    // ========== 5. 保存用户消息 ==========
    // 构造 user Message 并保存。
    // 如果保存失败，直接 return。
        Message userMessage = Message("user",message);
    if(!_sessionManager.addMessage(sessionId, userMessage))
    {
        ERR("Failed to save user message");
        return;
    }


    // ========== 6. 获取历史消息 ==========
    // 获取完整历史消息 history。
    // 如果历史消息为空，直接 return。
    std::vector<Message> history =
        _sessionManager.getSessionHistory(sessionId);

    if (history.empty()) {
        ERR("Failed to get session history");
        return;
    }


    // ========== 7. 包装流式回调 ==========

    bool providerDone = false;

    auto streamCallback =
        [&](const std::string& chunk, bool done) {

            if (!chunk.empty()) {
                callback(chunk, false);
            }

            if (done) {
                providerDone = true;
            }
        };


    // ========== 8. 调用模型流式接口 ==========

    // 调用 _llmManager.sendMessageStream()。
    // 接收返回的完整 response。
    std::string response =_llmManager.sendMessageStream(
        modelName,
        history,
        requestParams,
        streamCallback);
    // ========== 9. 检查模型响应 ==========

    // 判断 providerDone 是否为 true，
    // response 是否非空。
    // 如果响应不完整或失败：
    // 记录错误，不保存 assistant 消息，直接 return。
    if(!providerDone || response.empty())
    {
        ERR("model response failed");
        return;
    }


    // ========== 10. 保存完整助手回复 ==========

    // 使用完整 response 构造 assistant Message。
    // 保存到 SessionManager。
    // 如果保存失败，直接 return。
    Message assistantMessage("assistant",response);
    if(!_sessionManager.addMessage(sessionId, assistantMessage))
    {
        ERR("Failed to save assistant message");
        return;
    }


    // ========== 11. 通知客户端成功完成 ==========

    // 调用 callback("", true)。
    // 输出成功日志。
    callback("",true);
    INF("get streamMessages successfully");
    return;
}

} // namespace ai_chat_sdk
