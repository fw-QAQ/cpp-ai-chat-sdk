# cpp-ai-chat-sdk

C++17 聊天 SDK，提供多模型调用、流式回复、会话管理和 SQLite 聊天记录持久化。

## 构建与安装

需要 CMake 3.16 或以上版本、C++17 编译器、`bite_scaffold`、OpenSSL 和 SQLite3。将 `CMAKE_PREFIX_PATH` 指向 `bite_scaffold` 的安装目录。

~~~bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/bite-scaffold-install
cmake --build build --parallel
(cd build && ctest --output-on-failure)
cmake --install build --prefix "$PWD/install"
~~~

`ctest` 运行会话管理、数据管理和持久化测试。`examples/` 中还包含 Ollama 及其他模型的联调示例。只构建库时，在配置命令中加入 `-DCHAT_SDK_BUILD_TESTS=OFF`。

## 在其他项目中使用

将 SDK 和 `bite_scaffold` 的安装前缀都加入消费项目的 `CMAKE_PREFIX_PATH`。在 `CMakeLists.txt` 中链接导出的目标。

~~~cmake
find_package(chat_sdk CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE chat_sdk::chat_sdk)
~~~

C++ 代码可以包含 `#include <ai_chat_sdk/ChatSDK.h>`。
