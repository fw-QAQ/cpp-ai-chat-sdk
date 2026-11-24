#include "../include/QwenProvider.h"
#include "../include/SSEParser.h"
#include <bite_scaffold/json_util.h>
#include <bite_scaffold/log.h>

#include <httplib.h>
#include <jsoncpp/json/json.h>

#include <algorithm>
#include <cstdint>

namespace ai_chat_sdk {

// 初始化模型
bool QwenProvider::initModel(
    const std::map<std::string, std::string> &model_config) {
  _isAvailable = false;
  auto it = model_config.find("api_key");

  if (it == model_config.end() || it->second.empty()) {
    ERR("QwenProvider initialization failed: missing API key");
    return false;
  }
  _api_key = it->second;
  it = model_config.find("base_url");
  std::string base_url;

  if (it == model_config.end()) {
    base_url = "https://maas.qianwenaiapi.com";
  } else {
    base_url = it->second;
  }

  _endpoint = base_url;
  _isAvailable = true;
  INF("Init success!!!!");
  INF("Qwen provider initialized successfull whith endpoint: {}",
      _endpoint);
  return true;
}

// 判断模型是否已经初始化
bool QwenProvider::isAvailable() { return _isAvailable; }

// 返回模型名称
std::string QwenProvider::getModelName() const { return "qwen3.8-flash"; }

// 返回模型描述
std::string QwenProvider::getModelDesc() const {
  return "Qwen-4.1-flash 模型是深度求索公司基于 Qwen 4.1 "
         "模型的快速响应模型，支持实时对话。";
}

// 发送普通聊天请求
std::string QwenProvider::sendMessage(
    const std::vector<Message> &messages,
    const std::map<std::string, std::string> &request_param) {

  // 检查模型是否已初始化。
  if (!isAvailable()) {
    ERR("QwenProvider sendMessage failed: model not initialized");
    return "";
  }

  // 读取 temperature 和 max_tokens。
  // 对参数进行合理的合法性检查。
  double temperature = 0.7;
  int max_tokens = 2048;
  if (request_param.find("temperature") != request_param.end()) {
    temperature = std::stod(request_param.at("temperature"));
  }
  if (request_param.find("max_tokens") != request_param.end()) {
    max_tokens = std::stoi(request_param.at("max_tokens"));
  }

  // 将 vector<Message> 转为 JSON 数组。
  // 每条消息保留 role 与 content。

  Json::Value root(Json::objectValue);
  root["enable_thinking"] = false;
  root["model"] = getModelName().c_str();
  root["stream"] = false;

  // 创建消息数组
  Json::Value json_messages(Json::arrayValue);

  for (const auto &msg : messages) {
    Json::Value item(Json::objectValue);

    item["role"] = msg.role;
    item["content"] = msg.content;

    json_messages.append(item);
  }

  root["messages"] = json_messages;
  root["temperature"] = temperature;
  root["max_tokens"] = max_tokens;
  // 使用已经封装的 JSON 序列化接口
  std::string request_body;

  if (!bitejson::serialize(root, &request_body)) {
    ERR("Qwen request JSON serialization failed");
    return "";
  }

  httplib::Client client(_endpoint);
  client.set_connection_timeout(30, 0);
  client.set_read_timeout(60, 0);
  httplib::Headers headers = {{"Authorization", "Bearer " + _api_key},
                              {"Accept", "application/json"}};

  httplib::Result res = client.Post("/compatible-mode/v1/chat/completions", headers, request_body,
                                    "application/json");
  if (!res) {
    ERR("Failed to connect to Qwen API - check network and SSL "
        "configuration: {}",
        httplib::to_string(res.error()));
    return "";
  }
  if (res->status != 200) {
    ERR("Qwen API response HTTP status code not 200: {}", res->status);
    ERR("Qwen API error details: {}", res->body);
    return "";
  }
  std::string err;
  Json::Value response;
  if (!bitejson::unserialize(res->body, &response, &err)) {
    ERR("QwenProvider sendMessage failed: JSON response deserialization "
        "failed: {}",
        err);
    return "";
  }

  if (!response.isObject() || !response["choices"].isArray() ||
      response["choices"].empty()) {
    ERR("Qwen response: invalid choices");
    return "";
  }

  const Json::Value &choice = response["choices"][0];

  if (!choice.isObject() || !choice["message"].isObject() ||
      !choice["message"]["content"].isString()) {
    ERR("Qwen response: invalid message content");
    return "";
  }

  return choice["message"]["content"].asString();
}

std::string QwenProvider::sendMessageStream(
    const std::vector<Message> &messages,
    const std::map<std::string, std::string> &request_param,
    std::function<void(const std::string &, bool)> callback) {

  // 检查模型是否初始化、messages 是否为空。
      if (!isAvailable()) {
    ERR("QwenProvider sendMessage failed: model not initialized");
    return "";
  }

  // 读取 temperature 和 max_tokens。
  // 对参数进行合理的合法性检查。
  double temperature = 0.7;
  int max_tokens = 2048;
  if (request_param.find("temperature") != request_param.end()) {
    temperature = std::stod(request_param.at("temperature"));
  }
  if (request_param.find("max_tokens") != request_param.end()) {
    max_tokens = std::stoi(request_param.at("max_tokens"));
  }

  // 将 vector<Message> 转为 JSON 数组。
  // 每条消息保留 role 与 content。

  Json::Value root(Json::objectValue);
  root["enable_thinking"] = false;
  root["model"] = getModelName().c_str();
  root["stream"] = true;

  // 创建消息数组
  Json::Value json_messages(Json::arrayValue);

  for (const auto &msg : messages) {
    Json::Value item(Json::objectValue);

    item["role"] = msg.role;
    item["content"] = msg.content;

    json_messages.append(item);
  }

  root["messages"] = json_messages;
  root["temperature"] = temperature;
  root["max_tokens"] = max_tokens;
  // 使用已经封装的 JSON 序列化接口
  std::string request_body;

  if (!bitejson::serialize(root, &request_body)) {
    ERR("Qwen request JSON serialization failed");
    return "";
  }

  httplib::Client client(_endpoint);
  client.set_connection_timeout(30, 0);
  client.set_read_timeout(60, 0);

// 1. 保存流式执行状态
std::string full_answer;
bool received_done = false;


// 2. 创建 SSEParser
SSEParser parser(
    [&](const std::string& content, bool done) {
        if (done) {
            received_done = true;
            return;
        }

        full_answer += content;

        if (callback) {
            callback(content, false);
        }
    }
);

// 3. 构造 HTTP 请求
httplib::Request req;

req.method = "POST";
req.path = "/compatible-mode/v1/chat/completions";
req.body = request_body;

req.set_header("Authorization", "Bearer " + _api_key);
req.set_header("Content-Type", "application/json");
req.set_header("Accept", "text/event-stream");

// 4. 保存服务器返回的 HTTP 状态
int http_status = -1;
std::string error_body;

// 5. 收到 HTTP 响应头时执行
req.response_handler =
    [&](const httplib::Response& res) -> bool {
        http_status = res.status;
        return true;
    };

// 6. 收到 HTTP 响应体数据时执行
req.content_receiver =
    [&](const char* data,
        size_t len,
        uint64_t /*offset*/,
        uint64_t /*total*/) -> bool {

        if (http_status != 200) {
            return true;
        }
        // feed 失败，通知 cpp-httplib 停止接收
        return parser.feed(data, len);
    };

// 7. 执行 HTTP 请求

auto res = client.send(req);

// 1. 优先判断 SSE 解析错误
if (parser.hasError())
{
    ERR("Qwen SSE parse failed: {}",
        parser.errorMessage());

    return "";
}

// 2. 检查网络传输
if (!res)
{
    ERR("Qwen streaming network error: {}",
        httplib::to_string(res.error()));

    return "";
}

// 3. 检查 HTTP 状态
if (res->status != 200)
{
    ERR("Qwen streaming HTTP status: {}",
        res->status);

    return "";
}

// 4. 检查是否收到 SSE 结束标记
if (!parser.isDone())
{
    ERR("Qwen stream ended without [DONE]");

    return "";
}

// 5. 通知外部调用者正常完成
if (callback)
{
    callback("", true);
}

// 6. 返回完整回答
return full_answer;

}

} // namespace ai_chat_sdk
