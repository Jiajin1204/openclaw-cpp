#include "model_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include "../utils/config.h"
#include <sstream>
#include <curl/curl.h>
#include <thread>

// CURL 回调
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

namespace openclaw {

// 全局 CURL 初始化标记
bool g_curl_initialized = false;

ModelClient::ModelClient() {
    // 初始化 CURL（只执行一次）
    if (!g_curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_initialized = true;
    }
    
    const auto& config = ConfigManager::instance().get();
    api_key_ = config.model.api_key;
    base_url_ = config.model.base_url;
    model_ = config.model.model;
}

ModelClient::~ModelClient() {
}

void ModelClient::set_api_key(const std::string& key) {
    api_key_ = key;
}

void ModelClient::set_base_url(const std::string& url) {
    base_url_ = url;
}

void ModelClient::set_model(const std::string& model) {
    model_ = model;
}

// HTTP POST 请求
Result<std::string> ModelClient::http_post(
    const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return Result<std::string>::failure("Failed to init CURL");
    }
    
    std::string response;
    response.reserve(8192);  // 预分配避免多次扩容
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    struct curl_slist* header_list = nullptr;
    header_list = curl_slist_append(header_list, "Content-Type: application/json");
    
    // 添加认证头
    if (!api_key_.empty() && api_key_.length() > 5) {
        std::string auth = "Authorization: Bearer " + api_key_;
        header_list = curl_slist_append(header_list, auth.c_str());
    }
    
