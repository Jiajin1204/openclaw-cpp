#include "session_manager.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <chrono>

namespace openclaw {

SessionManager::SessionManager() {
    // 默认会话目录
    const char* home = std::getenv("HOME");
    if (home) {
        sessions_dir_ = std::string(home) + "/.openclaw-cpp/sessions";
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

void SessionManager::set_sessions_dir(const std::string& dir) {
    sessions_dir_ = dir;
    std::filesystem::create_directories(sessions_dir_);
    LOG_INFO("Sessions directory set to: ", sessions_dir_);
}

Session* SessionManager::get_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    
    // 尝试从磁盘加载
    auto loaded = load_session_from_disk(key);
    if (loaded) {
        Session* ptr = loaded.get();
        sessions_[key] = std::move(loaded);
        return ptr;
    }
    
    // 创建新会话
    auto new_session = std::make_unique<Session>();
    new_session->session_key = key;
    Session* ptr = new_session.get();
    sessions_[key] = std::move(new_session);
    
    return ptr;
}

bool SessionManager::save_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(key);
    if (it == sessions_.end()) {
        return false;
    }
    
    return save_session_to_disk(*it->second);
}

bool SessionManager::save_session_to_disk(const Session& session) {
    std::string filepath = sessions_dir_ + "/" + session.key() + ".jsonl";
    
    std::ofstream file(filepath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open session file: ", filepath);
        return false;
    }
    
    for (const auto& msg : session.history) {
        nlohmann::json j;
        j["role"] = role_to_string(msg.role);
        j["content"] = msg.content;
        if (msg.tool_call_id) j["tool_call_id"] = *msg.tool_call_id;
        if (msg.name) j["name"] = *msg.name;
        if (msg.sender != "") j["sender"] = msg.sender;
        if (msg.channel != "") j["channel"] = msg.channel;
        if (msg.timestamp != 0) j["timestamp"] = msg.timestamp;
        
        file << j.dump() << "\n";
    }
    
    return true;
}

std::unique_ptr<Session> SessionManager::load_session_from_disk(const std::string& key) {
    std::string filepath = sessions_dir_ + "/" + key + ".jsonl";
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return nullptr;
    }
    
    auto session = std::make_unique<Session>();
    session->session_key = key;
    std::string line;
    
    while (std::getline(file, line)) {
        try {
            auto j = nlohmann::json::parse(line);
            Message msg;
            msg.role = string_to_role(j.value("role", "user"));
            msg.content = j.value("content", "");
            if (j.contains("tool_call_id")) msg.tool_call_id = j["tool_call_id"].get<std::string>();
            if (j.contains("name")) msg.name = j["name"].get<std::string>();
            if (j.contains("sender")) msg.sender = j["sender"].get<std::string>();
            if (j.contains("channel")) msg.channel = j["channel"].get<std::string>();
            if (j.contains("timestamp")) msg.timestamp = j["timestamp"].get<int64_t>();
            
            session->add_message(msg);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse session line: ", e.what());
        }
    }
    
    LOG_INFO("Loaded session ", key, " with ", session->history.size(), " messages");
    return session;
}

std::vector<std::string> SessionManager::list_sessions() {
    std::vector<std::string> result;
    
    if (!std::filesystem::exists(sessions_dir_)) {
        return result;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(sessions_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
            std::string name = entry.path().stem();
            result.push_back(name);
        }
    }
    
    return result;
}

bool SessionManager::delete_session(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filepath = sessions_dir_ + "/" + key + ".jsonl";
    if (std::filesystem::remove(filepath)) {
        sessions_.erase(key);
        LOG_INFO("Deleted session: ", key);
        return true;
    }
    
    return false;
}

void SessionManager::prune_old_sessions(int days) {
    auto now = std::chrono::system_clock::now();
    auto cutoff_time = now - std::chrono::hours(days * 24);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> to_delete;
    
    for (const auto& entry : std::filesystem::directory_iterator(sessions_dir_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
            continue;
        }
        
        auto time = std::filesystem::last_write_time(entry);
        auto time_since_epoch = time.time_since_epoch();
        auto file_time = std::chrono::system_clock::time_point(time_since_epoch);
        
        if (file_time < cutoff_time) {
            std::string key = entry.path().stem();
            to_delete.push_back(key);
        }
    }
    
    for (const auto& key : to_delete) {
        std::string filepath = sessions_dir_ + "/" + key + ".jsonl";
        std::filesystem::remove(filepath);
        sessions_.erase(key);
        LOG_INFO("Pruned old session: ", key);
    }
    
    if (!to_delete.empty()) {
        LOG_INFO("Pruned ", to_delete.size(), " old sessions");
    }
}

} // namespace openclaw