#include "DataManager.h"

#include <bite_scaffold/log.h>

#include <linux/stat.h>
#include <stdexcept>
#include <utility>

namespace ai_chat_sdk {

// ========== 1. 构造函数 ==========

DataManager::DataManager(const std::string& dbName)
    : _db(nullptr)
    , _dbName(dbName)
{
    // 打开或者创建数据库
    int rc = sqlite3_open(_dbName.c_str(), &_db);

    if (rc != SQLITE_OK) {
        std::string error = _db
            ? sqlite3_errmsg(_db)
            : "Failed to allocate database handle";

        sqlite3_close(_db);
        _db = nullptr;

        throw std::runtime_error(error);
    }

    // 数据库被其他连接占用时，最多等待 5 秒
    rc = sqlite3_busy_timeout(_db, 5000);

    if (rc != SQLITE_OK || !initDatabase()) {
        sqlite3_close(_db);
        _db = nullptr;

        throw std::runtime_error(
            "Failed to initialize database"
        );
    }

    INF("Database opened successfully: {}", _dbName);
}


// ========== 2. 析构函数 ==========

DataManager::~DataManager()
{
    if (_db) {
        sqlite3_close(_db);
        _db = nullptr;

        INF("Database closed: {}", _dbName);
    }
}

// ========== 3. 初始化数据库 ==========

bool DataManager::initDatabase()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 显式启用外键约束
    if (!executeSQL("PRAGMA foreign_keys = ON;")) {
        ERR("enable foreign_keys failed")
        return false;
    }

    // 创建会话表
    const std::string createSessionsTable =R"(
        CREATE TABLE IF NOT EXISTS sessions (
            session_id TEXT PRIMARY KEY NOT NULL,
            model_name TEXT NOT NULL,
            create_time INTEGER NOT NULL,
            update_time INTEGER NOT NULL
        );
    )";

    if (!executeSQL(createSessionsTable)) {
          ERR("create session table failed");
        return false;
    }

    // 创建消息表
    const std::string createMessagesTable =R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id TEXT PRIMARY KEY NOT NULL,
            session_id TEXT NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,

            FOREIGN KEY (session_id)
                REFERENCES sessions(session_id)
                ON DELETE CASCADE
        );
    )";

    if (!executeSQL(createMessagesTable)) {
        ERR("create messages table failed");
        return false;
    }

    // 创建消息所属会话的索引
    const std::string createMessageIndex =R"(
        CREATE INDEX IF NOT EXISTS idx_messages_session_id
        ON messages(session_id);
    )";

    if (!executeSQL(createMessageIndex)) {
        ERR("create message index failed");
        return false;
    }

    INF("Database initialized successfully");

    return true;
}


// ========== 4. 执行 SQL 语句 ==========

bool DataManager::executeSQL(const std::string& sql)
{
    char* errMsg = nullptr;

    int rc = sqlite3_exec(
        _db,
        sql.c_str(),
        nullptr,
        nullptr,
        &errMsg
    );

    if (rc != SQLITE_OK) {
        ERR(
            "Failed to execute SQL: {}",
            errMsg ? errMsg : sqlite3_errmsg(_db)
        );

        sqlite3_free(errMsg);

        return false;
    }

    return true;
}

// ========== 5. 预编译 SQL ==========

DataManager::StatementPtr DataManager::prepare(
    const std::string& sql
) const {
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(
        _db,
        sql.c_str(),
        -1,
        &stmt,
        nullptr
    );

    // 使用智能指针接管 SQLite 语句对象
    StatementPtr result(
        stmt,
        &sqlite3_finalize
    );

    if (rc != SQLITE_OK) {
        ERR(
            "Failed to prepare SQL: {}",
            sqlite3_errmsg(_db)
        );

        return StatementPtr(
            nullptr,
            &sqlite3_finalize
        );
    }

    return result;
}


// ========== 6. 执行非查询语句 ==========

bool DataManager::executeStatement(
    sqlite3_stmt* stmt
) const {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        ERR(
            "Failed to execute statement: {}",
            sqlite3_errmsg(_db)
        );

        return false;
    }

    return true;
}


