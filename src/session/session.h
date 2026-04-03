#ifndef OPENCLAW_SESSION_H
#define OPENCLAW_SESSION_H

#include <string>
#include <vector>
#include <ctime>
#include <optional>
#include "openclaw/message.h"

namespace openclaw {

// ============ 会话结构 ============

struct Session {
    std::string session_key;      // e.g., "agent:main:main"
    std::string agent_id;         // e.g., "main"
    
    std::vector<Message> history;
    
    int total_tokens = 0;
    int input_tokens = 0;
    int output_tokens = 0;
    
    time_t created_at = std::time(nullptr);
    time_t last_active = std::time(nullptr);
    
    // 元数据
    std::string channel;          // 来源渠道
    std::string sender;           // 发送者
    std::string display_name;     // 显示名称
    
    // 兼容方法
    std::string key() const { return session_key; }
    
    // 获取历史
    std::vector<Message> get_history(int count = 10) const {
        if (history.size() <= static_cast<size_t>(count)) {
            return history;
        }
        return std::vector<Message>(history.end() - count, history.end());
    }
    
    // 添加消息到历史
    void add_message(const Message& msg) {
        history.push_back(msg);
        last_active = std::time(nullptr);
    }
    
    // 获取上下文消息（用于 LLM）
    std::vector<Message> get_context(int max_messages = 20) const {
        if (history.size() <= static_cast<size_t>(max_messages)) {
            return history;
        }
        return std::vector<Message>(
            history.end() - max_messages, 
            history.end()
        );
    }
    
    // 清空历史
    void clear() {
        history.clear();
        total_tokens = 0;
        input_tokens = 0;
        output_tokens = 0;
    }
    
    // 序列化到 JSON
    std::string to_json() const;
    
    // 从 JSON 反序列化
    static Session from_json(const std::string& json_str);
};

// ============ 会话键解析 ============

struct SessionKey {
    std::string agent_id;
    std::string channel;      // e.g., "telegram", "discord"
    std::string type;         // e.g., "main", "group", "channel"
    std::string id;           // e.g., group id, channel id
    
    static SessionKey parse(const std::string& key);
    
    std::string to_string() const;
};

} // namespace openclaw

#endif // OPENCLAW_SESSION_H
