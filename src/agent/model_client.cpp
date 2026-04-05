/**
 * @file model_client.cpp
 * @brief 通用 Model Client - 支持多种 API 格式
 */

#include "model_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include "../utils/config.h"
#include <sstream>
#include <curl/curl.h>
#include <thread>

namespace openclaw {

// ============================================================================
// CURL 辅助函数
// ============================================================================

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ============================================================================
// HTTP 客户端
// ============================================================================

struct HttpClient {
    std::string api_key;
    std::string base_url;
    
    HttpClient(const std::string& url = "", const std::string& key = "") 
        : base_url(url), api_key(key) {}
    
    void set_url(const std::string& url) { base_url = url; }
    void set_key(const std::string& key) { api_key = key; }
    
    // 通用 POST 请求
    Result<std::string> post(const std::string& path, const std::string& body,
                            const std::map<std::string, std::string>& extra_headers = {}) {
        std::string url = base_url;
        if (!path.empty() && path[0] != '/') url += "/";
        url += path;
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            return Result<std::string>::failure("Failed to init CURL");
        }
        
        std::string response;
        response.reserve(8192);
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        struct curl_slist* header_list = nullptr;
        header_list = curl_slist_append(header_list, "Content-Type: application/json");
        
        if (!api_key.empty()) {
            std::string auth = "Authorization: Bearer " + api_key;
            header_list = curl_slist_append(header_list, auth.c_str());
        }
        