// ========== 7. 绑定文本参数 ==========

bool DataManager::bindText(
    sqlite3_stmt* stmt,
    int index,
    const std::string& value
) const {
    int rc = sqlite3_bind_text64(
        stmt,
        index,
        value.c_str(),
        static_cast<sqlite3_uint64>(value.size()),
        SQLITE_TRANSIENT,
        SQLITE_UTF8
    );

    if (rc != SQLITE_OK) {
        ERR(
            "Failed to bind text: {}",
            sqlite3_errmsg(_db)
        );

        return false;
    }

    return true;
}


// ========== 8. 读取文本数据 ==========

std::string DataManager::readText(
    sqlite3_stmt* stmt,
    int column
) {
    const unsigned char* text =
        sqlite3_column_text(stmt, column);

    if (!text) {
        return "";
    }

    int bytes = sqlite3_column_bytes(stmt, column);

    return std::string(
        reinterpret_cast<const char*>(text),
        bytes
    );
}

// ========== 9. 插入会话 ==========

bool DataManager::insertSession(
    const Session& session
) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (session.id.empty() || session.model_name.empty()) {
        ERR("Invalid session");
        return false;
    }

    const std::string sql =R"(
        INSERT INTO sessions (
            session_id,
            model_name,
            create_time,
            update_time
        )
        VALUES (?, ?, ?, ?);
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    // 绑定所有参数
    if (
        !bindText(stmt.get(), 1, session.id) ||

        !bindText(stmt.get(), 2, session.model_name) ||

        sqlite3_bind_int64(
            stmt.get(),
            3,
            static_cast<sqlite3_int64>(session.created_at)
        ) != SQLITE_OK ||

        sqlite3_bind_int64(
            stmt.get(),
            4,
            static_cast<sqlite3_int64>(session.updated_at)
        ) != SQLITE_OK
    ) {
        ERR("Failed to bind session parameters");
        return false;
    }

    if (!executeStatement(stmt.get())) {
        return false;
    }

    INF("Inserted session: {}", session.id);

    return true;
}

// ========== 10. 获取指定会话 ==========

std::shared_ptr<Session> DataManager::getSession(
    const std::string& sessionId
) const {
    std::lock_guard<std::mutex> lock(_mutex);

    const std::string sql =R"(
        SELECT model_name, create_time, update_time
        FROM sessions
        WHERE session_id = ?;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return nullptr;
    }

    if (!bindText(stmt.get(), 1, sessionId)) {
        return nullptr;
    }

    int rc = sqlite3_step(stmt.get());

    // 会话不存在
    if (rc == SQLITE_DONE) {
        return nullptr;
    }

    // 查询发生错误
    if (rc != SQLITE_ROW) {
        ERR("Failed to query session: {}",
            sqlite3_errmsg(_db));

        return nullptr;
    }

    // 从数据库读取会话信息
    std::string modelName = readText(stmt.get(), 0);

    auto session = std::make_shared<Session>(modelName);

    session->id = sessionId;

    session->created_at = static_cast<std::time_t>(
        sqlite3_column_int64(stmt.get(), 1)
    );

    session->updated_at = static_cast<std::time_t>(
        sqlite3_column_int64(stmt.get(), 2)
    );

    // 提前释放当前 SQL 语句
    stmt.reset();

    // 1. 调用 readMessagesUnlocked() 查询历史消息。
    // 2. 将查询结果保存到 session->messages。
    // 3. 如果历史消息查询失败，返回 nullptr。
    if(!readMessagesUnlocked(session->id,session->messages))
    {
        ERR("read messages failed");
        return nullptr;
    }
    return session;
}
// ========== 11. 内部更新会话时间戳 ==========

bool DataManager::updateTimestampUnlocked(
    const std::string& sessionId,
    std::time_t timestamp
) {
    const std::string sql =R"(
        UPDATE sessions
        SET update_time = ?
        WHERE session_id = ?;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    if (
        sqlite3_bind_int64(
            stmt.get(),
            1,
            static_cast<sqlite3_int64>(timestamp)
        ) != SQLITE_OK ||

        !bindText(stmt.get(), 2, sessionId)
    ) {
        ERR("Failed to bind timestamp parameters");

        return false;
    }

    if (!executeStatement(stmt.get())) {
        return false;
    }

    // 确认确实匹配到会话记录
    return sqlite3_changes(_db) > 0;
}


