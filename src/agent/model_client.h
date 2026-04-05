#ifndef OPENCLAW_MODEL_CLIENT_H
#define OPENCLAW_MODEL_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "openclaw/message.h"
#include "openclaw/types.h"

namespace openclaw {

// ============ 流式回调类型 ============
using StreamCallback = std::function<void(const std::string& chunk)>;

// ============ 前向声明 ============
class ToolEngine;
struct ApiAdapter;

// ============ 结果类型 ============
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
    void set_tool_engine(ToolEngine* engine);
    
    // 聊天响应
    struct ChatResponse {
        std::string content;
        std::string finish_reason;
        int input_tokens = 0;
        int output_tokens = 0;
        std::vector<ToolCall> tool_calls;
    };
    
    // 聊天（非流式）
    Result<ChatResponse> chat(const std::vector<Message>& messages);
    
    // 聊天（流式）
    ResultVoid chat_stream(const std::vector<Message>& messages, StreamCallback on_chunk);
    
private:
    std::string api_key_;
    std::string base_url_;
    std::string model_;
    ToolEngine* tool_engine_;
    std::unique_ptr<ApiAdapter> adapter_;
};

} // namespace openclaw

#endif // OPENCLAW_MODEL_CLIENT_H
