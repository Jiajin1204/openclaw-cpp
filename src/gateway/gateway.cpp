#include "gateway.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <chrono>
#include <thread>

namespace openclaw {

// ============ RPCRequest ============

RPCRequest RPCRequest::from_json(const std::string& json_str) {
    RPCRequest req;
    
    // 简化 JSON 解析
    // 实际实现应该用完整的 JSON 解析器
    
    // 查找 method
    size_t method_pos = json_str.find("\"method\"");
    if (method_pos != std::string::npos) {
        size_t colon = json_str.find(":", method_pos);
        size_t quote1 = json_str.find("\"", colon);
        size_t quote2 = json_str.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            req.method = json_str.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    // 查找 id
    size_t id_pos = json_str.find("\"id\"");
    if (id_pos != std::string::npos) {
        size_t colon = json_str.find(":", id_pos);
        size_t quote1 = json_str.find("\"", colon);
        size_t quote2 = json_str.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            req.id = json_str.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    // 查找 params
    size_t params_pos = json_str.find("\"params\"");
    if (params_pos != std::string::npos) {
        size_t brace1 = json_str.find("{", params_pos);
        size_t brace2 = json_str.rfind("}");
        if (brace1 != std::string::npos && brace2 != std::string::npos && brace2 > brace1) {
            std::string params_str = json_str.substr(brace1, brace2 - brace1);
            
            // 简单解析 key-value
            size_t pos = 0;
            while (pos < params_str.size()) {
                size_t key_start = params_str.find("\"", pos);
                if (key_start == std::string::npos) break;
                
                size_t key_end = params_str.find("\"", key_start + 1);
                if (key_end == std::string::npos) break;
                
                std::string key = params_str.substr(key_start + 1, key_end - key_start - 1);
                
                size_t colon = params_str.find(":", key_end);
                if (colon == std::string::npos) break;
                
                size_t value_start = params_str.find("\"", colon);
                if (value_start == std::string::npos) break;
                
                size_t value_end = params_str.find("\"", value_start + 1);
                if (value_end == std::string::npos) break;
                
                std::string value = params_str.substr(value_start + 1, value_end - value_start - 1);
                req.params[key] = value;
                
                pos = value_end + 1;
            }
        }
    }
    
    return req;
}

// ============ RPCResponse ============

std::string RPCResponse::to_json() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"res\",";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false");
    
    if (!error.empty()) {
        oss << ",\"error\":\"" << error << "\"";
    }
    
    if (!payload.empty() && ok) {
        oss << ",\"payload\":{";
        bool first = true;
        for (const auto& [k, v] : payload) {
            if (!first) oss << ",";
            oss << "\"" << k << "\":\"" << v << "\"";
            first = false;
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

// ============ Gateway ============

Gateway::Gateway() 
    : ws_server_(nullptr)
    , session_manager_(nullptr)
    , tool_engine_(nullptr)
    , agent_loop_(nullptr)
{
}

Gateway::~Gateway() {
    stop();
}

bool Gateway::start() {
    if (running_.load()) {
        return true;
    }
    
    // 加载配置
    config_ = ConfigManager::instance().get();
    
    // 初始化组件
    session_manager_ = std::make_unique<SessionManager>();
    tool_engine_ = std::make_unique<ToolEngine>();
    agent_loop_ = std::make_unique<AgentLoop>(
        session_manager_.get(), 
        tool_engine_.get()
    );
    
    // 初始化 WebSocket 服务器
    ws_server_ = std::make_unique<WSServer>(
        config_.gateway.host,
        config_.gateway.port
    );
    
    // 设置消息处理
    ws_server_->set_message_handler([this](const std::string& client_id, const WSMessage& msg) {
        if (msg.type == WSMessage::Type::Text) {
            handle_message(client_id, msg.data);
        }
    });
    
    ws_server_->set_disconnect_handler([this](const std::string& client_id) {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_sessions_.erase(client_id);
    });
    
    // 启动 WebSocket 服务器
    if (!ws_server_->start()) {
        LOG_ERROR("Failed to start WebSocket server");
        return false;
    }
    
    // 启动 Agent 工作线程
    running_ = true;
    worker_thread_ = std::thread([this]() {
        agent_worker();
    });
    
    LOG_INFO("Gateway started successfully");
    return true;
}

void Gateway::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    queue_cv_.notify_all();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    if (ws_server_) {
        ws_server_->stop();
    }
    
    LOG_INFO("Gateway stopped");
}

bool Gateway::is_running() const {
    return running_.load();
}

void Gateway::set_config(const Config& config) {
    config_ = config;
}

void Gateway::handle_message(const std::string& client_id, const std::string& message) {
    // 解析 RPC 请求
    RPCRequest req = RPCRequest::from_json(message);
    
    if (req.method.empty()) {
        LOG_WARN("Invalid RPC request from ", client_id);
        return;
    }
    
    LOG_DEBUG("RPC request: ", req.method, " from ", client_id);
    
    // 处理请求
    RPCResponse response = process_rpc(req, client_id);
    
    // 发送响应
    send_response(client_id, response);
}

RPCResponse Gateway::process_rpc(const RPCRequest& req, const std::string& client_id) {
    try {
        if (req.method == "connect") {
            return handle_connect(req, client_id);
        } else if (req.method == "agent") {
            return handle_agent(req, client_id);
        } else if (req.method == "send") {
            return handle_send(req, client_id);
        } else if (req.method == "status") {
            return handle_status(req);
        } else if (req.method == "sessions.list") {
            return handle_sessions_list(req);
        } else {
            return RPCResponse{req.id, false, "Unknown method: " + req.method, {}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("RPC error: ", e.what());
        return RPCResponse{req.id, false, e.what(), {}};
    }
}

RPCResponse Gateway::handle_connect(const RPCRequest& req, const std::string& client_id) {
    // 验证 token（如果配置了）
    std::string token = req.params.count("token") ? req.params.at("token") : "";
    if (!config_.gateway.token.empty() && token != config_.gateway.token) {
        return RPCResponse{req.id, false, "Invalid token", {}};
    }
    
    // 关联客户端到默认会话
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_sessions_[client_id] = "agent:main:main";
    }
    
    LOG_INFO("Client connected: ", client_id);
    
    return RPCResponse{req.id, true, "", {
        {"hello", "ok"},
        {"sessionKey", "agent:main:main"}
    }};
}

RPCResponse Gateway::handle_agent(const RPCRequest& req, const std::string& client_id) {
    std::string message = req.params.count("message") ? req.params.at("message") : "";
    std::string session_key = req.params.count("sessionKey") 
        ? req.params.at("sessionKey") 
        : "agent:main:main";
    
    if (message.empty()) {
        return RPCResponse{req.id, false, "Missing message parameter", {}};
    }
    
    // 将任务加入队列
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        agent_queue_.push({client_id, session_key, message, req.id});
    }
    queue_cv_.notify_one();
    
    // 立即返回 accepted
    return RPCResponse{req.id, true, "", {
        {"status", "accepted"},
        {"runId", client_id + "_" + std::to_string(std::time(nullptr))}
    }};
}

RPCResponse Gateway::handle_send(const RPCRequest& req, const std::string& client_id) {
    // 简化实现：直接返回成功
    return RPCResponse{req.id, true, "", {
        {"sent", "true"}
    }};
}

RPCResponse Gateway::handle_status(const RPCRequest& req) {
    std::ostringstream oss;
    oss << "running=" << (running_.load() ? "true" : "false")
        << ",clients=" << 0
        << ",sessions=" << (session_manager_ ? std::to_string(session_manager_->count()) : "0");
    
    return RPCResponse{req.id, true, "", {
        {"status", oss.str()}
    }};
}

RPCResponse Gateway::handle_sessions_list(const RPCRequest& req) {
    if (!session_manager_) {
        return RPCResponse{req.id, false, "Session manager not initialized", {}};
    }
    
    auto sessions = session_manager_->list_sessions();
    
    std::ostringstream oss;
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << sessions[i];
    }
    
    return RPCResponse{req.id, true, "", {
        {"sessions", oss.str()}
    }};
}

void Gateway::agent_worker() {
    while (running_.load()) {
        AgentTask task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                return !agent_queue_.empty() || !running_.load();
            });
            
            if (!running_.load() || agent_queue_.empty()) {
                continue;
            }
            
            task = agent_queue_.front();
            agent_queue_.pop();
        }
        
        LOG_INFO("Processing agent task for client: ", task.client_id);
        
        // 运行 Agent
        if (agent_loop_) {
            std::string response = agent_loop_->run(task.session_key, task.message);
            
            // 发送响应
            RPCResponse res;
            res.id = task.request_id;
            res.ok = true;
            res.payload["content"] = response;
            
            send_response(task.client_id, res);
        }
    }
}

void Gateway::send_response(const std::string& client_id, const RPCResponse& response) {
    if (ws_server_) {
        ws_server_->send_to(client_id, response.to_json());
    }
}

void Gateway::send_event(const std::string& client_id, const std::string& event,
                         const std::map<std::string, std::string>& payload) {
    std::ostringstream oss;
    oss << "{\"type\":\"event\",\"event\":\"" << event << "\",\"payload\":{";
    bool first = true;
    for (const auto& [k, v] : payload) {
        if (!first) oss << ",";
        oss << "\"" << k << "\":\"" << v << "\"";
        first = false;
    }
    oss << "}}";
    
    if (ws_server_) {
        ws_server_->send_to(client_id, oss.str());
    }
}

std::string Gateway::status() const {
    std::ostringstream oss;
    oss << "Gateway: " << (running_.load() ? "running" : "stopped") << "\n";
    oss << "WebSocket: " << (ws_server_ && ws_server_->is_running() ? "running" : "stopped") << "\n";
    return oss.str();
}

} // namespace openclaw
