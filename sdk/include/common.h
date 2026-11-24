
#pragma once

#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace ai_chat_sdk {

// 单条聊天消息
struct Message {
    std::string id; // 消息 ID，唯一标识符
    std::string role;// 消息角色，user、assistant、system 等
    std::string content;// 消息内容
    std::time_t timestamp;// 消息发送时间戳

    Message(const std::string& r, const std::string& c)
        : role(r),
          content(c),
          timestamp(std::time(nullptr))
    {}
};

// 一次聊天会话
struct Session {
    std::string id; // 会话 ID，唯一标识符
    std::string model_name;// 模型名称
    std::vector<Message> messages;// 会话中的所有消息

    std::time_t created_at;// 会话创建时间戳
    std::time_t updated_at;//  会话更新时间戳

    explicit Session(const std::string& model)
        : model_name(model),
          created_at(std::time(nullptr)),
          updated_at(created_at)
    {}
};

// 通用模型配置
struct Config {
    virtual ~Config() = default;

    std::string model_name;
    double temperature = 0.7;
    int max_tokens = 2048;
};

// 云端 API 模型配置
struct ApiConfig : public Config {
    std::string api_key;// 接⼊模型时认证信息
};

// 模型基本信息
struct ModelInfo {
    std::string _name;//模型名称
    std::string _desc;//模型描述
    std::string _provider;//模型提供者
    std::string _endpoint;//base_url
    bool _isAvailable = false;// 是否可用


    explicit ModelInfo(
        const std::string& name,
        const std::string& desc = "",
        const std::string& provider = "",
        const std::string& endpoint = ""
    )
        : _name(name),
          _desc(desc),
          _provider(provider),
          _endpoint(endpoint)
    {}
};

} // namespace ai_chat_sdk
