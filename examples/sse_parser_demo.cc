
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

    void feed(const char* data, size_t len) {
        buffer_.append(data, len);

        size_t pos;

        // 每次只处理完整的一行
        while ((pos = buffer_.find('\n')) != std::string::npos) {
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 1);

            // 兼容 CRLF
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                // 空行表示当前 SSE 事件结束
                if (!event_data_.empty()) {
                    processEvent(event_data_);
                    event_data_.clear();
                }
            } else if (line.rfind("data:", 0) == 0) {
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
    }

private:

void processEvent(const std::string& data) {
    // 1. 判断流是否正常结束
    if (data == "[DONE]") {
        callback_("", true);
        return;
    }

    // 2. 将 JSON 字符串反序列化
    Json::Value root;
    std::string err;

    if (!bitejson::unserialize(data, &root, &err)) {
        std::cerr << "SSE JSON parse failed: "
                  << err << '\n';
        return;
    }

    // 3. 检查 choices 数组
    if (!root.isObject() ||
        !root["choices"].isArray() ||
        root["choices"].empty()) {
        return;
    }

    // 4. 提取 delta 对象
    const Json::Value& delta =
        root["choices"][0]["delta"];

    if (!delta.isObject()) {
        return;
    }

    // 5. 提取增量文本
    const Json::Value& content = delta["content"];

    if (content.isString() && !content.asString().empty()) {
        callback_(content.asString(), false);
    }
}


    std::string buffer_;
    std::string event_data_;
    Callback callback_;
};


int main() {
    SSEParser parser(
        [](const std::string& content, bool done) {
            if (done) {
                std::cout << "\nStream finished\n";
            } else {
                std::cout << content << std::flush;
            }
        }
    );

    // 模拟 DeepSeek 的真实 SSE 消息格式
    std::string stream =
        R"(data: {"choices":[{"delta":{"content":"Hello"}}]})"
        "\n\n"
        R"(data: {"choices":[{"delta":{"content":" world"}}]})"
        "\n\n"
        "data: [DONE]\n\n";

    // 故意每次只发送 7 字节，模拟任意分段
    for (size_t i = 0; i < stream.size(); i += 7) {
        size_t len = std::min<size_t>(
            7, stream.size() - i
        );

        parser.feed(stream.data() + i, len);
    }

    return 0;
}
