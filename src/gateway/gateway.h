#ifndef OPENCLAW_GATEWAY_H
#define OPENCLAW_GATEWAY_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>

#include "ws_server.h"
#include "../session/session_manager.h"
#include "../agent/agent_loop.h"
#include "../tools/tool_engine.h"

namespace openclaw {

// ============ RPC 请求/响应 ============

struct RPCRequest {
    std::string id;
    std::string method;
    std::map<std::string, std::string> params;
    
    // 解析 JSON
    static RPCRequest from_json(const std::string& json_str);
};

struct RPCResponse {
    std::string id;
    bool ok = true;
    std::string error;
    std::map<std::string, std::string> payload;
    
    std::string to_json() const;
};

// ============ Gateway ============

class Gateway {
public:
    Gateway();
    ~Gateway();
    
    // 启动/停止
    bool start();
    void stop();
    bool is_running() const;
    
    // 配置
    void set_config(const Config& config);
    
    // 消息处理
    void handle_message(const std::string& client_id, const std::string& message);
    
    // 获取状态
    std::string status() const;
    
private:
    // 组件
    std::unique_ptr<WSServer> ws_server_;
    std::unique_ptr<SessionManager> session_manager_;
    std::unique_ptr<ToolEngine> tool_engine_;
    std::unique_ptr<AgentLoop> agent_loop_;
    
    Config config_;
    std::atomic<bool> running_{false};
    
    // 客户端状态
    std::map<std::string, std::string> client_sessions_;  // client_id -> session_key
    std::mutex client_mutex_;
    
    // 消息队列（用于 agent 任务）
    struct AgentTask {
        std::string client_id;
        std::string session_key;
        std::string message;
        std::string request_id;
    };
    
    std::queue<AgentTask> agent_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    
    // 处理 RPC
    RPCResponse process_rpc(const RPCRequest& req, const std::string& client_id);
    
    // 处理具体方法
    RPCResponse handle_connect(const RPCRequest& req, const std::string& client_id);
    RPCResponse handle_agent(const RPCRequest& req, const std::string& client_id);
    RPCResponse handle_send(const RPCRequest& req, const std::string& client_id);
    RPCResponse handle_status(const RPCRequest& req);
    RPCResponse handle_sessions_list(const RPCRequest& req);
    
    // Agent 工作线程
    void agent_worker();
    
    // 发送响应
    void send_response(const std::string& client_id, const RPCResponse& response);
    
    // 发送事件
    void send_event(const std::string& client_id, const std::string& event, 
                    const std::map<std::string, std::string>& payload);
};

} // namespace openclaw

#endif // OPENCLAW_GATEWAY_H
