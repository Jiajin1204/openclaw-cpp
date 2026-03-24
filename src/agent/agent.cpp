/**
 * @file agent.cpp
 * @brief Agent模块实现
 */

#include "agent.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <curl/curl.h>
#include <cstdlib>

namespace openclaw {

// ===== ModelClient =====

static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static bool g_curl_init = []() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return true;
}();

ModelClient::ModelClient() {
    const auto& config = ConfigManager::instance().get();
    api_key_ = config.model.api_key;
    model_ = config.model.model;
    base_url_ = config.model.base_url;
    
    // 从环境变量覆盖 - 支持多种格式
    const char* env_keys[] = {
        "OPENCLAW_MODEL_API_KEY",
        "MINIMAX_API_KEY", 
        "MINIMAX_APIKEY",
        nullptr
    };
    
    for (int i = 0; env_keys[i] != nullptr; i++) {
        const char* env_key = std::getenv(env_keys[i]);
        if (env_key) {
            api_key_ = env_key;
            break;
        }
    }
    
    const char* env_url = std::getenv("OPENCLAW_MODEL_BASE_URL");
    const char* env_model = std::getenv("OPENCLAW_MODEL_NAME");
    
    if (env_url) base_url_ = env_url;
    if (env_model) model_ = env_model;
    
    LOG_INFO("Model client: ", model_, " @ ", base_url_);
    if (!api_key_.empty()) {
        LOG_INFO("API key loaded: ", api_key_.substr(0, 10), "...");
    } else {
        LOG_WARN("No API key found!");
    }
}

void ModelClient::set_api_key(const std::string& key) { api_key_ = key; }
void ModelClient::set_model(const std::string& model) { model_ = model; }

std::string ModelClient::http_post(const std::string& url, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return "{\"error\": \"curl init failed\"}";
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key_.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key_).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response;
}

ChatResponse ModelClient::chat(const std::vector<Message>& messages, const std::string& tools_json) {
    ChatResponse response;
    
    if (api_key_.empty()) {
        response.content = "Hello! I'm OpenClaw C++. How can I help you today? (No API key - set MINIMAX_API_KEY)";
        response.finish_reason = "stop";
        return response;
    }
    
    // 构建请求体 - MiniMax 格式
    std::string body = "{\"model\":\"" + model_ + "\",\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) body += ",";
        // 转义 content 中的特殊字符
        std::string content = messages[i].content;
        std::string escaped;
        for (char c : content) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else escaped += c;
        }
        body += "{\"role\":\"" + role_to_string(messages[i].role) + "\",\"content\":\"" + escaped + "\"}";
    }
    body += "]";
    
    // 添加工具定义（如果有）
    if (!tools_json.empty()) {
        body += ",\"tools\":" + tools_json;
        body += ",\"tool_choice\":\"auto\"";
    }
    
    body += ",\"max_tokens\":8192}";
    
    LOG_DEBUG("Request body: ", body.substr(0, 200), "...");
    
    std::string resp = http_post(base_url_, body);
    
    LOG_DEBUG("Response: ", resp.substr(0, 500));
    
    // 解析 MiniMax 响应
    // 检查是否有 tool_calls
    size_t tool_pos = resp.find("\"tool_calls\"");
    if (tool_pos != std::string::npos) {
        // 提取工具调用
        size_t fn_start = resp.find("\"function\"", tool_pos);
        if (fn_start != std::string::npos) {
            size_t name_start = resp.find("\"name\"", fn_start);
            size_t args_start = resp.find("\"arguments\"", fn_start);
            if (name_start != std::string::npos && args_start != std::string::npos) {
                size_t name_q1 = resp.find("\"", name_start + 7);
                size_t name_q2 = resp.find("\"", name_q1 + 1);
                size_t args_q1 = resp.find("\"", args_start + 11);
                size_t args_q2 = resp.find("}", args_q1); // arguments 是 JSON 对象
                
                if (name_q1 != std::string::npos && name_q2 != std::string::npos &&
                    args_q1 != std::string::npos && args_q2 != std::string::npos) {
                    response.tool_name = resp.substr(name_q1 + 1, name_q2 - name_q1 - 1);
                    response.tool_args = resp.substr(args_q1 + 1, args_q2 - args_q1);
                    response.finish_reason = "tool_calls";
                    LOG_INFO("Tool call detected: ", response.tool_name, "(", response.tool_args, ")");
                    return response;
                }
            }
        }
    }
    
    // 解析普通 content
    size_t pos = resp.find("\"content\"");
    if (pos != std::string::npos) {
        size_t q1 = resp.find("\"", pos + 9);
        size_t q2 = resp.find("\"", q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
            response.content = resp.substr(q1 + 1, q2 - q1 - 1);
            response.finish_reason = "stop";
        }
    } else if (resp.find("error") != std::string::npos) {
        response.content = "Error: " + resp;
        response.finish_reason = "error";
    }
    
    return response;
}

