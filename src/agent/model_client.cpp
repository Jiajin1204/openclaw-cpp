#include "model_client.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <sstream>
#include <curl/curl.h>

// CURL 回调
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

namespace openclaw {

ModelClient::ModelClient() {
    // 从配置初始化
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
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Headers
    struct curl_slist* header_list = nullptr;
    header_list = curl_slist_append(header_list, "Content-Type: application/json");
    
    if (!api_key_.empty()) {
        std::string auth = "Authorization: Bearer " + api_key_;
        header_list = curl_slist_append(header_list, auth.c_str());
    }
    
    for (const auto& [key, value] : headers) {
        std::string h = key + ": " + value;
        header_list = curl_slist_append(header_list, h.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return Result<std::string>::failure(std::string("CURL error: ") + curl_easy_strerror(res));
    }
    
    return Result<std::string>::success(response);
}

std::string ModelClient::build_request_body(const std::vector<Message>& messages, bool stream) const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"model\": \"" << model_ << "\",\n";
    oss << "  \"messages\": [\n";
    
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        oss << "    {\n";
        oss << "      \"role\": \"" << role_to_string(msg.role) << "\",\n";
        oss << "      \"content\": \"" << msg.content << "\"";
        
        if (msg.tool_call_id && !msg.tool_call_id->empty()) {
            oss << ",\n      \"tool_call_id\": \"" << *msg.tool_call_id << "\"";
        }
        
        if (msg.name && !msg.name->empty()) {
            oss << ",\n      \"name\": \"" << *msg.name << "\"";
        }
        
        oss << "\n    }";
        
        if (i < messages.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ],\n";
    oss << "  \"stream\": " << (stream ? "true" : "false") << "\n";
    oss << "}";
    
    return oss.str();
}

ModelClient::ChatResponse ModelClient::parse_response(const std::string& response_body) const {
    ChatResponse response;
    
    // 简化 JSON 解析
    // 查找 content
    size_t content_pos = response_body.find("\"content\"");
    if (content_pos != std::string::npos) {
        size_t colon = response_body.find(":", content_pos);
        size_t quote1 = response_body.find("\"", colon);
        size_t quote2 = response_body.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            response.content = response_body.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    // 查找 finish_reason
    size_t finish_pos = response_body.find("\"finish_reason\"");
    if (finish_pos != std::string::npos) {
        size_t colon = response_body.find(":", finish_pos);
        size_t quote1 = response_body.find("\"", colon);
        size_t quote2 = response_body.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            response.finish_reason = response_body.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    // 查找 usage
    size_t usage_pos = response_body.find("\"usage\"");
    if (usage_pos != std::string::npos) {
        // 简化：只检查是否存在
    }
    
    return response;
}

Result<ModelClient::ChatResponse> ModelClient::chat(const std::vector<Message>& messages) {
    if (api_key_.empty()) {
        // 如果没有 API key，返回模拟响应（用于测试）
        ChatResponse mock_response;
        mock_response.content = "Hello! I'm OpenClaw C++. How can I help you today?";
        mock_response.finish_reason = "stop";
        return Result<ChatResponse>::success(mock_response);
    }
    
    std::string url = base_url_ + "/chat/completions";
    std::string body = build_request_body(messages, false);
    
    auto result = http_post(url, body);
    
    if (!result.ok) {
        return Result<ChatResponse>::failure(result.error);
    }
    
    ChatResponse response = parse_response(result.value);
    return Result<ChatResponse>::success(response);
}

ResultVoid ModelClient::chat_stream(
    const std::vector<Message>& messages,
    StreamCallback on_chunk
) {
    if (api_key_.empty()) {
        return ResultVoid::failure("No API key configured");
    }
    
    std::string url = base_url_ + "/chat/completions";
    std::string body = build_request_body(messages, true);
    
    // 简化实现：先不支持真正的流式
    auto result = chat(messages);
    
    if (!result.ok) {
        return ResultVoid::failure(result.error);
    }
    
    // 逐块回调
    const std::string& content = result.value.content;
    size_t chunk_size = 10;
    for (size_t i = 0; i < content.size(); i += chunk_size) {
        std::string chunk = content.substr(i, std::min(chunk_size, content.size() - i));
        on_chunk(chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    return ResultVoid::success();
}

} // namespace openclaw
