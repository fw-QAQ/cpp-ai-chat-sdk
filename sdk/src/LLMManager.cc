#include "LLMManager.h"

#include "Log.h"

#include <utility>

namespace ai_chat_sdk {

// ========== 1. 注册模型 ==========

bool LLMManager::registerProvider(
    const std::string& name,
    std::unique_ptr<ILLMProvider> provider
) {
    // 检查 name 是否为空，provider 是否为空。
    if (name.empty()) {
        CHAT_SDK_ERROR("Provider name is empty");
        return false;
    }

    if (!provider) {
        CHAT_SDK_ERROR("Provider is null");
        return false;
    }

    // 将 provider 的所有权转移给 _providers。
    _providers[name] = std::move(provider);
    // 使用 insert_or_assign() 将 ModelInfo
    // 添加到 _models 中。
    _models.insert_or_assign(name, ModelInfo(name));

    // 输出注册成功日志并返回 true。
    CHAT_SDK_INFO("Provider {} registered", name);
    return true;
}


// ========== 2. 初始化指定模型 ==========

bool LLMManager::initModel(
    const std::string& modelName,
    const std::map<std::string, std::string>& modelParams
) {
    // 1. 查找对应的 Provider
    auto it = _providers.find(modelName);

    if (it == _providers.end()) {
        CHAT_SDK_ERROR("Model {} is not registered", modelName);
        return false;
    }

    // 2. 获取模型信息对象的引用
    ModelInfo& info = _models.at(modelName);

    // 3. 初始化实际的 Provider
    bool ret = it->second->initModel(modelParams);

    // 4. 同步模型可用状态
    info._isAvailable = ret;

    if (!ret) {
        CHAT_SDK_ERROR("Model {} initialization failed", modelName);
        return false;
    }

    // 5. 同步模型描述
    info._desc = it->second->getModelDesc();

    // 6. 同步模型服务地址
    auto endpoint_it = modelParams.find("endpoint");

    if (endpoint_it != modelParams.end()) {
        info._endpoint = endpoint_it->second;
    } else {
        auto base_url_it = modelParams.find("base_url");

        if (base_url_it != modelParams.end()) {
            info._endpoint = base_url_it->second;
        }
    }

    CHAT_SDK_INFO("Model {} initialized successfully", modelName);

    return true;
}


// ========== 3. 获取可用模型列表 ==========

std::vector<ModelInfo>
LLMManager::getAvailableModel() const
{
    std::vector<ModelInfo> availableModels;
    // 遍历 _models。
    // 将 _isAvailable 为 true 的 ModelInfo
    // 添加到 availableModels 中。
    for(auto& model : _models) {
        if(model.second._isAvailable) {
            availableModels.push_back(model.second);
        }
    }
    return availableModels;
}


// ========== 4. 检查模型是否可用 ==========

bool LLMManager::isModelAvailable(
    const std::string& modelName
) const {
    // 在 _models 中查找指定模型。
    // 如果模型存在且 _isAvailable 为 true，
    // 则返回 true，否则返回 false。
    if(_models.find(modelName) == _models.end()) {
        CHAT_SDK_ERROR("Model {} is not registered", modelName);
        return false;
    }
    return _models.at(modelName)._isAvailable;
}


// ========== 5. 普通聊天 ==========

std::string LLMManager::sendMessage(
    const std::string& modelName,
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& requestParams
) {
    // 检查模型是否注册
    auto it = _providers.find(modelName);

    if (it == _providers.end()) {
        CHAT_SDK_ERROR("Model {} is not registered", modelName);
        return "";
    }

    // 检查模型是否可用
    if (!it->second->isAvailable()) {
        CHAT_SDK_ERROR("Model {} is not available", modelName);
        return "";
    }

    // 通过多态调用对应模型的普通聊天接口
    return it->second->sendMessage(
        messages,
        requestParams
    );
}


// ========== 6. 流式聊天 ==========

std::string LLMManager::sendMessageStream(
    const std::string& modelName,
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& requestParam,
    std::function<void(const std::string&, bool)> callback
) {
    // 检查模型是否注册
    auto it = _providers.find(modelName);

    if (it == _providers.end()) {
        CHAT_SDK_ERROR("Model {} is not registered", modelName);
        return "";
    }

    // 检查模型是否可用
    if (!it->second->isAvailable()) {
        CHAT_SDK_ERROR("Model {} is not available", modelName);
        return "";
    }

    // 通过多态调用对应模型的流式聊天接口
    return it->second->sendMessageStream(
        messages,
        requestParam,
        callback
    );
}

} // namespace ai_chat_sdk
