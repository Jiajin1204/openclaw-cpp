#ifndef OPENCLAW_MODEL_CLIENT_H
#define OPENCLAW_MODEL_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

#include "openclaw/message.h"
#include "openclaw/types.h"

namespace openclaw {

// 结果类型简写
using ResultVoid = Result<bool>;

// ============ Model Client ============

class ModelClient {
public:
    ModelClient();
    ~ModelClient();
    
    // 配置
    void set_api_key(const std::string& key);
    void set_base_url(const std::string& url);
    void set_model(const std::string& model);
    
    // 聊天（非流式）
    struct ChatResponse {
        std::string content;
        std::string finish_reason;
        int input_tokens = 0;
        int output_tokens = 0;
        std::vector<ToolCall> tool_calls;
    };
    
    Result<ChatResponse> chat(const std::vector<Message>& messages);
    
    // 聊天（流式）
    using StreamCallback = std::function<void(const std::string& chunk)>;
    
    ResultVoid chat_stream(
        const std::vector<Message>& messages,
        StreamCallback on_chunk
    );
    
private:
    std::string api_key_;
    std::string base_url_;
    std::string model_;
    
    // HTTP 请求
    Result<std::string> http_post(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers = {}
    );
    
    // 构建请求体
    std::string build_request_body(const std::vector<Message>& messages, bool stream = false) const;
    
    // 解析响应
    ChatResponse parse_response(const std::string& response_body) const;
};

} // namespace openclaw

#endif // OPENCLAW_MODEL_CLIENT_H
