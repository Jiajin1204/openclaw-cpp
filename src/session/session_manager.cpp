#include "session_manager.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>

namespace openclaw {

SessionManager::SessionManager() {
    // 默认会话目录
    const char* home = std::getenv("HOME");
    if (home) {
        sessions_dir_ = std::string(home) + "/.openclaw/agents/main/sessions";
    } else {
        sessions_dir_ = "./sessions";
    }
    
    // 创建目录
    std::filesystem::create_directories(sessions_dir_);
    
    LOG_INFO("SessionManager initialized with dir: ", sessions_dir_);
}

SessionManager::~SessionManager() {
    // 保存所有会话
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, session] : sessions_) {
        save_session_to_disk(*session);
    }
}

Session* SessionManager::get_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    
    // 创建新会话
    auto session = std::make_shared<Session>();
    session->session_key = key;
    
    // 解析 key 获取 agent_id
    if (key.find("agent:") == 0) {
        size_t colon2 = key.find(':', 6);
        if (colon2 != std::string::npos) {
            session->agent_id = key.substr(6, colon2 - 6);
        }
    }
    
    sessions_[key] = session;
    
    // 尝试从磁盘加载
    load_session_from_disk(key);
    
    return session.get();
}

void SessionManager::save_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        save_session_to_disk(*it->second);
    }
}

void SessionManager::delete_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        // 删除磁盘文件
        std::string filename = sessions_dir_ + "/" + key + ".json";
        std::remove(filename.c_str());
        
        sessions_.erase(it);
    }
}

std::vector<std::string> SessionManager::list_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    for (const auto& [key, session] : sessions_) {
        keys.push_back(key);
    }
    
    return keys;
}

size_t SessionManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::cleanup(int max_age_days) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    time_t now = std::time(nullptr);
    time_t max_age = max_age_days * 24 * 60 * 60;
    
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (now - it->second->last_active > max_age) {
            // 删除磁盘文件
            std::string filename = sessions_dir_ + "/" + it->first + ".json";
            std::remove(filename.c_str());
            
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SessionManager::get_sessions_dir() const {
    return sessions_dir_;
}

void SessionManager::set_sessions_dir(const std::string& dir) {
    sessions_dir_ = dir;
    std::filesystem::create_directories(sessions_dir_);
}

void SessionManager::load_session_from_disk(const std::string& key) {
    std::string filename = sessions_dir_ + "/" + key + ".json";
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;  // 文件不存在，正常情况
    }
    
    try {
        // 简单读取 JSON
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // TODO: 完整 JSON 解析
        // 目前简化处理
        
        LOG_DEBUG("Loaded session from disk: ", key);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load session ", key, ": ", e.what());
    }
}

void SessionManager::save_session_to_disk(const Session& session) {
    std::string filename = sessions_dir_ + "/" + session.session_key + ".json";
    
    // 确保目录存在
    std::filesystem::create_directories(sessions_dir_);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to save session to: ", filename);
        return;
    }
    
    // 简单 JSON 输出
    file << "{\n";
    file << "  \"session_key\": \"" << session.session_key << "\",\n";
    file << "  \"agent_id\": \"" << session.agent_id << "\",\n";
    file << "  \"total_tokens\": " << session.total_tokens << ",\n";
    file << "  \"created_at\": " << session.created_at << ",\n";
    file << "  \"last_active\": " << session.last_active << ",\n";
    file << "  \"history_count\": " << session.history.size() << "\n";
    file << "}\n";
    
    LOG_DEBUG("Saved session to disk: ", session.session_key);
}

} // namespace openclaw