// ========== 12. 对外更新时间戳接口 ==========

bool DataManager::updateSessionTimestamp(
    const std::string& sessionId,
    std::time_t timestamp
) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!updateTimestampUnlocked(sessionId, timestamp)) {
        ERR("Failed to update session: {}", sessionId);

        return false;
    }

    INF("Updated session timestamp: {}", sessionId);
    return true;
}
// ========== 13. 删除会话 ==========

bool DataManager::deleteSession(
    const std::string& sessionId
) {
    std::lock_guard<std::mutex> lock(_mutex);

    const std::string sql =R"(
        DELETE FROM sessions
        WHERE session_id = ?;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    if (!bindText(stmt.get(), 1, sessionId)) {
        return false;
    }

    if (!executeStatement(stmt.get())) {
        return false;
    }

    if (sqlite3_changes(_db) == 0) {
        return false;
    }

    // 历史消息通过外键约束自动删除
    INF("Deleted session: {}", sessionId);

    return true;
}
// ========== 14. 获取全部会话 ID ==========

std::vector<std::string>
DataManager::getAllSessionIds() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<std::string> sessionIds;

    const std::string sql =R"(
        SELECT session_id
        FROM sessions
        ORDER BY update_time DESC, session_id DESC;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return {};
    }

    int rc;

    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        sessionIds.push_back(
            readText(stmt.get(), 0)
        );
    }

    if (rc != SQLITE_DONE) {
        ERR("Failed to query session IDs: {}",
            sqlite3_errmsg(_db));

        return {};
    }

    return sessionIds;
}
// ========== 15. 获取全部会话信息 ==========

std::vector<std::shared_ptr<Session>>
DataManager::getAllSessions() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<std::shared_ptr<Session>> sessions;

    const std::string sql =R"(
        SELECT
            session_id,
            model_name,
            create_time,
            update_time
        FROM sessions
        ORDER BY update_time DESC, session_id DESC;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return {};
    }

    int rc;

    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {

        std::string sessionId = readText(stmt.get(), 0);

        std::string modelName = readText(stmt.get(), 1);

        auto session =
            std::make_shared<Session>(modelName);

        session->id = sessionId;

        session->created_at = static_cast<std::time_t>(
            sqlite3_column_int64(stmt.get(), 2)
        );

        session->updated_at = static_cast<std::time_t>(
            sqlite3_column_int64(stmt.get(), 3)
        );

        // 不主动加载历史消息
        sessions.push_back(session);
    }

    if (rc != SQLITE_DONE) {
        ERR("Failed to query sessions: {}",
            sqlite3_errmsg(_db));

        return {};
    }

    return sessions;
}
// ========== 16. 获取会话数量 ==========

size_t DataManager::getSessionCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto stmt = prepare(
        "SELECT COUNT(*) FROM sessions;"
    );

    if (!stmt) {
        return 0;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        ERR("Failed to count sessions: {}",
            sqlite3_errmsg(_db));

        return 0;
    }

    return static_cast<size_t>(
        sqlite3_column_int64(stmt.get(), 0)
    );
}

// ========== 17. 清空所有会话 ==========

bool DataManager::clearAllSessions()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 通过外键约束同步清空所有历史消息
    if (!executeSQL("DELETE FROM sessions;")) {
        return false;
    }

    INF("Clear all sessions successfully");

    return true;
}

// ========== 18. 内部插入消息 ==========

bool DataManager::insertMessageUnlocked(
    const std::string& sessionId,
    const Message& message
) {
    const std::string sql =R"(
        INSERT INTO messages (
            message_id,
            session_id,
            role,
            content,
            timestamp
        )
        VALUES (?, ?, ?, ?, ?);
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    if (
        !bindText(stmt.get(), 1, message.id) ||

        !bindText(stmt.get(), 2, sessionId) ||

        !bindText(stmt.get(), 3, message.role) ||

        !bindText(stmt.get(), 4, message.content) ||

        sqlite3_bind_int64(
            stmt.get(),
            5,
            static_cast<sqlite3_int64>(message.timestamp)
        ) != SQLITE_OK
    ) {
        ERR("Failed to bind message parameters");

        return false;
    }

    return executeStatement(stmt.get());
}

