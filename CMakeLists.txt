# 1. 指定 CMake 最低版本
cmake_minimum_required(VERSION 3.16)

# 2. 定义项目名称和开发语言
project(cpp_ai_chat_sdk LANGUAGES CXX)

# 3. 查找第三方依赖
# bite_scaffold：我们之前封装的微服务基础库
# OpenSSL：用于 cpp-httplib 的 HTTPS 通信
find_package(bite_scaffold REQUIRED)
find_package(OpenSSL REQUIRED)

# 4. 将所有 SDK 源文件编译成静态库 libchat_sdk.a
add_library(chat_sdk STATIC
    sdk/src/DeepSeekProvider.cc
    sdk/src/ChatGPTProvider.cc
    sdk/src/QwenProvider.cc
    sdk/src/OllamaProvider.cc
    sdk/src/LLMManager.cc
)

# 5. 指定 SDK 使用 C++17 标准
# PUBLIC 表示使用该库的其他目标也需要满足此要求
target_compile_features(chat_sdk PUBLIC cxx_std_17)

# 6. 设置 SDK 的头文件搜索路径
# PUBLIC 表示链接 chat_sdk 的程序也能搜索该目录
target_include_directories(chat_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
)

# 7. 定义宏，启用 cpp-httplib 的 HTTPS 支持
# PRIVATE 表示该编译宏仅作用于 chat_sdk 自身
target_compile_definitions(chat_sdk PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)

# 8. 为 SDK 链接微服务脚手架和 OpenSSL
# 复用脚手架中的日志、JSON 等公共功能
target_link_libraries(chat_sdk PRIVATE
    bite_scaffold::bite_scaffold
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 9. 定义所有需要构建的测试程序
set(TEST_TARGETS
    chatgpt_provider_test
    qwen_provider_test
    ollama_provider_test
    llm_manager_test
)

# 10. 遍历测试程序列表，统一进行编译和链接
foreach(target IN LISTS TEST_TARGETS)

    # 根据目标名称找到对应的 .cc 文件
    add_executable(${target}
        examples/${target}.cc
    )

    # 测试程序链接 ChatSDK 和微服务基础库
    target_link_libraries(${target} PRIVATE
        chat_sdk
        bite_scaffold::bite_scaffold
    )

endforeach()
