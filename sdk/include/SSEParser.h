
#include <iostream>
#include <string>
#include <functional>
#include <bite_scaffold/json_util.h>
#include <jsoncpp/json/json.h>

#include <algorithm>
class SSEParser {
public:
    using Callback = std::function<void(
        const std::string&, bool
    )>;

    explicit SSEParser(Callback cb)
        : callback_(std::move(cb)) {}
    // 处理 SSE 数据

bool feed(const char* data, size_t len)
{
    // 已经出错，或者已经正常完成
    if (failed_ || done_) {
        return !failed_;
    }

    buffer_.append(data, len);

    size_t pos;

    while ((pos = buffer_.find('\n')) != std::string::npos)
    {
        std::string line = buffer_.substr(0, pos);

        buffer_.erase(0, pos + 1);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty())
        {
            if (!event_data_.empty())
            {
                // 关键修改：检查 processEvent 返回值
                bool success = processEvent(event_data_);

                event_data_.clear();

                if (!success) {
                    return false;
                }

                if (done_) {
                    return true;
                }
            }
        }
        else if (line.rfind("data:", 0) == 0)
        {
            std::string value = line.substr(5);

            if (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }

            if (!event_data_.empty()) {
                event_data_ += '\n';
            }

            event_data_ += value;
        }
    }

    return true;
}

    bool hasError() const {
        return failed_;
    }

    bool isDone() const {
        return done_;
    }

    std::string errorMessage() const {
        return error_message_;
    }

private:
    bool failed_ = false;
    bool done_ = false;
    std::string buffer_;
    std::string event_data_;
    Callback callback_;
    std::string error_message_;
    // 处理完整的 SSE 事件
     bool processEvent(const std::string& data) {
    // 1. 判断流是否正常结束
    if (data == "[DONE]") {
        done_ = true;
        callback_("", true);
        return true;
    }
    // 2. 将 JSON 字符串反序列化
    Json::Value root;
    std::string err;

    if (!bitejson::unserialize(data, &root, &err)) {
        failed_ = true;
        error_message_ = "JSON parse failed: " + err;

        return false;
    }

   if (!root.isObject())
    {
        failed_ = true;
        error_message_ = "Invalid SSE JSON root";

        return false;
    }

    // 4. 检查服务器是否返回错误事件
    if (root.isMember("error"))
    {
        failed_ = true;
        error_message_ = "DeepSeek returned an SSE error event";

        return false;
    }

    // 5. 检查 choices
    if (!root["choices"].isArray())
    {
        failed_ = true;
        error_message_ = "Invalid SSE choices";

        return false;
    }

    // choices 为空不一定是错误事件
    if (root["choices"].empty()) {
        return true;
    }

    // 6. 获取 delta
    const Json::Value& delta =
        root["choices"][0]["delta"];

    // 有些事件不包含 delta
    if (!delta.isObject()) {
        return true;
    }

    // 7. 获取 content
    const Json::Value& content = delta["content"];

    // 可能是只包含 role 或 reasoning_content 的事件
    if (content.isNull()) {
        return true;
    }

    if (!content.isString())
    {
        failed_ = true;
        error_message_ = "Invalid delta.content type";

        return false;
    }

    // 8. 将文本交给外层
    if (!content.asString().empty() && callback_)
    {
        callback_(content.asString(), false);
    }

    return true;

};

};
