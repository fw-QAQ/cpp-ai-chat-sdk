
#include "ILLMProvider.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace ai_chat_sdk;

class MockProvider : public ILLMProvider {
public:
    explicit MockProvider(std::string name)
        : name_(std::move(name))
    {}

    bool initModel(
        const std::map<std::string, std::string>&
    ) override {
        _isAvailable = true;
        return true;
    }

    bool isAvailable() override {
        return _isAvailable;
    }

    std::string getModelName() const override {
        return name_;
    }

    std::string getModelDesc() const override {
        return "Mock LLM provider for testing";
    }

    std::string sendMessage(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>&
    ) override {
        if (!_isAvailable || messages.empty()) {
            return "";
        }

        return "[" + name_ + "] Reply to: "
            + messages.back().content;
    }

    std::string sendMessageStream(
        const std::vector<Message>& messages,
        const std::map<std::string, std::string>& params,
        std::function<void(const std::string&, bool)> callback
    ) override {
        std::string result = sendMessage(messages, params);

        if (callback) {
            callback(result, true);
        }

        return result;
    }

private:
    std::string name_;
};

int main() {
    std::unique_ptr<ILLMProvider> provider =
        std::make_unique<MockProvider>("mock-deepseek");

    provider->initModel({});

    std::vector<Message> messages;
    messages.emplace_back("user", "Hello!");

    std::cout << provider->getModelName() << '\n';

    std::cout << provider->sendMessage(messages, {}) << '\n';

    provider = std::make_unique<MockProvider>("mock-gpt");
    provider->initModel({});

    std::cout << provider->getModelName() << '\n';

    provider->sendMessageStream(
        messages,
        {},
        [](const std::string& data, bool done) {
            std::cout << "Stream: " << data << '\n';
            std::cout << "Done: " << std::boolalpha << done << '\n';
        }
    );

    return 0;
}