    for (const auto& [key, value] : headers) {
        std::string h = key + ": " + value;
        header_list = curl_slist_append(header_list, h.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return Result<std::string>::failure(std::string("CURL error: ") + curl_easy_strerror(res));
    }
    
    return Result<std::string>::success(response);
}

// 构建请求体
std::string ModelClient::build_request_body(
    const std::vector<Message>& messages,
    bool stream
) const {
    nlohmann::json j;
    j["model"] = model_;
    j["stream"] = stream;
    
    // 添加工具定义（暂时禁用，避免错误）
    /*
    if (tool_engine_ != nullptr) {
        try {
            std::vector<Tool> tools = tool_engine_->list_tools();
            if (!tools.empty()) {
                nlohmann::json tarr = nlohmann::json::array();
                for (const auto& t : tools) {
                    nlohmann::json func;
                    func["name"] = t.name;
                    func["description"] = t.description;
                    func["parameters"] = nlohmann::json::object();  // 简化参数
                    
                    nlohmann::json tool_def;
                    tool_def["type"] = "function";
                    tool_def["function"] = func;
                    tarr.push_back(tool_def);
                }
                j["tools"] = tarr;
            }
        } catch (const std::exception& e) {
            LOG_WARN("Failed to build tools: ", e.what());
        }
    }
    */
    
    // 构建消息列表
    nlohmann::json msgs = nlohmann::json::array();
    for (const auto& m : messages) {
        nlohmann::json msg;
        msg["role"] = role_to_string(m.role);
        
        if (!m.content.empty()) {
            msg["content"] = m.content;
        }
        
        if (m.tool_call_id && !m.tool_call_id->empty()) {
            msg["tool_call_id"] = *m.tool_call_id;
        }
        if (m.name && !m.name->empty()) {
            msg["name"] = *m.name;
        }
        
        msgs.push_back(msg);
    }
    j["messages"] = msgs;
    
    return j.dump();
}

// 解析响应
ModelClient::ChatResponse ModelClient::parse_response(const std::string& response_body) const {
    ChatResponse response;
    
    try {
        auto j = nlohmann::json::parse(response_body);
        
        // 检查 API 错误响应
        if (j.contains("base_resp") && j["base_resp"].contains("status_code")) {
            int status_code = j["base_resp"]["status_code"];
            if (status_code != 0) {
                std::string error_msg = j["base_resp"].value("status_msg", "Unknown error");
                response.content = "API Error: " + error_msg;
                return response;
            }
        }
        
        // 检查 error 字段
        if (j.contains("error")) {
            response.content = "Error: " + j["error"].dump();
            return response;
        }
        
        // ===== Anthropic 格式 =====
        if (j.contains("content") && j["content"].is_array()) {
            for (auto& item : j["content"]) {
                if (item.contains("text")) {
                    response.content += item["text"].get<std::string>();
                }
            }
            if (j.contains("stop_reason")) {
                response.finish_reason = j["stop_reason"].get<std::string>();
            }
            return response;
        }
        
        // ===== OpenAI 格式 =====
        // 提取 content 和 tool_calls
        if (j.contains("choices") && !j["choices"].empty()) {
            auto& choice = j["choices"][0];
            
            if (choice.contains("message")) {
                auto& msg = choice["message"];
                
                if (msg.contains("content") && !msg["content"].is_null()) {
                    response.content = msg["content"];
                }
                
                // 标准格式 tool_calls
                if (msg.contains("tool_calls") && !msg["tool_calls"].empty()) {
                    for (auto& tc : msg["tool_calls"]) {
                        ToolCall call;
                        if (tc.contains("id")) call.id = tc["id"];
                        if (tc.contains("function")) {
                            auto& func = tc["function"];
                            if (func.contains("name")) call.name = func["name"];
                            if (func.contains("arguments")) {
                                if (func["arguments"].is_string()) {
                                    call.arguments = func["arguments"];
                                } else {
                                    call.arguments = func["arguments"].dump();
                                }
                            }
                        }
                        response.tool_calls.push_back(call);
                    }
                }
                
                // 自定义格式：某些模型把 tool_calls 放在 content 中
                if (response.tool_calls.empty() && !response.content.empty()) {
                    std::string& content = response.content;
                    size_t start = content.find("[TOOL_CALL]");
                    if (start != std::string::npos) {
                        size_t end = content.find("[/TOOL_CALL]", start);
                        if (end != std::string::npos) {
                            std::string tool_str = content.substr(start + 11, end - start - 11);
                            
                            // 解析 {tool => "xxx", args => {...}}
                            size_t tool_pos = tool_str.find("tool => \"");
                            if (tool_pos != std::string::npos) {
                                ToolCall call;
                                size_t name_start = tool_pos + 9;
                                size_t name_end = tool_str.find("\"", name_start);
                                if (name_end != std::string::npos && name_end > name_start) {
                                    call.name = tool_str.substr(name_start, name_end - name_start);
                                    call.id = "call_" + call.name + "_manual";
                                }
                                
                                size_t args_start = tool_str.find("args => ", name_end);
                                if (args_start != std::string::npos) {
                                    args_start += 8;
                                    call.arguments = tool_str.substr(args_start);
                                }
                                
                                if (!call.name.empty()) {
                                    response.tool_calls.push_back(call);
                                }
                            }
                        }
                    }
                }
            }
            
            if (choice.contains("finish_reason")) {
                response.finish_reason = choice["finish_reason"];
            }
        }
        
        // 提取 usage
        if (j.contains("usage")) {
            auto& usage = j["usage"];
            if (usage.contains("prompt_tokens")) response.input_tokens = usage["prompt_tokens"];
            if (usage.contains("completion_tokens")) response.output_tokens = usage["completion_tokens"];
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse response: ", e.what());
        response.content = response_body;
    }
    
    return response;
}

// 聊天（非流式）
Result<ModelClient::ChatResponse> ModelClient::chat(const std::vector<Message>& messages) {
    if (api_key_.empty()) {
        ChatResponse mock_response;
        mock_response.content = "Hello! I'm OpenClaw C++. How can I help you today?";
        mock_response.finish_reason = "stop";
        return Result<ChatResponse>::success(mock_response);
    }
    
    // 兼容 Anthropic API 路径
    std::string url = base_url_;
    LOG_INFO("ModelClient::chat - base_url: ", base_url_);
    LOG_INFO("ModelClient::chat - api_key length: ", (int)api_key_.length());
    if (url.find("/chat/completions") != std::string::npos) {
        url = url.replace(url.find("/chat/completions"), 17, "/anthropic/v1/messages");
    } else if (url.find("/anthropic") != std::string::npos && url.find("/messages") == std::string::npos) {
        url = url + "/v1/messages";
    } else if (url.find("/v1") == std::string::npos) {
        url = url + "/anthropic/v1/messages";
    }
    LOG_INFO("ModelClient::chat - final url: ", url);
    
    std::string body = build_request_body(messages, false);
    
    auto result = http_post(url, body);
    
    if (!result.ok) {
        return Result<ChatResponse>::failure(result.error);
    }
    
    ChatResponse response = parse_response(result.value);
    return Result<ChatResponse>::success(response);
}

// 聊天（流式）- 模拟实现：先获取完整响应，再逐块回调
ResultVoid ModelClient::chat_stream(
    const std::vector<Message>& messages,
    StreamCallback on_chunk
) {
    if (api_key_.empty()) {
        return ResultVoid::failure("No API key configured");
    }
    
    // 兼容 Anthropic API 路径
    std::string url = base_url_;
    if (url.find("/chat/completions") != std::string::npos) {
        url = url.replace(url.find("/chat/completions"), 17, "/anthropic/v1/messages");
    } else if (url.find("/anthropic") != std::string::npos && url.find("/messages") == std::string::npos) {
        url = url + "/v1/messages";
    } else if (url.find("/v1") == std::string::npos) {
        url = url + "/anthropic/v1/messages";
    }
    
    // 使用非流式请求获取完整响应
    auto result = chat(messages);
    
    if (!result.ok) {
        return ResultVoid::failure(result.error);
    }
    
    // 模拟流式：按字符（而非字节）逐块回调，避免UTF-8中文乱码
    const std::string& content = result.value.content;
    const size_t chunk_size = 10;  // 每10个字符一块
    
    size_t i = 0;
    while (i < content.size()) {
        // 计算这块的结束位置
        size_t end = std::min(i + chunk_size, content.size());
        
        // 如果结束位置不是字符边界（UTF-8中文字符3字节），往回找
        while (end > i && (content[end] & 0xC0) == 0x80) {
            end--;
        }
        
        if (end <= i) end = std::min(i + 1, content.size());
        
        std::string chunk = content.substr(i, end - i);
        on_chunk(chunk);
        
        i = end;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    return ResultVoid::success(true);
}

} // namespace openclaw