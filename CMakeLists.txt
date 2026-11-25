# ========================================
# 1. 项目基本配置
# ========================================

cmake_minimum_required(VERSION 3.16)

project(cpp_ai_chat_sdk
    VERSION 1.0.0
    LANGUAGES CXX
)

# 统一管理标准安装目录
include(GNUInstallDirs)

# 提供软件包配置文件生成工具
include(CMakePackageConfigHelpers)


# ========================================
# 2. 构建选项
# ========================================

option(
    CHAT_SDK_BUILD_TESTS
    "Build ChatSDK test programs"
    ON
)


# ========================================
# 3. 查找第三方依赖
# ========================================

find_package(OpenSSL REQUIRED)

find_package(Threads REQUIRED)

find_package(SQLite3 REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

find_path(CHAT_SDK_HTTPLIB_INCLUDE_DIR NAMES httplib.h)
if(NOT CHAT_SDK_HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR
        "Header-only cpp-httplib is required. Set CHAT_SDK_HTTPLIB_INCLUDE_DIR to the directory containing httplib.h.")
endif()


# ========================================
# 4. 收集 SDK 源文件
# ========================================

set(SDK_SOURCES
    sdk/src/DeepSeekProvider.cc
    sdk/src/ChatGPTProvider.cc
    sdk/src/QwenProvider.cc
    sdk/src/OllamaProvider.cc
    sdk/src/LLMManager.cc
    sdk/src/SessionManager.cc
    sdk/src/DataManager.cc
    sdk/src/ChatSDK.cc
    sdk/src/JsonUtil.cc
    sdk/src/Log.cc
)


# ========================================
# 5. 创建静态库目标
# ========================================

add_library(chat_sdk STATIC
    ${SDK_SOURCES}
)

# 为构建阶段提供统一的命名空间目标
add_library(chat_sdk::chat_sdk ALIAS chat_sdk)


# ========================================
# 6. 设置目标属性
# ========================================

set_target_properties(chat_sdk PROPERTIES

    # 实际生成 libai_chat_sdk.a
    OUTPUT_NAME ai_chat_sdk

    # 禁用非标准 C++ 扩展
    CXX_EXTENSIONS OFF

    # 支持将来链接进共享库
    POSITION_INDEPENDENT_CODE ON
)


# ========================================
# 7. 设置 C++ 标准
# ========================================

target_compile_features(chat_sdk PUBLIC
    cxx_std_17
)


# ========================================
# 8. 设置头文件搜索路径
# ========================================

target_include_directories(chat_sdk
    PUBLIC

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/include>

    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>

    PRIVATE
        ${CHAT_SDK_HTTPLIB_INCLUDE_DIR}
)


# ========================================
# 9. 设置预处理宏
# ========================================

target_compile_definitions(chat_sdk PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)


# ========================================
# 10. 链接第三方库
# ========================================

target_link_libraries(chat_sdk

    PUBLIC
        SQLite::SQLite3
        JsonCpp::JsonCpp
        spdlog::spdlog

    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)


# ========================================
# 11. 构建测试程序
# ========================================

if(CHAT_SDK_BUILD_TESTS)

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
            chat_sdk::chat_sdk
        )

    endforeach()

    enable_testing()

    # 只自动注册不需要调用真实大模型的测试
    add_test(
        NAME data_manager_test
        COMMAND data_manager_test
    )

    add_test(
        NAME session_manager_test
        COMMAND session_manager_test
    )

    add_test(
        NAME session_persistence_test
        COMMAND session_persistence_test
    )

endif()


# ========================================
# 12. 安装静态库
# ========================================

install(
    TARGETS chat_sdk

    EXPORT chat_sdkTargets

    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)


# ========================================
# 13. 安装头文件
# ========================================

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/

    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ai_chat_sdk

    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)


# ========================================
# 14. 导出目标信息
# ========================================

set(CHAT_SDK_CMAKE_INSTALL_DIR
    ${CMAKE_INSTALL_LIBDIR}/cmake/chat_sdk
)

install(
    EXPORT chat_sdkTargets

    FILE chat_sdkTargets.cmake

    NAMESPACE chat_sdk::

    DESTINATION ${CHAT_SDK_CMAKE_INSTALL_DIR}
)


# ========================================
# 15. 生成软件包配置文件
# ========================================

configure_package_config_file(

    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/chat_sdkConfig.cmake.in

    ${CMAKE_CURRENT_BINARY_DIR}/chat_sdkConfig.cmake

    INSTALL_DESTINATION
        ${CHAT_SDK_CMAKE_INSTALL_DIR}
)


# ========================================
# 16. 生成软件包版本文件
# ========================================

write_basic_package_version_file(

    ${CMAKE_CURRENT_BINARY_DIR}/chat_sdkConfigVersion.cmake

    VERSION ${PROJECT_VERSION}

    COMPATIBILITY SameMajorVersion
)


# ========================================
# 17. 安装软件包配置文件
# ========================================

install(
    FILES

        ${CMAKE_CURRENT_BINARY_DIR}/chat_sdkConfig.cmake

        ${CMAKE_CURRENT_BINARY_DIR}/chat_sdkConfigVersion.cmake

    DESTINATION
        ${CHAT_SDK_CMAKE_INSTALL_DIR}
)
