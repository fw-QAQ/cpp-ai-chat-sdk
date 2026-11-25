#include "SessionManager.h"

#include <bite_scaffold/log.h>

#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <ctime>

namespace ai_chat_sdk {

std::atomic<int64_t>
    SessionManager::_message_counter{0};

std::atomic<int64_t>
    SessionManager::_session_counter{0};

namespace {

// 生成 128 位随机后缀
std::string generateRandomSuffix()
{
    unsigned char bytes[16];

    sqlite3_randomness(sizeof(bytes), bytes);

    std::ostringstream os;

    os << std::hex << std::setfill('0');

    for (unsigned char byte : bytes) {
        os << std::setw(2)
           << static_cast<unsigned int>(byte);
    }

    return os.str();
}

} // anonymous namespace


// ========== 生成消息 ID ==========

std::string SessionManager::generateMessageId()
{
    int64_t count =
        _message_counter.fetch_add(1) + 1;

    std::ostringstream os;

    os << "msg_"
       << std::time(nullptr)
       << "_"
       << std::setfill('0')
       << std::setw(8)
       << count
       << "_"
       << generateRandomSuffix();

    return os.str();
}


// ========== 生成会话 ID ==========

std::string SessionManager::generateSessionId()
{
    int64_t count =
        _session_counter.fetch_add(1) + 1;

    std::ostringstream os;

    os << "session_"
       << std::time(nullptr)
       << "_"
       << std::setfill('0')
       << std::setw(8)
       << count
       << "_"
       << generateRandomSuffix();

    return os.str();
}

SessionManager::SessionManager(
    const std::string& dbName
)
    : _dataManager(dbName)
{
    // 从数据库读取全部会话的基本信息
    auto sessions = _dataManager.getAllSessions();


    // 遍历 sessions，将每个会话添加到 _sessions。
    // Key：session->id
    // Value：session 智能指针
    for(auto session:sessions)
    {
        _sessions[session->id] = session;
    }

    INF(
        "SessionManager initialized, session count: {}",
        _sessions.size()
    );
}

std::string SessionManager::createSession(
    const std::string& model_name
) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (model_name.empty()) {
        ERR("Invalid model name");
        return "";
    }

    // 生成会话 ID
    std::string session_id = generateSessionId();

    // 创建会话对象
    auto session =
        std::make_shared<Session>(model_name);

    session->id = session_id;

    // 调用 _dataManager.insertSession()，
    // 将会话保存到数据库。
    // 如果保存失败，返回空字符串。
    if(!_dataManager.insertSession(*session))
    {
        return "";
    }

    // 将会话添加到 _sessions。
    _sessions[session_id] = session;

    // 将会话 ID 添加到 _loadedSessions。
    _loadedSessions.insert(session_id);

    INF(
        "Created persistent session: {}, model: {}",
        session_id,
        model_name
    );

    return session_id;
}

std::shared_ptr<Session>
SessionManager::loadSessionUnlocked(
    const std::string& session_id
) {
    // 查找内存中的会话
    auto it = _sessions.find(session_id);

    // 判断会话是否存在于 _sessions，
    // 并且 session_id 是否存在于 _loadedSessions。

    // 如果两个条件同时成立，
    // 直接返回缓存中的会话对象。
    if(it != _sessions.end() &&
    _loadedSessions.find(session_id) != _loadedSessions.end())
    {
        return it->second;
    }

    // 调用 _dataManager.getSession(session_id)，
    // 从数据库读取完整会话。
    // 如果查询失败或者会话不存在，
    // 返回 nullptr。
    std::shared_ptr<Session>session =  _dataManager.getSession(session_id);
    if(session == nullptr)
    {
        return nullptr;
    }
    // 将数据库返回的会话保存到 _sessions。
    // 如果原来存在只有基本信息的会话，则覆盖它。
    _sessions[session_id] = session;

    // 将 session_id 添加到 _loadedSessions。
    _loadedSessions.insert(session_id);


    return session;
}

std::shared_ptr<Session>
SessionManager::getSession(
    const std::string& session_id
) {
    std::lock_guard<std::mutex> lock(_mutex);

    return loadSessionUnlocked(session_id);
}

bool SessionManager::addMessage(
    const std::string& session_id,
    const Message& message
) {
    std::lock_guard<std::mutex> lock(_mutex);

    // 获取完整的会话对象
    auto session = loadSessionUnlocked(session_id);

    if (!session) {
        ERR("Session not found: {}", session_id);
        return false;
    }

    // 构造新消息
    Message new_message(
        message.role,
        message.content
    );

    // 生成消息 ID
    new_message.id = generateMessageId();

    // 提前为内存消息列表分配足够空间
    session->messages.reserve(
        session->messages.size() + 1
    );


    // 调用 _dataManager.insertMessage()，
    // 将 new_message 保存到数据库。

    // 如果数据库写入失败，
    // 不修改内存消息列表，直接返回 false。
    if(!_dataManager.insertMessage(session_id,new_message))
    {
        return false;
    }

    // 更新内存中的 session->updated_at。
    // 使用 new_message.timestamp。
    session->updated_at = new_message.timestamp;

    // 将 new_message 添加到 session->messages。
    // 可以使用 std::move() 转移消息对象。
    session->messages.push_back(std::move(new_message));

    INF("Added persistent message to session: {}",
        session_id);

    return true;
}

std::vector<Message>
SessionManager::getSessionHistory(
    const std::string& session_id
) {
    std::lock_guard<std::mutex> lock(_mutex);

    // 确保历史消息已经加载
    auto session = loadSessionUnlocked(session_id);

    if (!session) {
        return {};
    }

    // 返回历史消息的副本
    return session->messages;
}

std::vector<std::string>
SessionManager::getSessionList() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    return _dataManager.getAllSessionIds();
}

bool SessionManager::deleteSession(
    const std::string& session_id
) {
    std::lock_guard<std::mutex> lock(_mutex);

    // 首先删除数据库中的会话
    // 历史消息会通过外键约束级联删除
    if (!_dataManager.deleteSession(session_id)) {
        ERR("Failed to delete session: {}", session_id);
        return false;
    }

    // 删除内存缓存
    _sessions.erase(session_id);

    // 删除加载状态
    _loadedSessions.erase(session_id);

    INF("Deleted persistent session: {}", session_id);

    return true;
}

void SessionManager::updateSessionTimestamp(
    const std::string& session_id
) {
    std::lock_guard<std::mutex> lock(_mutex);

    // 生成统一的时间戳
    std::time_t timestamp = std::time(nullptr);

    // 首先更新数据库
    if (!_dataManager.updateSessionTimestamp(
            session_id, timestamp)) {

        ERR("Failed to update persistent session: {}",
            session_id);

        return;
    }

    // 如果内存中存在该会话，则同步更新时间
    auto it = _sessions.find(session_id);

    if (it != _sessions.end()) {
        it->second->updated_at = timestamp;
    }
}

void SessionManager::clearAllSessions()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 清空持久化数据
    if (!_dataManager.clearAllSessions()) {
        ERR("Failed to clear persistent sessions");
        return;
    }

    // 清空内存缓存
    _sessions.clear();

    // 清空加载状态
    _loadedSessions.clear();

    INF("Cleared all persistent sessions");
}

size_t SessionManager::getSessionCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    return _dataManager.getSessionCount();
}

} // namespace ai_chat_sdk
