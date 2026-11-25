cmake_minimum_required(VERSION 3.16)

project(cpp_ai_chat_sdk LANGUAGES CXX)


# ========== 第三方依赖 ==========

find_package(bite_scaffold REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)


# ========== SDK 静态库 ==========

add_library(chat_sdk STATIC
    sdk/src/DeepSeekProvider.cc
    sdk/src/ChatGPTProvider.cc
    sdk/src/QwenProvider.cc
    sdk/src/OllamaProvider.cc
    sdk/src/LLMManager.cc
    sdk/src/SessionManager.cc
    sdk/src/DataManager.cc
    sdk/src/ChatSDK.cc
)


# ========== C++ 标准 ==========

target_compile_features(chat_sdk PUBLIC cxx_std_17)


# ========== 头文件路径 ==========

target_include_directories(chat_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
)


# ========== HTTPS 支持 ==========

target_compile_definitions(chat_sdk PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)


# ========== 链接第三方库 ==========

target_link_libraries(chat_sdk
    PUBLIC
        SQLite::SQLite3

    PRIVATE
        bite_scaffold::bite_scaffold
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)


# ========== 测试程序 ==========

set(TEST_TARGETS
    chatgpt_provider_test
    qwen_provider_test
    ollama_provider_test
    llm_manager_test
    session_manager_test
    data_manager_test
    session_persistence_test
    chat_sdk_test
    chat_sdk_multi_model_test
)

foreach(target IN LISTS TEST_TARGETS)

    add_executable(${target}
        examples/${target}.cc
    )

    target_link_libraries(${target} PRIVATE
        chat_sdk
        bite_scaffold::bite_scaffold
    )

endforeach()
