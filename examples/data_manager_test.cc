#include "DataManager.h"

#include "Log.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

using namespace ai_chat_sdk;

// ========== 断言辅助函数 ==========

void check(bool condition, const std::string& name)
{
    if (!condition) {
        throw std::runtime_error(name + " failed");
    }

    CHAT_SDK_INFO("{} passed", name);
}


// ========== 创建测试会话 ==========

Session makeSession(
    const std::string& id,
    const std::string& model
) {
    Session session(model);

    session.id = id;

    return session;
}


// ========== 创建测试消息 ==========

Message makeMessage(
    const std::string& id,
    const std::string& role,
    const std::string& content,
    std::time_t timestamp
) {
    Message message(role, content);

    message.id = id;
    message.timestamp = timestamp;

    return message;
}


// ========== 执行测试专用 SQL ==========

bool executeTestSQL(
    const std::string& dbName,
    const std::string& sql
) {
    sqlite3* db = nullptr;

    if (sqlite3_open(dbName.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);

        return false;
    }

    char* errMsg = nullptr;

    int rc = sqlite3_exec(
        db,
        sql.c_str(),
        nullptr,
        nullptr,
        &errMsg
    );

    if (rc != SQLITE_OK) {
        CHAT_SDK_ERROR("Test SQL failed: {}",
            errMsg ? errMsg : sqlite3_errmsg(db));
    }

    sqlite3_free(errMsg);

    sqlite3_close(db);

    return rc == SQLITE_OK;
}