// ========== 19. 插入消息并同步更新会话 ==========

bool DataManager::insertMessage(
    const std::string& sessionId,
    const Message& message
) {
    std::lock_guard<std::mutex> lock(_mutex);

    // 检查会话 ID 和消息 ID
    if (sessionId.empty() || message.id.empty()) {
        ERR("Invalid session ID or message ID");

        return false;
    }


    // 使用 executeSQL() 开启事务。
    std::string sql = "BEGIN IMMEDIATE";
    if(!executeSQL(sql))
    {
        ERR("enable IMMEDIATE failed");
        return false;
    }

    // 如果插入失败：
    // 执行 ROLLBACK，然后返回 false。
    if(!insertMessageUnlocked(sessionId,message))
    {
        executeSQL("ROLLBACK;");
        return false;
    }

    // 如果更新失败：
    // 执行 ROLLBACK，然后返回 false。
    if(!updateTimestampUnlocked(sessionId,message.timestamp))
    {
        executeSQL("ROLLBACK;");
        return false;
    }


    // 如果提交失败：
    // 尝试执行 ROLLBACK，然后返回 false。
    sql = "COMMIT";
    if(!executeSQL(sql))
    {
        ERR("commit failed");
        executeSQL("ROLLBACK;");
        return false;
    }


    // 输出成功日志并返回 true。
    // 日志只记录消息 ID 和会话 ID。
    INF("insert message successfully: {}, {}",
        message.id,
        sessionId
    );

    return true;
}

// ========== 20. 内部查询历史消息 ==========

bool DataManager::readMessagesUnlocked(
    const std::string& sessionId,
    std::vector<Message>& messages
) const {
    const std::string sql =R"(
        SELECT message_id, role, content, timestamp
        FROM messages
        WHERE session_id = ?
        ORDER BY timestamp ASC, rowid ASC;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    if (!bindText(stmt.get(), 1, sessionId)) {
        return false;
    }

    // 临时容器，避免失败时返回不完整的数据
    std::vector<Message> result;

    int rc;

    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {

        std::string messageId =
            readText(stmt.get(), 0);

        std::string role =
            readText(stmt.get(), 1);

        std::string content =
            readText(stmt.get(), 2);

        std::time_t timestamp =
            static_cast<std::time_t>(
                sqlite3_column_int64(stmt.get(), 3)
            );

        // 使用已有的 Message 构造函数
        Message message(role, content);

        // 恢复原来的消息 ID 和时间戳
        message.id = messageId;
        message.timestamp = timestamp;

        result.push_back(std::move(message));
    }

    // 判断整个查询是否成功完成
    if (rc != SQLITE_DONE) {
        ERR("Failed to query messages: {}",
            sqlite3_errmsg(_db));

        return false;
    }

    messages = std::move(result);

    return true;
}

// ========== 21. 获取历史消息 ==========

std::vector<Message>
DataManager::getMessagesBySessionId(
    const std::string& sessionId
) const {
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<Message> messages;

    if (!readMessagesUnlocked(sessionId, messages)) {
        return {};
    }

    return messages;
}

// ========== 22. 删除指定会话的历史消息 ==========

bool DataManager::deleteMessagesBySessionId(
    const std::string& sessionId
) {
    std::lock_guard<std::mutex> lock(_mutex);

    const std::string sql =R"(
        DELETE FROM messages
        WHERE session_id = ?;
    )";

    auto stmt = prepare(sql);

    if (!stmt) {
        return false;
    }

    if (!bindText(stmt.get(), 1, sessionId)) {
        return false;
    }

    if (!executeStatement(stmt.get())) {
        return false;
    }

    INF("Deleted all messages for session: {}", sessionId);

    return true;
}


}// namespace ai_chat_sdk
