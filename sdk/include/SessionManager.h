#pragma once

#include "common.h"
#include "DataManager.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace ai_chat_sdk {

class SessionManager {
public:
    // 构造函数：初始化数据库并恢复历史会话
    explicit SessionManager(
        const std::string& dbName = "chatDB.db"
    );

    // 创建会话
    std::string createSession(
        const std::string& model_name
    );

    // 获取完整会话
    std::shared_ptr<Session> getSession(
        const std::string& session_id
    );

    // 添加消息
    bool addMessage(
        const std::string& session_id,
        const Message& message
    );

    // 获取历史消息
    std::vector<Message> getSessionHistory(
        const std::string& session_id
    );

    // 获取会话列表
    std::vector<std::string> getSessionList() const;

    // 删除会话
    bool deleteSession(
        const std::string& session_id
    );

    // 更新会话时间戳
    void updateSessionTimestamp(
        const std::string& session_id
    );

    // 清空全部会话
    void clearAllSessions();

    // 获取会话数量
    size_t getSessionCount() const;

private:

    // 生成会话 ID
    std::string generateSessionId();

    // 生成消息 ID
    std::string generateMessageId();

    // 加载完整会话，调用者必须持有 _mutex
    std::shared_ptr<Session> loadSessionUnlocked(
        const std::string& session_id
    );

private:

    // 数据库管理对象
    DataManager _dataManager;

    // 内存中的会话缓存
    std::unordered_map<
        std::string,
        std::shared_ptr<Session>
    > _sessions;

    // 记录已经加载完整历史消息的会话
    std::unordered_set<std::string> _loadedSessions;

    // 保护会话数据
    mutable std::mutex _mutex;

    // ID 原子计数器
    static std::atomic<int64_t> _message_counter;

    static std::atomic<int64_t> _session_counter;
};

} // namespace ai_chat_sdk
