
#include "../include/ChatGPTProvider.h"

#include <bite_scaffold/log.h>
#include <bite_scaffold/json_util.h>

#include <httplib.h>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <cmath>
#include <exception>
#include <string>



namespace ai_chat_sdk {

// ========== 1. 初始化模型 ==========

bool ChatGPTProvider::initModel(
    const std::map<std::string, std::string>& model_config
) {
    _isAvailable = false;
    _api_key.clear();
    _endpoint.clear();

    auto key_it = model_config.find("api_key");

    if (key_it == model_config.end() ||
        key_it->second.empty()) {
        ERR("ChatGPTProvider: api_key is missing");
        return false;
    }

    _api_key = key_it->second;

    auto endpoint_it = model_config.find("endpoint");

    _endpoint = endpoint_it == model_config.end()
        ? "https://api.hopeplay.online"
        : endpoint_it->second;

    if (_endpoint.empty()) {
        ERR("ChatGPTProvider: endpoint is empty");
        return false;
    }

    // 去掉末尾的 /，避免接口路径拼接问题
    while (!_endpoint.empty() && _endpoint.back() == '/') {
        _endpoint.pop_back();
    }

    _isAvailable = true;

    INF("ChatGPTProvider initialized: {}", _endpoint);
    return true;
}


// ========== 2. 模型基本信息 ==========

bool ChatGPTProvider::isAvailable() {
    return _isAvailable;
}

std::string ChatGPTProvider::getModelName() const {
    return "gpt-6-sol";
}

std::string ChatGPTProvider::getModelDesc() const {
    return "OpenAI GPT-6 Sol reasoning model";
}


// ========== 3. 普通聊天 ==========

std::string ChatGPTProvider::sendMessage(
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& request_param
) {
    // 检查初始化状态
    if (!isAvailable()) {
        ERR("ChatGPTProvider is not initialized");
        return "";
    }

    if (messages.empty()) {
        ERR("ChatGPTProvider: messages is empty");
        return "";
    }

    // ---------- 读取请求参数 ----------

    int max_output_tokens = 2048;

    try {
        auto temp_it = request_param.find("temperature");


        auto tokens_it =
            request_param.find("max_output_tokens");

        if (tokens_it != request_param.end()) {
            max_output_tokens = std::stoi(tokens_it->second);
        }
    }
    catch (const std::exception& e) {
        ERR("Invalid GPT request parameter: {}", e.what());
        return "";
    }


    // ---------- 构造消息数组 ----------

    Json::Value json_messages(Json::arrayValue);

    for (const auto& msg : messages) {
        Json::Value item(Json::objectValue);

        item["role"] = msg.role;
        item["content"] = msg.content;

        json_messages.append(item);
    }

    // ---------- 构造 Responses API 请求 ----------

    Json::Value root(Json::objectValue);

    root["model"] = getModelName();
    root["input"] = json_messages;

    root["max_output_tokens"] = max_output_tokens;
    root["stream"] = false;
    root["store"] = false;

    // 第一次实验关闭额外推理，方便验证普通文本返回
    root["reasoning"]["effort"] = "none";

    // 复用微服务脚手架的 JSON 序列化
    std::string request_body;

    if (!bitejson::serialize(root, &request_body)) {
        ERR("GPT request serialization failed");
        return "";
    }

    // ---------- HTTP 请求 ----------

    httplib::Client client(_endpoint);

    client.set_connection_timeout(30, 0);
    client.set_read_timeout(60, 0);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + _api_key}
    };

    // Content-Type 由 Post 最后一个参数设置一次
    auto res = client.Post(
        "/v1/responses",
        headers,
        request_body,
        "application/json"
    );

    if (!res) {
        ERR("GPT network error: {}",
            httplib::to_string(res.error()));
        return "";
    }

    if (res->status != 200) {
        ERR("GPT HTTP error: {}", res->status);
        ERR("GPT error details: {}",
            res->body.substr(0, 1024));
        return "";
    }

    // ---------- JSON 反序列化 ----------

    Json::Value response;
    std::string err;

    if (!bitejson::unserialize(
            res->body, &response, &err)) {
        ERR("GPT JSON parse failed: {}", err);
        return "";
    }

    if (!response.isObject()) {
        ERR("GPT response is not a JSON object");
        return "";
    }

    // 防止把未完成的响应当成正常答案
    if (!response["status"].isString() ||
        response["status"].asString() != "completed") {
        ERR("GPT response not completed");
        return "";
    }

    // TODO 1:
    // 判断 response["output"] 是否为数组。
    if (!response["output"].isArray()) {
        ERR("GPT response output is not an array");
        return "";
    }

    // TODO 2:
    // 遍历 output 数组。
    // 找到 type == "message" 的输出项。
    Json::Value message;

    for (const auto& item : response["output"]) {
        if (item.isObject() &&
            item["type"].isString() &&
            item["type"].asString() == "message") {
            message = item;
            break;
        }
    }

    if (!message.isObject() ||
        !message["type"].isString() ||
        message["type"].asString() != "message") {
        ERR("ChatGPT response message is not found");
        return "";
    }



    // TODO 3:
    // 遍历 message 中的 content 数组。
    std::string result;

    for (const auto& content_item : message["content"]) {
        if (content_item.isObject() &&
            content_item["type"].isString() &&
            content_item["type"].asString() == "output_text") {
            result += content_item["text"].asString();
        }
    }

    if (result.empty()) {
        ERR("ChatGPT response content is empty");
        return "";
    }

    return result;
}


