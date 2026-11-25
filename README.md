# cpp-ai-chat-sdk 使用手册

ChatSDK 是一款基于 C++ 实现的大模型接入库。目前支持以下功能。

- 接入 DeepSeek 的 `deepseek-flash`、ChatGPT 的 `gpt-6-sol` 和 Qwen 的 `qwen3.8-flash`。
- 通过 Ollama 接入本地模型，示例使用 `qwen2.5:7b`。
- 在同一会话中进行多轮聊天，可一次性获取完整回复，也可流式接收。
- 管理会话，支持查看历史会话、读取历史消息和删除会话。
- 使用 SQLite 持久化会话与消息，应用重启后可恢复对话。

[工程目录](#工程目录) · [构建与安装](#构建与安装) · [使用说明](#使用说明) · [快速上手](#快速上手)

## 工程目录

~~~text
cpp-ai-chat-sdk/
├── CMakeLists.txt                  # 构建静态库、示例和测试
├── cmake/
│   └── chat_sdkConfig.cmake.in     # 导出的 CMake 包配置
├── sdk/
│   ├── include/
│   │   ├── ChatSDK.h               # 对应用提供的聊天和会话接口
│   │   ├── LLMManager.h            # 模型注册、初始化和调用
│   │   ├── ILLMProvider.h          # 模型 Provider 接口
│   │   ├── SessionManager.h        # 会话与历史消息管理
│   │   ├── DataManager.h           # SQLite 数据访问
│   │   ├── Log.h                   # SDK 日志
│   │   └── JsonUtil.h              # JSON 序列化与反序列化
│   └── src/                        # SDK、各模型 Provider 及封装的实现
└── examples/                       # 联调示例和测试
~~~

## 构建与安装

在 Linux 上使用 CMake 3.16 或以上版本和支持 C++17 的编译器。日志和 JSON 封装随 SDK 编译，CMake 直接链接 spdlog、JsonCpp、SQLite3 和 OpenSSL。Ubuntu 开发环境可先安装以下依赖。

~~~bash
sudo apt install build-essential cmake libspdlog-dev libjsoncpp-dev libsqlite3-dev libssl-dev
~~~

构建 SDK 源码还需要 header-only 版的 cpp-httplib。如果 `httplib.h` 位于自定义目录，在首次配置时传入 `-DCHAT_SDK_HTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib`，该目录需直接包含此头文件。

~~~bash
git clone https://github.com/fw-QAQ/cpp-ai-chat-sdk.git
cd cpp-ai-chat-sdk
export SDK_PREFIX="$PWD/install"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
(cd build && ctest --output-on-failure)
cmake --install build --prefix "$SDK_PREFIX"
~~~

安装生成静态库 `libai_chat_sdk.a`、`ai_chat_sdk/` 头文件和 `chat_sdkConfig.cmake`。只构建库时，在配置命令中加入 `-DCHAT_SDK_BUILD_TESTS=OFF`。

## 使用说明

### 类关系

~~~mermaid
classDiagram
    direction TB
    ChatSDK *-- LLMManager
    ChatSDK *-- SessionManager
    ChatSDK o-- Config
    Config <|-- ApiConfig
    Config <|-- OllamaConfig

    LLMManager *-- ModelInfo
    LLMManager *-- ILLMProvider
    ILLMProvider <|-- DeepSeekProvider
    ILLMProvider <|-- ChatGPTProvider
    ILLMProvider <|-- QwenProvider
    ILLMProvider <|-- OllamaProvider
    ILLMProvider ..> Message

    SessionManager *-- DataManager
    SessionManager o-- Session
    Session *-- Message
    DataManager ..> Session
    DataManager ..> Message
~~~

应用通过 `ChatSDK` 创建会话和发送消息。`LLMManager` 把请求交给对应的 Provider。`SessionManager` 管理当前会话，并通过 `DataManager` 将会话与消息读写到 SQLite。

### ChatSDK 使用介绍

应用包含 `<ai_chat_sdk/ChatSDK.h>`，并链接 `chat_sdk::chat_sdk`。日志默认输出到终端。需要调整级别、输出文件或开启异步日志时，包含 `<ai_chat_sdk/Log.h>`，填写 `ai_chat_sdk::log::Settings` 后调用 `ai_chat_sdk::log::initialize(settings)`。

公开接口定义在 [ChatSDK.h](sdk/include/ChatSDK.h)，配置、会话和消息类型定义在 [common.h](sdk/include/common.h)。

| API | 行为 |
| --- | --- |
| `ChatSDK(dbName)` | 打开或创建 SQLite 数据库，默认使用工作目录下的 `chatDB.db`。传入 `":memory:"` 可创建仅在当前实例存活期间有效的数据库 |
| `initModels(configs)` | 至少一个模型初始化成功时返回 `true`。初始化成功后再次调用返回 `false` |
| `getAvailableModels()` | 列出完成 Provider 初始化的模型。本地初始化成功后仍需用实际请求验证服务可达性、凭据及模型权限 |
| `createSession(modelName)` | 为选定模型创建会话，返回会话 ID，失败返回空字符串 |
| `getSessionList()` | 返回当前数据库中的会话 ID 列表 |
| `getSession(sessionId)` | 返回对应的 `Session` 指针。可以通过 `messages` 读取历史，找不到会话时返回空指针 |
| `deleteSession(sessionId)` | 删除会话及其关联的消息，返回是否成功 |
| `sendMessage(sessionId, message)` | 同步返回完整回复，失败时返回空字符串 |
| `sendMessageStream(sessionId, message, callback)` | 返回类型为 `void`。数据片段通过 `callback(chunk, false)` 交付，完整助手回复保存成功后再调用 `callback("", true)` |

`Session` 记录会话 ID、绑定的模型和历史消息。`Message` 包含消息 ID、角色、正文和时间戳。调用聊天接口时，已有消息会作为上下文交给相应的 Provider。重新打开同一个数据库可读取旧会话，继续对话时仍需重新初始化会话绑定的模型。

流式请求可能在输出部分内容后失败。若调用返回时仍未收到 `done = true`，应按失败处理。发送前 SDK 会保存用户消息，重试前可先读取会话确认当前历史。

### 模型配置

`Config` 的公共字段为 `model_name`、`temperature` 和 `max_tokens`，后两项默认值分别是 `0.7` 和 `2048`。远端模型使用 `ApiConfig`，Ollama 使用 `OllamaConfig`。

| Provider | 当前实现识别的 `model_name` | 请求路径 | 配置 |
| --- | --- | --- | --- |
| DeepSeek | `deepseek-flash` | `/chat/completions` | `ApiConfig`，提供 `api_key`，可指定 `endpoint` |
| ChatGPT | `gpt-6-sol` | `/v1/responses` | `ApiConfig`，提供 `api_key` 和适配该模型的 `endpoint` |
| Qwen | `qwen3.8-flash` | `/compatible-mode/v1/chat/completions` | `ApiConfig`，提供 `api_key`。当前 `ChatSDK` 封装下使用 Provider 默认地址 |
| Ollama | Ollama 已安装的模型标签，例如 `qwen2.5:7b` | `/api/chat` | `OllamaConfig`，提供 `endpoint` 和 `model_desc` |

表中的远端模型名是本项目的 Provider 映射。接入时核对服务是否支持对应模型名和请求路径，并使用该服务签发的密钥。`ApiConfig` 接收应用传入的密钥，环境变量由应用读取。仓库中的多模型示例使用 `DEEPSEEK_API_KEY`、`OPENAI_API_KEY` 和 `QWEN_API_KEY`。

例如，用以下代码替换快速上手中 Ollama 的配置和初始化部分，接入支持 `deepseek-flash` 的端点。需要同时使用多个模型时，把配置一并传入一次 `initModels`。

~~~cpp
auto config = std::make_shared<ai_chat_sdk::ApiConfig>();
config->model_name = "deepseek-flash";

const char* key = std::getenv("DEEPSEEK_API_KEY");
const char* endpoint = std::getenv("DEEPSEEK_ENDPOINT");
if (!key || !*key || !endpoint || !*endpoint) {
    std::cerr << "请设置 DEEPSEEK_API_KEY 和 DEEPSEEK_ENDPOINT\n";
    return 1;
}
config->api_key = key;
config->endpoint = endpoint;
if (!sdk.initModels({config})) {
    return 1;
}
~~~

需要直接指定 Qwen Provider 的地址时，可参考仓库中的 [QwenProvider 示例](examples/qwen_provider_test.cc)，它在 Provider 层使用 `base_url`。

### 测试与排查

| 范围 | 运行方式 |
| --- | --- |
| 会话、数据和历史恢复 | 在 SDK 目录执行 `(cd build && ctest --output-on-failure)` |
| Ollama 完整联调 | 在 SDK 目录设置 `OLLAMA_ENDPOINT`，执行 `./build/chat_sdk_test`。覆盖普通回复、流式回复、历史恢复和会话删除 |
| 云端多模型 | 查看 [多模型联调示例](examples/chat_sdk_multi_model_test.cc)，按其配置准备模型端点和对应密钥后执行 `./build/chat_sdk_multi_model_test` |

- **CMake 找不到包**。确认 SDK 已执行安装，且 `CMAKE_PREFIX_PATH` 指向包含 `chat_sdkConfig.cmake` 的安装前缀。
- **找不到 `httplib.h`**。确认使用 header-only 版本，再将头文件所在目录传给 `CHAT_SDK_HTTPLIB_INCLUDE_DIR`。
- **会话创建失败或回复为空**。先看日志并确认 `model_name` 与配置一致。Ollama 可从程序运行环境请求 `/api/tags`。远端模型检查服务地址、路径与密钥。
- **重启后找不到历史**。检查程序是否仍在使用同一个数据库文件。相对路径以进程的工作目录为准，长期使用时可指定固定的绝对路径。

## 快速上手

### 1. 准备 Ollama 模型

在运行 Ollama 的机器上安装示例模型。SDK 与 Ollama 在同一网络环境时可用回环地址。从虚拟机或容器访问宿主机时，把 `OLLAMA_ENDPOINT` 改成开发环境能访问的服务地址。

~~~bash
# 在运行 Ollama 的机器上执行
ollama pull qwen2.5:7b

# 在运行 C++ 程序的环境中执行
export OLLAMA_ENDPOINT=http://127.0.0.1:11434
curl -fsS "$OLLAMA_ENDPOINT/api/tags"
~~~

确认模型列表中有 `qwen2.5:7b`。`endpoint` 填服务根地址，SDK 会请求 `/api/chat`。

### 2. 编写客户端

在单独的目录中创建 `main.cc`。示例先初始化日志和模型，再在同一会话里发起一次普通请求和一次流式请求。传入上次打印的会话 ID 可以接着聊。

~~~cpp
#include <ai_chat_sdk/ChatSDK.h>
#include <ai_chat_sdk/Log.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    try {
        ai_chat_sdk::log::initialize();

        ai_chat_sdk::ChatSDK sdk("chat-demo.db");
        auto config = std::make_shared<ai_chat_sdk::OllamaConfig>();
        const char* endpoint = std::getenv("OLLAMA_ENDPOINT");
        config->endpoint = (endpoint && *endpoint) ? endpoint : "http://127.0.0.1:11434";
        config->model_name = "qwen2.5:7b";
        config->model_desc = "本地 Qwen 2.5 7B";
        config->temperature = 0.3;
        config->max_tokens = 128;

        if (!sdk.initModels({config})) {
            std::cerr << "模型初始化失败\n";
            return 1;
        }

        const std::string sessionId = argc > 1
            ? argv[1]
            : sdk.createSession(config->model_name);
        if (sessionId.empty() || !sdk.getSession(sessionId)) {
            std::cerr << "会话创建失败或会话 ID 无效\n";
            return 1;
        }
        std::cout << "会话 ID: " << sessionId << '\n';

        const auto reply = sdk.sendMessage(sessionId, "用一句话解释 C++ 的 RAII。");
        if (reply.empty()) {
            std::cerr << "普通请求失败\n";
            return 1;
        }
        std::cout << "普通回复: " << reply << "\n流式回复: ";

        bool completed = false;
        sdk.sendMessageStream(sessionId, "给出一个简短的 C++ 例子。",
            [&](const std::string& chunk, bool done) {
                std::cout << chunk << std::flush;
                if (done) completed = true;
            });
        std::cout << '\n';
        if (!completed) {
            std::cerr << "流式回复未完成\n";
            return 1;
        }

        if (const auto session = sdk.getSession(sessionId)) {
            std::cout << "已保存消息: " << session->messages.size() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
~~~

### 3. 编译并运行客户端

在同一目录创建 `CMakeLists.txt`。CMake 从安装前缀查找 SDK，链接目标会带上头文件目录和依赖。

~~~cmake
cmake_minimum_required(VERSION 3.16)
project(chat_demo LANGUAGES CXX)

find_package(chat_sdk CONFIG REQUIRED)
add_executable(chat_demo main.cc)
target_link_libraries(chat_demo PRIVATE chat_sdk::chat_sdk)
~~~

~~~bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$SDK_PREFIX"
cmake --build build
./build/chat_demo

# 将会话 ID 替换为上一次运行时打印的值
./build/chat_demo 'session_这里替换为实际ID'
~~~

第一次成功运行会写入两轮对话。保持工作目录和数据库路径一致，第二次运行可通过相同 ID 读取历史并继续追问。故障时可先按 [排查表](#测试与排查) 检查服务地址与数据库路径。
