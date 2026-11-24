
#pragma once

#include "common.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ai_chat_sdk {

class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;

    // 初始化模型
    virtual bool initModel(
        const std::map<std::string, std::string>& model_config
    ) = 0;

    // 判断模型是否已初始化并可供调用
    virtual bool isAvailable() = 0;

    // 获取模型名称
    virtual std::string getModelName() const = 0;

    // 获取模型描述
    virtual std::string getModelDesc() const = 0;

    // 普通模式：一次性返回完整回答
    virtual std::string sendMessage(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param
    ) = 0;

    // 流式模式：通过回调接收增量内容
    virtual std::string sendMessageStream(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param,
        std::function<void(const std::string&, bool)> callback
    ) = 0;

protected:
    bool _isAvailable = false;
    std::string _api_key;
    std::string _endpoint;
};

} // namespace ai_chat_sdk
