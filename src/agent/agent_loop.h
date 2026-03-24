#ifndef OPENCLAW_AGENT_LOOP_H
#define OPENCLAW_AGENT_LOOP_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "model_client.h"

namespace openclaw {

// 前向声明
class SessionManager;
class ToolEngine;

// ============ Agent Loop ============

class AgentLoop {
public:
    AgentLoop(SessionManager* session_manager, ToolEngine* tool_engine);
    ~AgentLoop();
    
    // 运行 Agent（单轮）
    std::string run(const std::string& session_key, const std::string& user_message);
    
    // 运行 Agent（流式）
    void run_stream(
        const std::string& session_key,
        const std::string& user_message,
        std::function<void(const std::string&)> on_chunk
    );
    
    // 设置系统提示词
    void set_system_prompt(const std::string& prompt);
    
    // 配置
    void set_model_client(std::unique_ptr<ModelClient> client);
    
private:
    SessionManager* session_manager_;
    ToolEngine* tool_engine_;
    std::unique_ptr<ModelClient> model_client_;
    
    std::string system_prompt_;
    
    // 构建消息列表
    std::vector<Message> build_messages(
        const std::string& session_key,
        const std::string& user_message
    );
    
    // 处理工具调用
    std::vector<ToolResult> process_tool_calls(
        const std::vector<ToolCall>& tool_calls
    );
};

} // namespace openclaw

#endif // OPENCLAW_AGENT_LOOP_H
