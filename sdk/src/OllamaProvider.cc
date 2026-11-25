#include "OllamaProvider.h"

#include "Log.h"
#include "JsonUtil.h"

#include <httplib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>

namespace ai_chat_sdk {

bool OllamaProvider::initModel(
    const std::map<std::string, std::string>& model_config
) {
    _isAvailable = false;

    _model_name.clear();
    _model_desc.clear();
    _endpoint.clear();

    // 初始化模型名称
    auto it = model_config.find("model_name");

    if (it == model_config.end() || it->second.empty()) {
        CHAT_SDK_ERROR("Ollama model name is empty");
        return false;
    }

    _model_name = it->second;

    // 初始化模型描述
    it = model_config.find("model_desc");

    if (it == model_config.end() || it->second.empty()) {
        CHAT_SDK_ERROR("Ollama model description is empty");
        return false;
    }

    _model_desc = it->second;

    // 初始化服务端点
    it = model_config.find("endpoint");

    if (it == model_config.end() || it->second.empty()) {
        CHAT_SDK_ERROR("Ollama endpoint is empty");
        return false;
    }

    _endpoint = it->second;

    // 删除末尾的斜杠
    while (!_endpoint.empty() && _endpoint.back() == '/') {
        _endpoint.pop_back();
    }

    if (_endpoint.empty()) {
        CHAT_SDK_ERROR("Invalid Ollama endpoint");
        return false;
    }

    _isAvailable = true;

    CHAT_SDK_INFO("Ollama provider initialized: {}", _endpoint);
    CHAT_SDK_INFO("Ollama model: {}", _model_name);

    return true;
}

bool OllamaProvider::isAvailable() {
    return _isAvailable;
}

std::string OllamaProvider::getModelName() const {
    return _model_name;
}

std::string OllamaProvider::getModelDesc() const {
    return _model_desc;
}
namespace {

bool buildRequestBody(
    const std::string& model_name,
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& request_param,
    bool stream,
    std::string* request_body
) {
    double temperature = 0.7;
    int max_tokens = 1024;
    int num_ctx = 4096;

    try {
        if (request_param.count("temperature")) {
            temperature = std::stod(
                request_param.at("temperature")
            );
        }

        if (request_param.count("max_tokens")) {
            max_tokens = std::stoi(
                request_param.at("max_tokens")
            );
        }

        if (request_param.count("num_ctx")) {
            num_ctx = std::stoi(
                request_param.at("num_ctx")
            );
        }
    } catch (const std::exception& e) {
        CHAT_SDK_ERROR("Invalid Ollama request parameter: {}", e.what());
        return false;
    }

    // 参数校验
    if (!std::isfinite(temperature) ||
        temperature < 0.0 ||
        temperature > 2.0 ||
        max_tokens <= 0 ||
        num_ctx <= 0) {

        CHAT_SDK_ERROR("Invalid Ollama generation options");
        return false;
    }

    // 构造历史消息
    Json::Value messages_array(Json::arrayValue);

    for (const auto& message : messages) {
        Json::Value msg(Json::objectValue);

        msg["role"] = message.role;
        msg["content"] = message.content;

        messages_array.append(msg);
    }

    // 构造模型参数
    Json::Value options(Json::objectValue);

    options["temperature"] = temperature;
    options["num_ctx"] = num_ctx;
    options["num_predict"] = max_tokens;

    // 构造完整请求体
    Json::Value root(Json::objectValue);

    root["model"] = model_name;
    root["messages"] = messages_array;
    root["stream"] = stream;
    root["options"] = options;
    root["keep_alive"] = "5m";

    // 复用微服务脚手架
    return ai_chat_sdk::json::serialize(root, request_body);
}

} // anonymous namespace
std::string OllamaProvider::sendMessage(
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& request_param
) {
    if (!isAvailable()) {
        CHAT_SDK_ERROR("OllamaProvider is not initialized");
        return "";
    }

    if (messages.empty()) {
        CHAT_SDK_ERROR("Ollama messages is empty");
        return "";
    }

    // 1. 构造请求体
    std::string request_body;

    if (!buildRequestBody(
            _model_name,
            messages,
            request_param,
            false,
            &request_body)) {

        CHAT_SDK_ERROR("Failed to build Ollama request");
        return "";
    }

    // 2. 创建 HTTP 客户端
    httplib::Client client(_endpoint);

    client.set_connection_timeout(30, 0);
    client.set_read_timeout(300, 0);

    // 3. 发送请求
    // Ollama 原生本地接口不需要 Authorization
    auto res = client.Post(
        "/api/chat",
        request_body,
        "application/json"
    );

    // 4. 网络错误处理
    if (!res) {
        CHAT_SDK_ERROR("Ollama network error: {}",
            httplib::to_string(res.error()));
        return "";
    }

    // 5. HTTP 状态码校验
    if (res->status != 200) {
        CHAT_SDK_ERROR("Ollama HTTP error: {}", res->status);
        CHAT_SDK_ERROR("Ollama error details: {}",
            res->body.substr(0, 1024));

        return "";
    }

    // 6. 反序列化
    Json::Value response;
    std::string err;

    if (!ai_chat_sdk::json::unserialize(
            res->body, &response, &err)) {

        CHAT_SDK_ERROR("Ollama JSON parse failed: {}", err);
        return "";
    }

    // 7. 检查响应结构
    if (!response.isObject()) {
        CHAT_SDK_ERROR("Invalid Ollama response object");
        return "";
    }

    if (response.isMember("error")) {
        CHAT_SDK_ERROR("Ollama returned an error");
        return "";
    }

    if (!response["done"].isBool() ||
        !response["done"].asBool()) {

        CHAT_SDK_ERROR("Ollama response is not completed");
        return "";
    }

    // 8. 提取模型回答
    const Json::Value& message = response["message"];

    if (!message.isObject() ||
        !message["content"].isString()) {

        CHAT_SDK_ERROR("Invalid Ollama message format");
        return "";
    }

    return message["content"].asString();
}
std::string OllamaProvider::sendMessageStream(
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& request_param,
    std::function<void(const std::string&, bool)> callback
) {
    if (!isAvailable() || messages.empty()) {
        CHAT_SDK_ERROR("Ollama provider unavailable or messages empty");
        return "";
    }

    // ========== 1. 构造请求体 ==========

    std::string request_body;

    if (!buildRequestBody(
            _model_name,
            messages,
            request_param,
            true,
            &request_body)) {

        CHAT_SDK_ERROR("Failed to build Ollama stream request");
        return "";
    }

    // ========== 2. 创建客户端 ==========

    httplib::Client client(_endpoint);

    client.set_connection_timeout(30, 0);
    client.set_read_timeout(300, 0);

    // ========== 3. 流式状态变量 ==========

    std::string buffer;
    std::string full_answer;

    std::string error_body;
    std::string error_message;

    int http_status = -1;

    bool got_error = false;
    bool stream_finished = false;

    // ========== 4. 创建请求对象 ==========

    httplib::Request req;

    req.method = "POST";
    req.path = "/api/chat";
    req.body = request_body;

    req.set_header(
        "Content-Type",
        "application/json"
    );

    req.set_header(
        "Accept",
        "application/x-ndjson"
    );

    // ========== 5. 响应头回调 ==========

    req.response_handler =
        [&](const httplib::Response& response) -> bool {

            http_status = response.status;

            return true;
        };

    // ========== 6. 内容接收回调 ==========

    req.content_receiver =
        [&](const char* data,
            size_t len,
            uint64_t offset,
            uint64_t total) -> bool {

            // HTTP 错误响应不进入 NDJSON 解析
            if (http_status != 200) {
                if (error_body.size() < 2048) {
                    size_t remaining =
                        2048 - error_body.size();

                    error_body.append(
                        data,
                        std::min(len, remaining)
                    );
                }

                return true;
            }

            if (got_error) {
                return false;
            }

            // 追加数据到缓冲区
            buffer.append(data, len);

            size_t pos = 0;

            // 处理所有完整的 JSON 行
            while (
                (pos = buffer.find('\n'))
                != std::string::npos
            ) {
                std::string jsonLine =
                    buffer.substr(0, pos);

                buffer.erase(0, pos + 1);

                // 兼容 CRLF
                if (!jsonLine.empty() &&
                    jsonLine.back() == '\r') {

                    jsonLine.pop_back();
                }

                // 跳过空行
                if (jsonLine.empty()) {
                    continue;
                }

                // 使用 ai_chat_sdk::json::unserialize()
                // 将 jsonLine 转换成 Json::Value。
                //
                // 如果解析失败：
                // 设置 got_error 和 error_message，
                // return false;
                Json::Value response;
                std::string err;

                if (!ai_chat_sdk::json::unserialize(jsonLine, &response, &err)) {
                    got_error = true;
                    error_message = "Failed to parse JSON: " + err;
                    return false;
                }

                // 检查 JSON 是否包含 error 字段。
                //
                // 如果存在服务端错误，
                // 设置错误状态并 return false;
                if (response.isObject() &&
                    response["error"].isString()) {
                    got_error = true;
                    error_message = response["error"].asString();
                    CHAT_SDK_ERROR("Ollama NDJSON error: {}", error_message);
                    return false;
                }

                // 检查 done 字段是否为布尔类型。
                if (!response.isObject() ||
                    !response["done"].isBool()) {
                    CHAT_SDK_ERROR("Ollama NDJSON done field not boolean");
                    continue;
                }

                bool done = response["done"].asBool();

                // 提取 message.content。
                //
                // 如果存在非空字符串：
                // 追加至 full_answer，
                // 并执行 callback(content, false)。
                if (!response["done"].isBool()) {
                    got_error = true;
                    error_message = "Ollama NDJSON done field not boolean";
                    return false;
                }

                std::string content =
                    response["message"]["content"].asString();

                if (!content.empty())
                {
                    full_answer += content;

                    if (callback)
                    {
                    callback(content, false);
                    }
                }
                if (done) {
                    stream_finished = true;
                }
            }

            return true;
        };

    // ========== 7. 发送请求 ==========

    auto res = client.send(req);

    // 优先处理解析错误
    if (got_error) {
        CHAT_SDK_ERROR("Ollama NDJSON error: {}", error_message);
        return "";
    }

    // 网络错误
    if (!res) {
        CHAT_SDK_ERROR("Ollama streaming network error: {}",
            httplib::to_string(res.error()));

        return "";
    }

    // HTTP 错误
    if (res->status != 200) {
        CHAT_SDK_ERROR("Ollama streaming HTTP error: {}",
            res->status);

        CHAT_SDK_ERROR("Ollama streaming error: {}", error_body);

        return "";
    }

    // 没有收到 done: true
    if (!stream_finished) {
        CHAT_SDK_ERROR("Ollama stream ended without done=true");
        return "";
    }

    // ========== 8. 通知业务层正常结束 ==========

    if (callback) {
        callback("", true);
    }

    return full_answer;
}

} // namespace ai_chat_sdk
