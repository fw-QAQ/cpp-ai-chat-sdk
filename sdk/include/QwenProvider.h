
#pragma once

#include "ILLMProvider.h"

namespace ai_chat_sdk {

class QwenProvider : public ILLMProvider {
public:
    bool initModel(
        const std::map<std::string, std::string>& model_config
    ) override;

    bool isAvailable() override;

    std::string getModelName() const override;

    std::string getModelDesc() const override;

    std::string sendMessage(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param
    ) override;

    std::string sendMessageStream(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& request_param,
        std::function<void(const std::string&, bool)> callback
    ) override;
};

} // namespace ai_chat_sdk
