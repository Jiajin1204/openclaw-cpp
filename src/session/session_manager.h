#ifndef OPENCLAW_SESSION_MANAGER_H
#define OPENCLAW_SESSION_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <fstream>
#include <filesystem>

#include "session.h"

namespace openclaw {

// ============ Session Manager ============

class SessionManager {
public:
    SessionManager();
    ~SessionManager();
    
    // 获取或创建会话
    Session* get_session(const std::string& key);
    
    // 保存会话
    bool save_session(const std::string& key);
    
    // 删除会话
    bool delete_session(const std::string& key);
    
    // 列出会话
    std::vector<std::string> list_sessions();
    
    // 会话数量
    size_t count() const { return sessions_.size(); }
    
    // 清理过期会话
    void prune_old_sessions(int days = 30);
    
    // 获取会话目录
    std::string get_sessions_dir() const;
    void set_sessions_dir(const std::string& dir);
    
private:
    std::map<std::string, std::unique_ptr<Session>> sessions_;
    mutable std::mutex mutex_;
    
    std::string sessions_dir_;
    
    // 持久化
    std::unique_ptr<Session> load_session_from_disk(const std::string& key);
    bool save_session_to_disk(const Session& session);
};

} // namespace openclaw

#endif // OPENCLAW_SESSION_MANAGER_H