        for (const auto& [k, v] : extra_headers) {
            std::string h = k + ": " + v;
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
    
    // 流式 POST 请求
    ResultVoid post_stream(const std::string& path, const std::string& body,
                          StreamCallback on_chunk,
                          const std::map<std::string, std::string>& extra_headers = {}) {
        std::string url = base_url;
        if (!path.empty() && path[0] != '/') url += "/";
        url += path;
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            return ResultVoid::failure("Failed to init CURL");
        }
        
        std::string buffer;
        buffer.reserve(8192);
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        // 流式回调：实时处理收到的数据
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
            [](char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                std::string* buf = (std::string*)userp;
                buf->append(contents, size * nmemb);
                return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        
        struct curl_slist* header_list = nullptr;
        header_list = curl_slist_append(header_list, "Content-Type: application/json");
        header_list = curl_slist_append(header_list, "Accept: text/event-stream");
        
        if (!api_key.empty()) {
            std::string auth = "Authorization: Bearer " + api_key;
            header_list = curl_slist_append(header_list, auth.c_str());
        }
        
        for (const auto& [k, v] : extra_headers) {
            std::string h = k + ": " + v;
            header_list = curl_slist_append(header_list, h.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return ResultVoid::failure(std::string("CURL error: ") + curl_easy_strerror(res));
        }
        
        // 解析 SSE 流
        on_chunk(buffer);
        
        return ResultVoid::success(true);
    }
};

// ============================================================================
// API 适配器基类
// ============================================================================

struct ApiAdapter {
    virtual ~ApiAdapter() = default;
    
    // 获取 API 路径（相对于 base_url）
    virtual std::string get_path(bool stream) const = 0;
    
    // 构建请求体
    virtual std::string build_body(const std::string& model, 
                                    const std::vector<Message>& messages,
                                    bool stream) const = 0;
    
    // 解析非流式响应
    virtual ModelClient::ChatResponse parse_response(const std::string& body) const = 0;
    
    // 解析流式响应块
    virtual std::vector<std::string> parse_stream_chunk(const std::string& line) const = 0;
};

// ============================================================================
// OpenAI 格式适配器
// ============================================================================

struct OpenAiAdapter : ApiAdapter {
    std::string get_path(bool stream) const override {
        return "/v1/chat/completions";
    }
    
    std::string build_body(const std::string& model,
                          const std::vector<Message>& messages,
                          bool stream) const override {
        nlohmann::json j;
        j["model"] = model;
        j["stream"] = stream;
        
        nlohmann::json msgs = nlohmann::json::array();
        for (const auto& m : messages) {
            nlohmann::json msg;
            msg["role"] = role_to_string(m.role);
            if (!m.content.empty()) msg["content"] = m.content;
            msgs.push_back(msg);
        }
        j["messages"] = msgs;
        
        return j.dump();
    }
    
    ModelClient::ChatResponse parse_response(const std::string& body) const override {
        ModelClient::ChatResponse response;
        try {
            auto j = nlohmann::json::parse(body);
            
            if (j.contains("error")) {
                response.content = "Error: " + j["error"].dump();
                return response;
            }
            
            if (j.contains("choices") && !j["choices"].empty()) {
                auto& choice = j["choices"][0];
                if (choice.contains("message")) {
                    auto& msg = choice["message"];
                    if (msg.contains("content") && !msg["content"].is_null()) {
                        response.content = msg["content"].get<std::string>();
                    }
                }
                if (choice.contains("finish_reason")) {
                    response.finish_reason = choice["finish_reason"].get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("OpenAI parse error: ", e.what());
            response.content = body;
        }
        return response;
    }
    
    std::vector<std::string> parse_stream_chunk(const std::string& line) const override {
        std::vector<std::string> chunks;
        
        // OpenAI SSE: "data: {...}\n\n"
        size_t pos = 0;
        while (pos < line.size()) {
            size_t data_start = line.find("data:", pos);
            if (data_start == std::string::npos) break;
            
            data_start += 5; // skip "data:"
            while (data_start < line.size() && line[data_start] == ' ') data_start++;
            
            size_t line_end = line.find('\n', data_start);
            if (line_end == std::string::npos) line_end = line.size();
            
            std::string json_str = line.substr(data_start, line_end - data_start);
            if (json_str == "[DONE]") return chunks;
            
            try {
                auto j = nlohmann::json::parse(json_str);
                if (j.contains("choices") && !j["choices"].empty()) {
                    auto& choice = j["choices"][0];
                    if (choice.contains("delta") && choice["delta"].contains("content")) {
                        chunks.push_back(choice["delta"]["content"].get<std::string>());
                    }
                }
            } catch (...) {}
            
            pos = line_end + 1;
        }
        
        return chunks;
    }
};

// ============================================================================
// Anthropic 格式适配器
// ============================================================================

struct AnthropicAdapter : ApiAdapter {
    std::string get_path(bool stream) const override {
        return "/v1/messages";
    }
    
    std::string build_body(const std::string& model,
                          const std::vector<Message>& messages,
                          bool stream) const override {
        nlohmann::json j;
        j["model"] = model;
        j["stream"] = stream;
        
        nlohmann::json msgs = nlohmann::json::array();
        for (const auto& m : messages) {
            nlohmann::json msg;
            msg["role"] = role_to_string(m.role);
            if (!m.content.empty()) msg["content"] = m.content;
            msgs.push_back(msg);
        }
        j["messages"] = msgs;
        
        return j.dump();
    }
    
    ModelClient::ChatResponse parse_response(const std::string& body) const override {
        ModelClient::ChatResponse response;
        try {
            auto j = nlohmann::json::parse(body);
            
            if (j.contains("type") && j["type"] == "error") {
                response.content = "Error: " + j.value("message", "unknown error");
                return response;
            }
            
            if (j.contains("content") && j["content"].is_array()) {
                for (auto& item : j["content"]) {
                    if (item.contains("text")) {
                        response.content += item["text"].get<std::string>();
                    }
                }
            }
            
            if (j.contains("stop_reason")) {
                response.finish_reason = j["stop_reason"].get<std::string>();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Anthropic parse error: ", e.what());
            response.content = body;
        }
        return response;
    }
    
    std::vector<std::string> parse_stream_chunk(const std::string& line) const override {
        std::vector<std::string> chunks;
        
        // Anthropic SSE: "data: {...}\n\n"
        size_t pos = 0;
        while (pos < line.size()) {
            size_t data_start = line.find("data:", pos);
            if (data_start == std::string::npos) break;
            
            data_start += 5;
            while (data_start < line.size() && line[data_start] == ' ') data_start++;
            
            size_t line_end = line.find('\n', data_start);
            if (line_end == std::string::npos) line_end = line.size();
            
            std::string json_str = line.substr(data_start, line_end - data_start);
            if (json_str == "[DONE]") return chunks;
            
            try {
                auto j = nlohmann::json::parse(json_str);
                std::string type = j.value("type", "");
                
                if (type == "content_block_delta") {
                    if (j.contains("delta") && j["delta"].contains("text")) {
                        chunks.push_back(j["delta"]["text"].get<std::string>());
                    }
                }
            } catch (...) {}
            
            pos = line_end + 1;
        }
        
        return chunks;
    }
};

// ============================================================================
// Model Client
// ============================================================================

// 全局 CURL 初始化标记
static bool g_curl_initialized = false;

ModelClient::ModelClient() {
    if (!g_curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_initialized = true;
    }
    
    const auto& config = ConfigManager::instance().get();
    api_key_ = config.model.api_key;
    base_url_ = config.model.base_url;
    model_ = config.model.model;
    
    // 根据 base_url 自动选择适配器
    if (base_url_.find("anthropic") != std::string::npos) {
        adapter_ = std::make_unique<AnthropicAdapter>();
    } else if (base_url_.find("openai") != std::string::npos ||
               base_url_.find("dashscope") != std::string::npos ||
               base_url_.find("aliyun") != std::string::npos) {
        adapter_ = std::make_unique<OpenAiAdapter>();
    } else {
        // 默认 OpenAI 兼容
        adapter_ = std::make_unique<OpenAiAdapter>();
    }
}

ModelClient::~ModelClient() {
}

void ModelClient::set_api_key(const std::string& key) { api_key_ = key; }
void ModelClient::set_base_url(const std::string& url) { base_url_ = url; }
void ModelClient::set_model(const std::string& model) { model_ = model; }
void ModelClient::set_tool_engine(ToolEngine* engine) { tool_engine_ = engine; }

// 非流式聊天
Result<ModelClient::ChatResponse> ModelClient::chat(const std::vector<Message>& messages) {
    if (api_key_.empty()) {
        ChatResponse mock_response;
        mock_response.content = "Hello! I'm OpenClaw C++. How can I help you today?";
        mock_response.finish_reason = "stop";
        return Result<ChatResponse>::success(mock_response);
    }
    
    HttpClient client(base_url_, api_key_);
    std::string path = adapter_->get_path(false);
    std::string body = adapter_->build_body(model_, messages, false);
    
    auto result = client.post(path, body);
    if (!result.ok) {
        return Result<ChatResponse>::failure(result.error);
    }
    
    ChatResponse response = adapter_->parse_response(result.value);
    return Result<ChatResponse>::success(response);
}

// 流式聊天 - 使用模拟流式（非流式获取 + 逐块输出）
ResultVoid ModelClient::chat_stream(
    const std::vector<Message>& messages,
    StreamCallback on_chunk
) {
    if (api_key_.empty()) {
        return ResultVoid::failure("No API key configured");
    }
    
    // 使用非流式获取完整响应
    auto non_stream = chat(messages);
    if (!non_stream.ok) {
        return ResultVoid::failure(non_stream.error);
    }
    
    // 模拟流式：UTF-8 友好分割，逐块输出
    const std::string& content = non_stream.value.content;
    const size_t chunk_size = 10;
    
    size_t i = 0;
    while (i < content.size()) {
        size_t end = std::min(i + chunk_size, content.size());
        // 确保不切断 UTF-8 字符
        while (end > i && (content[end] & 0xC0) == 0x80) {
            end--;
        }
        if (end <= i) end = std::min(i + 1, content.size());
        
        on_chunk(content.substr(i, end - i));
        i = end;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    return ResultVoid::success(true);
}

} // namespace openclaw