// ========== 4. 流式聊天：下一轮实现 ==========


std::string ChatGPTProvider::sendMessageStream(
    const std::vector<Message>& messages,
    const std::map<std::string, std::string>& request_param,
    std::function<void(const std::string&, bool)> callback
) {
    // ========== 1. 检查模型状态 ==========
    if (!isAvailable() || messages.empty()) {
        ERR("GPT stream: provider unavailable or messages empty");
        return "";
    }

    // ========== 2. 读取请求参数 ==========
    int max_output_tokens = 1024;

    try {
        auto it = request_param.find("max_output_tokens");
        if (it != request_param.end()) {
            max_output_tokens = std::stoi(it->second);
        }
    } catch (const std::exception& e) {
        ERR("Invalid GPT stream parameter: {}", e.what());
        return "";
    }

    if (max_output_tokens <= 0) {
        ERR("max_output_tokens must be positive");
        return "";
    }

    // ========== 3. 构造 JSON 请求 ==========
    Json::Value input(Json::arrayValue);

    for (const auto& msg : messages) {
        Json::Value item(Json::objectValue);
        item["role"] = msg.role;
        item["content"] = msg.content;
        input.append(item);
    }

    Json::Value root(Json::objectValue);

    root["model"] = getModelName();
    root["input"] = input;
    root["stream"] = true;
    root["store"] = false;
    root["max_output_tokens"] = max_output_tokens;

    // GPT 推理模型先不传 temperature，
    // 以免不同模型之间出现参数兼容问题。

    std::string request_body;

    if (!bitejson::serialize(root, &request_body)) {
        ERR("GPT stream request serialization failed");
        return "";
    }

    // ========== 4. 创建 HTTP 客户端 ==========
    httplib::Client client(_endpoint);

    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    httplib::Request req;

    req.method = "POST";
    req.path = "/v1/responses";
    req.body = request_body;

    // Content-Type 只设置一次
    req.set_header("Authorization", "Bearer " + _api_key);
    req.set_header("Content-Type", "application/json");
    req.set_header("Accept", "text/event-stream");

    // ========== 5. 保存流式执行状态 ==========
    std::string full_answer;

    // SSE 接收缓冲区
    std::string buffer;
    std::string event_data;
    std::string event_name;

    // HTTP 错误响应
    std::string error_body;
    int http_status = -1;

    bool received_completed = false;
    bool stream_failed = false;
    std::string failure_reason;

    auto fail = [&](const std::string& reason) -> bool {
        stream_failed = true;
        failure_reason = reason;
        return false;
    };

    // ========== 6. 处理完整 SSE 事件 ==========
    auto processEvent = [&]() -> bool {
        if (event_data.empty()) {
            event_name.clear();
            return true;
        }

        // 兼容可能出现的结束哨兵。
        // Responses API 仍以 response.completed
        // 作为真正的成功标志。
        if (event_data == "[DONE]") {
            event_data.clear();
            event_name.clear();
            return true;
        }

        Json::Value event;
        std::string err;

        if (!bitejson::unserialize(event_data, &event, &err)) {
            return fail("SSE JSON parse failed: " + err);
        }

        if (!event.isObject()) {
            return fail("Invalid SSE event object");
        }

        // 优先读取 JSON 中的 type；
        // 如果没有，则使用 SSE 的 event: 字段。
        std::string type = event["type"].isString()
            ? event["type"].asString()
            : event_name;

        // ---------- 模型输出的增量文本 ----------
        if (type == "response.output_text.delta") {
            if (!event["delta"].isString()) {
                return fail("Invalid GPT text delta");
            }

            std::string delta = event["delta"].asString();

            if (!delta.empty()) {
                // 保存到完整回答
                full_answer += delta;

                // 把增量交给业务程序
                if (callback) {
                    try {
                        callback(delta, false);
                    } catch (...) {
                        return fail("GPT user callback exception");
                    }
                }
            }
        }

        // ---------- 正常完成 ----------
        else if (type == "response.completed") {
            const Json::Value& response = event["response"];

            if (!response.isObject() ||
                !response["status"].isString() ||
                response["status"].asString() != "completed") {
                return fail("GPT response.completed has invalid status");
            }

            received_completed = true;
        }

        // ---------- 服务端异常 ----------
        else if (type == "response.failed" ||
                 type == "response.incomplete" ||
                 type == "error") {
            std::string detail = type;

            if (event["message"].isString()) {
                detail += ": " + event["message"].asString();
            } else if (event["response"].isObject()) {
                const auto& response = event["response"];

                if (response["error"].isObject() &&
                    response["error"]["message"].isString()) {
                    detail += ": " +
                        response["error"]["message"].asString();
                } else if (
                    response["incomplete_details"].isObject() &&
                    response["incomplete_details"]["reason"].isString()) {
                    detail += ": " +
                        response["incomplete_details"]["reason"].asString();
                }
            }

            return fail(detail);
        }

        // 其他事件（created、in_progress、
        // output_text.done 等）不需要处理。

        event_data.clear();
        event_name.clear();

        return true;
    };

    // ========== 7. 处理 HTTP 响应头 ==========
    req.response_handler =
        [&](const httplib::Response& res) -> bool {
            http_status = res.status;
            return true;
        };

    // ========== 8. 接收并拼接 SSE 数据 ==========
    req.content_receiver =
        [&](const char* data,
            size_t len,
            uint64_t /*offset*/,
            uint64_t /*total*/) -> bool {

            // HTTP 错误响应不进入 SSE 解析器
            if (http_status != 200) {
                if (error_body.size() < 2048) {
                    size_t remaining = 2048 - error_body.size();
                    error_body.append(data, std::min(len, remaining));
                }
                return true;
            }

            // 追加本次收到的数据
            buffer.append(data, len);

            size_t pos;

            // 逐行解析 SSE
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                // 兼容 CRLF
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                // 空行：一个 SSE 事件结束
                if (line.empty()) {
                    if (!processEvent()) {
                        return false;
                    }
                }

                // data: 行
                else if (line.rfind("data:", 0) == 0) {
                    std::string value = line.substr(5);

                    if (!value.empty() && value.front() == ' ') {
                        value.erase(0, 1);
                    }

                    if (!event_data.empty()) {
                        event_data += '\n';
                    }

                    event_data += value;
                }

                // event: 行
                else if (line.rfind("event:", 0) == 0) {
                    event_name = line.substr(6);

                    if (!event_name.empty() &&
                        event_name.front() == ' ') {
                        event_name.erase(0, 1);
                    }
                }
            }

            return true;
        };

    // ========== 9. 发送请求 ==========
    auto res = client.send(req);

    // 优先判断解析器错误
    if (stream_failed) {
        ERR("GPT SSE error: {}", failure_reason);
        return "";
    }

    // 网络错误
    if (!res) {
        ERR("GPT streaming network error: {}",
            httplib::to_string(res.error()));
        return "";
    }

    // HTTP 错误
    if (res->status != 200) {
        ERR("GPT streaming HTTP status: {}", res->status);
        ERR("GPT streaming API error: {}", error_body);
        return "";
    }

    // 没有收到完成事件
    if (!received_completed) {
        ERR("GPT stream ended without response.completed");
        return "";
    }

    // ========== 10. 通知正常结束 ==========
    if (callback) {
        callback("", true);
    }

    // ========== 11. 返回完整回答 ==========
    return full_answer;
}


} // namespace ai_chat_sdk
