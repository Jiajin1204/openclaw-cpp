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
    model_client_->set_tool_engine(tool_engine_);
    
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

// 构建消息列表
std::vector<Message> AgentLoop::build_messages(
    const std::string& session_key,
    const std::string& user_message
) {
    std::vector<Message> messages;
    messages.push_back(Message(MessageRole::System, system_prompt_));
    
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            auto context = session->get_context(10);
            for (const auto& msg : context) {
                messages.push_back(msg);
            }
        }
    }
    
    messages.push_back(Message(MessageRole::User, user_message));
    return messages;
}

// 处理工具调用
std::vector<ToolResult> AgentLoop::process_tool_calls(
    const std::vector<ToolCall>& tool_calls
) {
    std::vector<ToolResult> results;
    
    if (!tool_engine_) {
        return results;
    }
    
    for (const auto& tc : tool_calls) {
        auto result = tool_engine_->execute(tc.name, tc.arguments);
        
        ToolResult tr;
        tr.tool_call_id = tc.id;
        tr.content = result.value;
        tr.is_error = !result.ok;
        
        if (!result.ok) {
            tr.content = "Error: " + result.error;
        }
        
        results.push_back(tr);
    }
    
    return results;
}

// 运行 Agent（单次对话）
std::string AgentLoop::run(const std::string& session_key, const std::string& user_message) {
    // 构建消息
    std::vector<Message> messages = build_messages(session_key, user_message);
    
    // 保存用户消息
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::User, user_message));
        }
    }
    
    // 调用模型
    auto result = model_client_->chat(messages);
    
    if (!result.ok) {
        return "Sorry, I encountered an error: " + result.error;
    }
    
    std::string response = result.value.content;
    std::vector<ToolCall> tool_calls = result.value.tool_calls;
    
    // 处理工具调用
    if (!tool_calls.empty()) {
        auto tool_results = process_tool_calls(tool_calls);
        
        // 直接返回工具执行结果
        response = "工具执行完成。结果：\n" + tool_results[0].content;
        
        // TODO: 实现第二次模型调用，让模型生成更自然的回复
    }
    
    // 保存助手回复
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::Assistant, response));
            session_manager_->save_session(session_key);
        }
    }
    
    return response;
}

// 运行 Agent（流式输出）
void AgentLoop::run_stream(
    const std::string& session_key,
    const std::string& user_message,
    std::function<void(const std::string&)> on_chunk
) {
    std::vector<Message> messages = build_messages(session_key, user_message);
    
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::User, user_message));
        }
    }
    
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
    
    if (session_manager_) {
        Session* session = session_manager_->get_session(session_key);
        if (session) {
            session->add_message(Message(MessageRole::Assistant, full_response));
            session_manager_->save_session(session_key);
        }
    }
}

} // namespace openclaw