// ===== AgentLoop =====

AgentLoop::AgentLoop(IModelClient* model_client, IToolEngine* tool_engine)
    : model_client_(model_client), tool_engine_(tool_engine)
{
    system_prompt_ = "You are OpenClaw C++, a helpful AI assistant.";
}

void AgentLoop::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

std::vector<Message> AgentLoop::build_messages(
    const std::string& session_key,
    const std::string& user_input
) {
    std::vector<Message> messages;
    messages.push_back(Message(MessageRole::System, system_prompt_));
    
    // 获取会话历史
    auto* sm = &SessionManager::instance();
    auto* session = sm->get_session(session_key);
    auto history = session->get_history(10);
    for (const auto& msg : history) {
        messages.push_back(msg);
    }
    
    messages.push_back(Message(MessageRole::User, user_input));
    return messages;
}

// 转换工具定义为 JSON 格式
static std::string tools_to_json(IToolEngine* engine) {
    auto tools = engine->list_tools();
    if (tools.empty()) return "";
    
    // 简化版本 - 不传 parameters
    std::string json = "[";
    for (size_t i = 0; i < tools.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"type\":\"function\",\"function\":{";
        json += "\"name\":\"" + tools[i].name + "\",";
        json += "\"description\":\"" + tools[i].description + "\"";
        json += "}}";
    }
    json += "]";
    
    // 修复: 去除多余的 { }
    // 重新生成正确的格式
    json = "[";
    for (size_t i = 0; i < tools.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"type\":\"function\",\"function\":{";
        json += "\"name\":\"" + tools[i].name + "\",";
        json += "\"description\":\"" + tools[i].description + "\",";
        json += "\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}}}";
    }
    json += "]";
    return json;
}

std::string AgentLoop::run(const std::string& session_key, const std::string& user_input) {
    auto* sm = &SessionManager::instance();
    auto* session = sm->get_session(session_key);
    
    // 添加用户消息到会话
    session->add_message(Message(MessageRole::User, user_input));
    
    // 获取工具定义
    std::string tools_json = tools_to_json(tool_engine_);
    
    // Agent Loop: 最多 5 轮工具调用
    const int max_loops = 5;
    for (int loop = 0; loop < max_loops; ++loop) {
        auto messages = build_messages(session_key, "");
        
        // 调用模型
        auto response = model_client_->chat(messages, tools_json);
        
        // 检查是否是工具调用
        if (response.finish_reason == "tool_calls" && !response.tool_name.empty()) {
            LOG_INFO("Executing tool: ", response.tool_name, " with args: ", response.tool_args);
            
            // 解析工具参数 - 提取 "name" 字段的值
            std::string tool_params = response.tool_args;
            
            // 尝试多种格式解析
            size_t name_pos = tool_params.find("name");
            if (name_pos != std::string::npos && name_pos < tool_params.size()) {
                // 找冒号后的值
                size_t colon = tool_params.find(":", name_pos);
                if (colon != std::string::npos) {
                    // 找第一个引号
                    size_t q1 = tool_params.find("\"", colon);
                    // 找第二个引号
                    size_t q2 = tool_params.find("\"", q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos) {
                        tool_params = tool_params.substr(q1 + 1, q2 - q1 - 1);
                        LOG_INFO("Parsed tool params: ", tool_params);
                    }
                }
            }
            
            // 执行工具
            std::string tool_result = tool_engine_->execute(response.tool_name, tool_params);
            LOG_INFO("Tool result: ", tool_result.substr(0, 100), "...");
            
            // 添加工具结果到消息
            std::string result_msg = "Tool " + response.tool_name + " returned: " + tool_result;
            session->add_message(Message(MessageRole::Assistant, response.content));
            session->add_message(Message(MessageRole::User, result_msg));
            
            // 继续循环，让模型处理工具结果
            continue;
        }
        
        // 普通响应
        session->add_message(Message(MessageRole::Assistant, response.content));
        return response.content;
    }
    
    return "Max tool loops reached.";
}

} // namespace openclaw
