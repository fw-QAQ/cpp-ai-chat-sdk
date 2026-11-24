#pragma once

#include "ILLMProvider.h"

namespace ai_chat_sdk {

class OllamaProvider : public ILLMProvider {
public:
    OllamaProvider() = default;
    ~OllamaProvider() override = default;

    // 初始化模型
    bool initModel(
        const std::map<std::string, std::string>& model_config
    ) override;

    // 检查初始化状态
    bool isAvailable() override;

    // 获取模型名称
    std::string getModelName() const override;

    // 获取模型描述
    std::string getModelDesc() const override;

    // 普通聊天
    std::string sendMessage(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param
    ) override;

    // 流式聊天
    std::string sendMessageStream(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param,
        std::function<void(const std::string&, bool)> callback
    ) override;

private:
    std::string _model_name;
    std::string _model_desc;
};

} // namespace ai_chat_sdk
