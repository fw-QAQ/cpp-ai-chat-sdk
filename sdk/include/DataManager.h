#pragma once

#include "common.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>
#include <cstddef>
#include <sqlite3.h>

namespace ai_chat_sdk {

class DataManager {
public:
    // 打开数据库并初始化数据表
    explicit DataManager(const std::string& dbName);

    // 关闭数据库连接
    ~DataManager();

    // 禁止拷贝，避免重复释放数据库连接
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;

    // ========== Session 相关操作 ==========

    // 插入新会话
    bool insertSession(const Session& session);

    // 获取指定会话，包括历史消息
    std::shared_ptr<Session> getSession(
        const std::string& sessionId
    ) const;

    // 更新指定会话的时间戳
    bool updateSessionTimestamp(
        const std::string& sessionId,
        std::time_t timestamp
    );

    // 删除指定会话及其历史消息
    bool deleteSession(const std::string& sessionId);

    // 获取全部会话 ID，按更新时间降序排列
    std::vector<std::string> getAllSessionIds() const;

    // 获取全部会话信息，暂不加载历史消息
    std::vector<std::shared_ptr<Session>>
    getAllSessions() const;

    // 清空所有会话
    bool clearAllSessions();

    // 获取会话数量
    size_t getSessionCount() const;

    // ========== Message 相关操作 ==========

    // 向指定会话插入消息
    bool insertMessage(
        const std::string& sessionId,
        const Message& message
    );

    // 获取指定会话的历史消息
    std::vector<Message> getMessagesBySessionId(
        const std::string& sessionId
    ) const;

    // 删除指定会话的所有消息
    bool deleteMessagesBySessionId(
        const std::string& sessionId
    );

private:
    // ========== 数据库初始化 ==========

    bool initDatabase();

    bool executeSQL(const std::string& sql);

    // ========== RAII 资源管理 ==========

    using StatementPtr = std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;

    // 预编译 SQL
    StatementPtr prepare(const std::string& sql) const;

    // 执行非查询语句
    bool executeStatement(sqlite3_stmt* stmt) const;

    // 绑定字符串参数
    bool bindText(
        sqlite3_stmt* stmt,
        int index,
        const std::string& value
    ) const;

    // 读取文本类型的查询结果
    static std::string readText(
        sqlite3_stmt* stmt,
        int column
    );

    // ========== 内部无锁辅助函数 ==========

    // 调用者必须已经持有 _mutex
    bool readMessagesUnlocked(
        const std::string& sessionId,
        std::vector<Message>& messages
    ) const;

    bool insertMessageUnlocked(
        const std::string& sessionId,
        const Message& message
    );

    bool updateTimestampUnlocked(
        const std::string& sessionId,
        std::time_t timestamp
    );

private:
    sqlite3* _db = nullptr;

    std::string _dbName;

    mutable std::mutex _mutex;
};

} // namespace ai_chat_sdk