int main()
{
    // ========== 1. 初始化日志 ==========

    ai_chat_sdk::log::Settings settings;

    settings.async = false;

    ai_chat_sdk::log::initialize(settings);


    // ========== 2. 创建临时数据库文件 ==========

    char path[] = "/tmp/chat_sdk_data_test_XXXXXX";

    int fd = mkstemp(path);

    if (fd == -1) {
        CHAT_SDK_ERROR("Failed to create temporary database file");

        return 1;
    }

    close(fd);

    std::string dbName = path;

    std::time_t now = std::time(nullptr);


    try {

        // ========================================
        // 第一阶段：数据库 CRUD 测试
        // ========================================

        {
            DataManager manager(dbName);

            check(
                manager.getSessionCount() == 0,
                "Initial session count"
            );


            // ---------- 插入会话 ----------

            Session session1 = makeSession(
                "session_001",
                "qwen3.8-flash"
            );

            Session session2 = makeSession(
                "session_002",
                "qwen2.5:7b"
            );

            check(
                manager.insertSession(session1) &&
                manager.insertSession(session2),
                "Insert sessions"
            );

            check(
                manager.getSessionCount() == 2,
                "Session count"
            );


            // ---------- 插入消息 ----------

            Message msg1 = makeMessage(
                "msg_001",
                "user",
                "什么是 RAII？",
                now + 1
            );

            Message msg2 = makeMessage(
                "msg_002",
                "assistant",
                "RAII 是一种资源生命周期管理机制。",
                now + 2
            );

            Message msg3 = makeMessage(
                "msg_003",
                "user",
                "什么是 epoll？",
                now + 3
            );

            check(
                manager.insertMessage(
                    session1.id, msg1
                ) &&

                manager.insertMessage(
                    session1.id, msg2
                ) &&

                manager.insertMessage(
                    session2.id, msg3
                ),
                "Insert messages"
            );


            // ---------- 获取完整会话 ----------

            auto session =
                manager.getSession(session1.id);

            check(
                session != nullptr &&
                session->id == session1.id &&
                session->model_name == session1.model_name &&
                session->created_at == session1.created_at &&
                session->updated_at == msg2.timestamp,
                "Restore session information"
            );

            check(
                session->messages.size() == 2 &&
                session->messages[0].id == msg1.id &&
                session->messages[1].id == msg2.id,
                "Restore session messages"
            );


            // ---------- 验证历史消息内容 ----------

            auto history = manager.getMessagesBySessionId(
                session1.id
            );

            check(
                history.size() == 2 &&
                history[0].role == msg1.role &&
                history[0].content == msg1.content &&
                history[0].timestamp == msg1.timestamp &&
                history[1].content == msg2.content,
                "Message content and timestamp"
            );


            // ---------- 会话列表排序 ----------

            auto ids = manager.getAllSessionIds();

            check(
                ids.size() == 2 &&
                ids[0] == session2.id &&
                ids[1] == session1.id,
                "Session sorting"
            );


            // ---------- 延迟加载 ----------

            auto sessions = manager.getAllSessions();

            check(
                sessions.size() == 2 &&
                sessions[0]->messages.empty() &&
                sessions[1]->messages.empty(),
                "Lazy loading"
            );


            // ---------- 不存在的会话 ----------

            check(
                manager.getSession("unknown") == nullptr &&
                !manager.deleteSession("unknown"),
                "Unknown session handling"
            );


            // ---------- 重复消息 ID ----------

            check(
                !manager.insertMessage(session1.id, msg1) &&
                manager.getMessagesBySessionId(
                    session1.id
                ).size() == 2,
                "Duplicate message rejection"
            );


            // ---------- 外键约束 ----------

            Message orphan = makeMessage(
                "msg_orphan",
                "user",
                "Invalid session",
                now + 4
            );

            check(
                !manager.insertMessage("unknown", orphan) &&
                manager.getMessagesBySessionId(
                    "unknown"
                ).empty(),
                "Foreign key constraint"
            );


            // ====================================
            // 第二阶段：事务回滚测试
            // ====================================

            // 创建一个测试触发器：
            // 强制下一次会话时间更新失败
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

            Message rollbackMessage = makeMessage(
                "msg_rollback",
                "user",
                "This message must be rolled back",
                now + 5
            );

            // INSERT 本身可以成功，
            // 但后续 UPDATE 会被触发器阻止
            check(
                !manager.insertMessage(
                    session1.id,
                    rollbackMessage
                ),
                "Transaction failure handling"
            );

            auto afterFailure =
                manager.getMessagesBySessionId(session1.id);

            check(
                afterFailure.size() == 2,
                "Transaction rollback"
            );

            // 删除测试触发器
            check(
                executeTestSQL(
                    dbName,
                    "DROP TRIGGER test_update_failure;"
                ),
                "Remove failure trigger"
            );

            // 验证失败后数据库仍然可以正常写入
            manager.updateSessionTimestamp(
                session1.id,
                now + 10
            );

            auto updated = manager.getSession(session1.id);

            check(
                updated != nullptr &&
                updated->updated_at == now + 10,
                "Update session timestamp"
            );

        } // DataManager 析构，数据库连接关闭


        // ========================================
        // 第三阶段：重新打开数据库
        // ========================================

        {
            DataManager manager(dbName);

            check(
                manager.getSessionCount() == 2,
                "Persistent session recovery"
            );

            auto session =
                manager.getSession("session_001");

            check(
                session != nullptr &&
                session->messages.size() == 2 &&
                session->updated_at == now + 10,
                "Persistent message recovery"
            );


            // ---------- 输出恢复的历史消息 ----------

            std::cout
                << "\n========== Restored Session ==========\n";

            std::cout
                << "Session ID: " << session->id << '\n';

            std::cout
                << "Model: " << session->model_name << '\n';

            for (const auto& message : session->messages) {
                std::cout
                    << "[" << message.role << "] "
                    << message.content << '\n';
            }


            // ---------- 单独删除消息 ----------

            check(
                manager.deleteMessagesBySessionId(
                    "session_001"
                ) &&

                manager.getMessagesBySessionId(
                    "session_001"
                ).empty() &&

                manager.getSessionCount() == 2,

                "Delete session messages"
            );


            // ---------- 级联删除 ----------

            check(
                manager.deleteSession("session_002") &&

                manager.getSession(
                    "session_002"
                ) == nullptr &&

                manager.getMessagesBySessionId(
                    "session_002"
                ).empty(),

                "Cascade deletion"
            );


            // ---------- 清空数据库 ----------

            Message finalMessage = makeMessage(
                "msg_final",
                "user",
                "Clear database test",
                now + 11
            );

            check(
                manager.insertMessage(
                    "session_001",
                    finalMessage
                ),
                "Insert before clearing"
            );

            check(
                manager.clearAllSessions() &&

                manager.getSessionCount() == 0 &&

                manager.getAllSessionIds().empty() &&

                manager.getMessagesBySessionId(
                    "session_001"
                ).empty(),

                "Clear all sessions"
            );
        }

        // 删除临时测试数据库
        std::remove(dbName.c_str());

        CHAT_SDK_INFO("All DataManager tests passed!");

    } catch (const std::exception& e) {

        CHAT_SDK_ERROR("DataManager test failed: {}", e.what());

        // 保留失败数据库，方便排查
        CHAT_SDK_ERROR("Test database: {}", dbName);

        return 1;
    }

    return 0;
}
