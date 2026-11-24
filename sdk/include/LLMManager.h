#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <map>

#include "common.h"
#include "ILLMProvider.h"

namespace ai_chat_sdk {

class LLMManager {
public:
    // 注册 LLM 提供者
    bool registerProvider(
        const std::string& name,
        std::unique_ptr<ILLMProvider> provider
    );

    // 初始化指定模型
    bool initModel(
        const std::string& modelName,
        const std::map<std::string, std::string>& modelParams
    );

    // 获取可用模型列表
    std::vector<ModelInfo> getAvailableModel() const;

    // 检查指定模型是否可用
    bool isModelAvailable(
        const std::string& modelName
    ) const;

    // 发送消息到指定模型
    std::string sendMessage(
        const std::string& modelName,
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& requestParams
    );

    // 发送消息到指定模型，流式响应
    std::string sendMessageStream(
        const std::string& modelName,
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& requestParam,
        std::function<void(const std::string&, bool)> callback
    );

private:
    // 保存实际的模型对象
    std::unordered_map<
        std::string,
        std::unique_ptr<ILLMProvider>
    > _providers;

    // 保存模型信息
    std::unordered_map<
        std::string,
        ModelInfo
    > _models;
};

} // namespace ai_chat_sdk
