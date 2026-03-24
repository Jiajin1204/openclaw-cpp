#ifndef OPENCLAW_MESSAGE_H
#define OPENCLAW_MESSAGE_H

#include <string>
#include <vector>
#include <optional>
#include <ctime>
#include "types.h"

namespace openclaw {

// ============ 消息角色 ============

enum class MessageRole {
    System,
    User,
    Assistant,
    Tool,
    ToolResult
};

inline std::string role_to_string(MessageRole role) {
    switch (role) {
        case MessageRole::System: return "system";
        case MessageRole::User: return "user";
        case MessageRole::Assistant: return "assistant";
        case MessageRole::Tool: return "tool";
        case MessageRole::ToolResult: return "tool_result";
        default: return "user";
    }
}

inline MessageRole string_to_role(const std::string& s) {
    if (s == "system") return MessageRole::System;
    if (s == "assistant") return MessageRole::Assistant;
    if (s == "tool") return MessageRole::Tool;
    if (s == "tool_result") return MessageRole::ToolResult;
    return MessageRole::User;
}

// ============ 消息结构 ============

struct Message {
    MessageRole role = MessageRole::User;
    std::string content;
    
    // For tool calls
    std::optional<std::string> tool_call_id;
    std::optional<std::string> tool_name;
    
    // For tool results
    std::optional<std::string> name;
    
    // Metadata
    std::string sender;
    std::string channel;
    int64_t timestamp = 0;
    
    // Token count (optional)
    std::optional<int> input_tokens;
    std::optional<int> output_tokens;
    
    Message() : timestamp(std::time(nullptr)) {}
    
    Message(MessageRole r, const std::string& c) 
        : role(r), content(c), timestamp(std::time(nullptr)) {}
    
    // Convert to JSON for LLM API
    Json to_json() const {
        std::map<std::string, Json> m;
        m["role"] = role_to_string(role);
        m["content"] = content;
        
        if (tool_call_id && !tool_call_id->empty()) {
            m["tool_call_id"] = *tool_call_id;
        }
        if (name && !name->empty()) {
            m["name"] = *name;
        }
        
        return m;
    }
    
    static Message from_json(const Json& j) {
        Message msg;
        
        // 解析 JSON（简化版，需要调用方确保格式正确）
        // 实际实现应该用完整的 JSON 解析
        return msg;
    }
};

// ============ 工具调用 ============

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // JSON string
    
    Json to_json() const {
        std::map<std::string, Json> m;
        m["id"] = id;
        m["type"] = "function";
        
        std::map<std::string, Json> function;
        function["name"] = name;
        function["arguments"] = arguments;
        m["function"] = function;
        
        return m;
    }
};

struct ToolResult {
    std::string tool_call_id;
    std::string content;
    bool is_error = false;
    
    Message to_message() const {
        Message msg;
        msg.role = MessageRole::ToolResult;
        msg.content = content;
        msg.tool_call_id = tool_call_id;
        return msg;
    }
};

} // namespace openclaw

#endif // OPENCLAW_MESSAGE_H
