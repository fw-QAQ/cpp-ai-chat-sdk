#include "Log.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>

namespace ai_chat_sdk::log {
namespace {

std::shared_ptr<spdlog::logger> make_logger(const Settings& settings) {
    spdlog::sink_ptr sink;
    if (settings.path == "stdout") {
        sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    } else {
        sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(settings.path);
    }

    std::shared_ptr<spdlog::logger> result;
    if (settings.async) {
        static const auto pool = std::make_shared<spdlog::details::thread_pool>(8192, 1);
        result = std::make_shared<spdlog::async_logger>(
            "cpp-ai-chat-sdk", sink, pool, spdlog::async_overflow_policy::block);
    } else {
        result = std::make_shared<spdlog::logger>("cpp-ai-chat-sdk", sink);
    }

    result->set_level(settings.level);
    result->set_pattern(settings.format);
    return result;
}

std::shared_ptr<spdlog::logger>& configured_logger() {
    static auto current = make_logger(Settings{});
    return current;
}

} // namespace

void initialize(const Settings& settings) {
    std::atomic_store(&configured_logger(), make_logger(settings));
}

std::shared_ptr<spdlog::logger> logger() {
    return std::atomic_load(&configured_logger());
}

} // namespace ai_chat_sdk::log
