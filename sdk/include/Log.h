#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace ai_chat_sdk::log {

struct Settings {
    bool async = false;
    spdlog::level::level_enum level = spdlog::level::debug;
    std::string format = "[%H:%M:%S][%-7l]: [%s:%#] %v";
    std::string path = "stdout";
};

// 配置 SDK 专用日志器，支持同步或异步输出到终端和文件。
void initialize(const Settings& settings = {});

// 提供默认日志器，应用可以直接创建 ChatSDK。
std::shared_ptr<spdlog::logger> logger();

} // namespace ai_chat_sdk::log

#define CHAT_SDK_DEBUG(...) SPDLOG_LOGGER_CALL(::ai_chat_sdk::log::logger(), ::spdlog::level::debug, __VA_ARGS__)
#define CHAT_SDK_INFO(...) SPDLOG_LOGGER_CALL(::ai_chat_sdk::log::logger(), ::spdlog::level::info, __VA_ARGS__)
#define CHAT_SDK_WARN(...) SPDLOG_LOGGER_CALL(::ai_chat_sdk::log::logger(), ::spdlog::level::warn, __VA_ARGS__)
#define CHAT_SDK_ERROR(...) SPDLOG_LOGGER_CALL(::ai_chat_sdk::log::logger(), ::spdlog::level::err, __VA_ARGS__)
