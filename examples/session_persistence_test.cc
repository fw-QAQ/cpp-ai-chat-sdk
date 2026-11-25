#include "SessionManager.h"

#include <bite_scaffold/log.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

using namespace ai_chat_sdk;


// 断言辅助函数
void check(bool condition, const std::string& name)
{
    if (!condition) {
        throw std::runtime_error(name + " failed");
    }

    INF("{} passed", name);
}


// 执行测试专用 SQL
bool executeTestSQL(
    const std::string& dbName,
    const std::string& sql
) {
    sqlite3* db = nullptr;

    if (sqlite3_open(dbName.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    char* error = nullptr;

    int rc = sqlite3_exec(
        db, sql.c_str(),
        nullptr, nullptr, &error
    );

    if (rc != SQLITE_OK) {
        ERR("Test SQL failed: {}",
            error ? error : sqlite3_errmsg(db));
    }

    sqlite3_free(error);
    sqlite3_close(db);

    return rc == SQLITE_OK;
}


int main()
{
    // ========== 1. 初始化日志 ==========

    bitelog::log_settings settings;

    settings.async = false;

    bitelog::bitelog_init(settings);


    // ========== 2. 创建临时数据库 ==========

    char path[] = "/tmp/chat_sdk_session_test_XXXXXX";

    int fd = mkstemp(path);

    if (fd == -1) {
        ERR("Failed to create temporary database");
        return 1;
    }

    close(fd);

    std::string dbName = path;

    std::string session1;
    std::string session2;

    std::vector<Message> originalHistory;

    try {

        // ====================================
        // 第一阶段：创建会话并保存历史消息
        // ====================================

        {
            SessionManager manager(dbName);

            check(
                manager.getSessionCount() == 0,
                "Initial session count"
            );

            session1 = manager.createSession("gpt-6-sol");

            session2 = manager.createSession("qwen2.5:7b");

            check(
                !session1.empty() &&
                !session2.empty() &&
                session1 != session2 &&
                manager.getSessionCount() == 2,
                "Persistent session creation"
            );

            check(
                manager.addMessage(
                    session1,
                    Message("user", "什么是 RAII？")
                ) &&

                manager.addMessage(
                    session1,
                    Message("assistant",
                            "RAII 是资源生命周期管理机制。")
                ) &&

                manager.addMessage(
                    session2,
                    Message("user", "什么是 epoll？")
                ),
                "Persistent message insertion"
            );

            originalHistory =
                manager.getSessionHistory(session1);

            check(
                originalHistory.size() == 2 &&
                manager.getSessionHistory(session2).size() == 1,
                "Initial message history"
            );


            // ====================================
            // 测试数据库写入失败后的内存一致性
            // ====================================

            // 强制数据库拒绝会话时间更新
            check(
                executeTestSQL(
                    dbName,
R"(
                        CREATE TRIGGER test_update_failure
                        BEFORE UPDATE ON sessions
                        BEGIN
                            SELECT RAISE(
                                ABORT,
                                'Forced update failure'
                            );
                        END;
                    )"
                ),
                "Create failure trigger"
            );

            auto before = manager.getSession(session1);

            check(before != nullptr, "Get cached session");

            std::time_t oldTimestamp = before->updated_at;

            // 添加消息时，数据库事务会失败
            check(
                !manager.addMessage(
                    session1,
                    Message("user", "This message must fail")
                ),
                "Reject failed database insertion"
            );

            // 验证内存没有加入失败的消息
            auto after = manager.getSession(session1);

            check(
                after != nullptr &&
                after->messages.size() == 2 &&
                after->updated_at == oldTimestamp,
                "Memory consistency after database failure"
            );

            check(
                executeTestSQL(
                    dbName,
                    "DROP TRIGGER test_update_failure;"
                ),
                "Remove failure trigger"
            );


            // 测试会话插入失败
            check(
                executeTestSQL(
                    dbName,
R"(
                        CREATE TRIGGER test_create_failure
                        BEFORE INSERT ON sessions
                        BEGIN
                            SELECT RAISE(
                                ABORT,
                                'Forced create failure'
                            );
                        END;
                    )"
                ),
                "Create session failure trigger"
            );

            check(
                manager.createSession("gpt-6-sol").empty() &&
                manager.getSessionCount() == 2 &&
                manager.getSessionList().size() == 2,
                "Reject failed session creation"
            );

            check(
                executeTestSQL(
                    dbName,
                    "DROP TRIGGER test_create_failure;"
                ),
                "Remove session failure trigger"
            );

        } // SessionManager 和 DataManager 析构


        // ====================================
        // 第二阶段：重新打开数据库，恢复会话
        // ====================================

        {
            SessionManager manager(dbName);

            check(
                manager.getSessionCount() == 2,
                "Restore persistent session count"
            );

            check(
                manager.getSessionList().size() == 2,
                "Restore persistent session list"
            );

            // 直接添加消息，验证能够加载原有历史
            check(
                manager.addMessage(
                    session1,
                    Message("user", "RAII 与智能指针有什么关系？")
                ),
                "Append message after restart"
            );

            auto history =
                manager.getSessionHistory(session1);

            // 应当包含之前的两条消息和本次新增消息
            check(
                history.size() == 3 &&
                history[0].id == originalHistory[0].id &&
                history[0].timestamp == originalHistory[0].timestamp &&
                history[1].id == originalHistory[1].id &&
                history[1].content == originalHistory[1].content &&
                history[2].id != history[0].id &&
                history[2].id != history[1].id &&
                history[2].content ==
                    "RAII 与智能指针有什么关系？",
                "Restore and append history"
            );

            auto session = manager.getSession(session1);

            check(
                session != nullptr &&
                session->model_name == "gpt-6-sol" &&
                session->updated_at == history.back().timestamp,
                "Restore complete session"
            );

            std::cout
                << "\n========== Restored Chat History ==========\n";

            for (const auto& message : history) {
                std::cout
                    << "[" << message.role << "] "
                    << message.content << '\n';
            }

            // 删除第二个会话
            check(
                manager.deleteSession(session2),
                "Delete persistent session"
            );

            check(
                manager.getSessionCount() == 1 &&
                manager.getSession(session2) == nullptr,
                "Delete memory cache"
            );

        }


        // ====================================
        // 第三阶段：验证删除操作已经持久化
        // ====================================

        {
            SessionManager manager(dbName);

            check(
                manager.getSessionCount() == 1 &&
                manager.getSession(session2) == nullptr &&
                manager.getSessionHistory(session2).empty(),
                "Persistent deletion"
            );

            check(
                manager.getSessionHistory(session1).size() == 3,
                "Persistent appended messages"
            );

            manager.clearAllSessions();

            check(
                manager.getSessionCount() == 0 &&
                manager.getSessionList().empty() &&
                manager.getSession(session1) == nullptr,
                "Clear persistent sessions"
            );
        }


        // ====================================
        // 第四阶段：验证清空结果已经持久化
        // ====================================

        {
            SessionManager manager(dbName);

            check(
                manager.getSessionCount() == 0 &&
                manager.getSessionList().empty(),
                "Verify empty database after restart"
            );
        }

        // 删除临时数据库
        std::remove(dbName.c_str());

        INF("All session persistence tests passed!");

    } catch (const std::exception& e) {

        ERR("Session persistence test failed: {}", e.what());

        ERR("Test database: {}", dbName);

        return 1;
    }

    return 0;
}
