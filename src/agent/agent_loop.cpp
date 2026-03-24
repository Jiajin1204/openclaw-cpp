#include "agent_loop.h"
#include "../session/session_manager.h"
#include "../tools/tool_engine.h"
#include "../utils/logger.h"

namespace openclaw {

AgentLoop::AgentLoop(SessionManager* session_manager, ToolEngine* tool_engine)
    : session_manager_(session_manager)
    , tool_engine_(tool_engine)
{
    model_client_ = std::make_unique<ModelClient>();
    
    // 默认系统提示词
    system_prompt_ = R"(You are OpenClaw C++, an AI assistant. You can help users with various tasks.
    
You have access to tools:
- exec: Execute shell commands
- read: Read files
- write: Write files

Always be helpful and concise. Use tools when needed.)";
}

AgentLoop::~AgentLoop() {
}

void AgentLoop::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

void AgentLoop::set_model_client(std::unique_ptr<ModelClient> client) {
    model_client_ = std::move(client);
}

std::vector<Message> AgentLoop::build_messages(
    const std::string& session_key,
    const std::string& user_message
) {
    std::vector<Message> messages;
    
    // 添加系统消息
    messages.push_back(Message(MessageRole::System, system_prompt_));
    
    // 添加历史消息
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            auto context = session->get_context(10);  // 最近 10 条
            for (const auto& msg : context) {
                messages.push_back(msg);
            }
        }
    }
    
    // 添加用户新消息
    messages.push_back(Message(MessageRole::User, user_message));
    
    return messages;
}

std::vector<ToolResult> AgentLoop::process_tool_calls(
    const std::vector<ToolCall>& tool_calls
) {
    std::vector<ToolResult> results;
    
    if (!tool_engine_) {
        return results;
    }
    
    for (const auto& tc : tool_calls) {
        LOG_INFO("Executing tool: ", tc.name);
        
        // 执行工具
        auto result = tool_engine_->execute(tc.name, tc.arguments);
        
        ToolResult tr;
        tr.tool_call_id = tc.id;
        tr.content = result.value;
        tr.is_error = !result.ok;
        
        if (!result.ok) {
            tr.content = "Error: " + result.error;
        }
        
        results.push_back(tr);
        
        LOG_INFO("Tool result: ", tr.content.substr(0, 100));
    }
    
    return results;
}

std::string AgentLoop::run(const std::string& session_key, const std::string& user_message) {
    LOG_INFO("Agent run for session: ", session_key);
    
    // 1. 构建消息
    std::vector<Message> messages = build_messages(session_key, user_message);
    
    // 添加用户消息到会话
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::User, user_message));
        }
    }
    
    // 2. 调用模型
    auto result = model_client_->chat(messages);
    
    if (!result.ok) {
        LOG_ERROR("Model call failed: ", result.error);
        return "Sorry, I encountered an error: " + result.error;
    }
    
    std::string response = result.value.content;
    
    // 3. 处理工具调用（简化版：目前不支持）
    // 实际实现需要解析 result.value.tool_calls
    
    // 4. 添加响应到会话
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::Assistant, response));
            session_manager_->save_session(session_key);
        }
    }
    
    LOG_INFO("Agent response: ", response.substr(0, 100));
    return response;
}

void AgentLoop::run_stream(
    const std::string& session_key,
    const std::string& user_message,
    std::function<void(const std::string&)> on_chunk
) {
    // 构建消息
    std::vector<Message> messages = build_messages(session_key, user_message);
    
    // 添加用户消息到会话
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::User, user_message));
        }
    }
    
    // 流式调用
    std::string full_response;
    
    auto result = model_client_->chat_stream(messages, 
        [&on_chunk, &full_response](const std::string& chunk) {
            full_response += chunk;
            on_chunk(chunk);
        }
    );
    
    if (!result.ok) {
        on_chunk("Error: " + result.error);
        return;
    }
    
    // 添加响应到会话
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::Assistant, full_response));
            session_manager_->save_session(session_key);
        }
    }
}

} // namespace openclaw
