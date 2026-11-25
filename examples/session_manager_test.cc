#include "SessionManager.h"

#include <bite_scaffold/log.h>

#include <iostream>
#include <set>
#include <string>

using namespace ai_chat_sdk;

// 测试断言辅助函数
bool check(bool condition, const std::string& name)
{
    if (!condition) {
        ERR("{} failed", name);
        return false;
    }

    INF("{} passed", name);

    return true;
}

int main()
{
    // ========== 1. 初始化日志 ==========

    bitelog::log_settings settings;
    settings.async = false;

    bitelog::bitelog_init(settings);


    // ========== 2. 创建会话管理器 ==========

    SessionManager manager;

    if (!check(
            manager.getSessionCount() == 0,
            "Initial session count"
        )) {
        return 1;
    }


    // ========== 3. 创建两个会话 ==========

    std::string session1 =
        manager.createSession("qwen3.8-flash");

    std::string session2 =
        manager.createSession("qwen2.5:7b");

    if (!check(
            !session1.empty() &&
            !session2.empty() &&
            session1 != session2 &&
            manager.getSessionCount() == 2,
            "Create sessions"
        )) {
        return 1;
    }


    // ========== 4. 获取指定会话 ==========

    auto session = manager.getSession(session1);

    if (!check(
            session != nullptr &&
            session->id == session1 &&
            session->model_name == "qwen3.8-flash",
            "Get session"
        )) {
        return 1;
    }


    // ========== 5. 添加聊天消息 ==========

    if (!manager.addMessage(
            session1,
            Message("user", "什么是 RAII？")
        )) {
        return 1;
    }

    if (!manager.addMessage(
            session1,
            Message(
                "assistant",
                "RAII 是通过对象生命周期管理资源的机制。"
            )
        )) {
        return 1;
    }

    if (!manager.addMessage(
            session1,
            Message("user", "它与智能指针有什么关系？")
        )) {
        return 1;
    }

    if (!manager.addMessage(
            session2,
            Message("user", "什么是 epoll？")
        )) {
        return 1;
    }

    INF("Add messages passed");


    // ========== 6. 获取历史消息 ==========

    auto history1 =
        manager.getSessionHistory(session1);

    auto history2 =
        manager.getSessionHistory(session2);

    if (!check(
            history1.size() == 3 &&
            history2.size() == 1,
            "Session history isolation"
        )) {
        return 1;
    }

    if (!check(
            history1[0].role == "user" &&
            history1[1].role == "assistant" &&
            history1[2].role == "user",
            "Message order"
        )) {
        return 1;
    }


    // ========== 7. 检查消息 ID 唯一性 ==========

    std::set<std::string> message_ids;

    for (const auto& message : history1) {
        if (message.id.empty()) {
            ERR("Empty message ID");
            return 1;
        }

        message_ids.insert(message.id);
    }

    for (const auto& message : history2) {
        if (message.id.empty()) {
            ERR("Empty message ID");
            return 1;
        }

        message_ids.insert(message.id);
    }

    if (!check(
            message_ids.size() == 4,
            "Unique message IDs"
        )) {
        return 1;
    }


    // ========== 8. 显示会话历史 ==========

    std::cout << "\n========== Session History ==========\n";

    std::cout << "Session ID: " << session1 << '\n';

    for (const auto& message : history1) {

        std::cout << "[" << message.role << "] "
                  << message.content << '\n';

        std::cout << "Message ID: "
                  << message.id << '\n';

        std::cout << "Timestamp: "
                  << message.timestamp << '\n';

        std::cout << '\n';
    }


    // ========== 9. 获取会话列表 ==========

    auto session_ids = manager.getSessionList();

    if (!check(
            session_ids.size() == 2,
            "Session list"
        )) {
        return 1;
    }

    std::cout << "\n========== Session List ==========\n";

    for (size_t i = 0; i < session_ids.size(); ++i) {

        auto current =
            manager.getSession(session_ids[i]);

        if (!current) {
            ERR("Invalid session in session list");
            return 1;
        }

        // 验证会话更新时间按照降序排列
        if (i > 0) {
            auto previous =
                manager.getSession(session_ids[i - 1]);

            // 验证会话按照最近更新时间降序排列
if (previous->updated_at <
    current->updated_at) {

    ERR("Session sorting failed");
    return 1;
}
        }

        std::cout << "Session ID: "
                  << current->id << '\n';

        std::cout << "Model: "
                  << current->model_name << '\n';

        std::cout << "Messages: "
                  << current->messages.size() << '\n';

        std::cout << "Update time: "
                  << current->updated_at << '\n';

        std::cout << "----------------------------------\n";
    }

    INF("Session sorting passed");


    // ========== 10. 测试不存在的会话 ==========

    if (!check(
            manager.getSession("unknown") == nullptr &&
            manager.getSessionHistory("unknown").empty() &&
            !manager.addMessage(
                "unknown",
                Message("user", "test")
            ) &&
            !manager.deleteSession("unknown"),
            "Unknown session handling"
        )) {
        return 1;
    }


    // ========== 11. 删除会话 ==========

    if (!check(
            manager.deleteSession(session1) &&
            manager.getSessionCount() == 1 &&
            manager.getSession(session1) == nullptr,
            "Delete session"
        )) {
        return 1;
    }


    // ========== 12. 清空所有会话 ==========

    manager.clearAllSessions();

    if (!check(
            manager.getSessionCount() == 0 &&
            manager.getSessionList().empty(),
            "Clear all sessions"
        )) {
        return 1;
    }

    INF("All SessionManager tests passed!");

    return 0;
}
