# OpenClaw C++

一个轻量级的 C++ 版 AI 助手，简化自 OpenClaw 原版，支持工具调用、会话管理、模型接入。

## 功能特性

- **REPL 交互模式**：交互式命令行界面
- **工具系统**：内置 exec、read、write 三个工具
- **模型支持**：支持 MiniMax、OpenAI 等 OpenAI API 兼容的模型
- **会话管理**：自动保存对话历史
- **环境变量**：支持 `${ENV_VAR}` 形式的配置

## 编译

### 依赖

- CMake >= 3.10
- C++17 编译器
- libcurl
- nlohmann/json

### 编译步骤

```bash
cd openclaw-cpp
mkdir build && cd build
cmake ..
make -j4
```

编译产物在 `build/bin/openclaw-cpp`

## 运行

```bash
# 方式1：直接运行（会自动找同目录的 config.json）
./build/bin/openclaw-cpp

# 方式2：指定配置文件
./build/bin/openclaw-cpp -c /path/to/config.json

# 方式3：交互模式测试
echo "ls /" | ./build/bin/openclaw-cpp
```

## 配置

创建 `config.json`：

```json
{
  "model": {
    "provider": "minimax-cn",
    "api_key": "${OPENCLAW_CPP_API_KEY}",
    "base_url": "https://api.minimaxi.com/v1/text/chatcompletion_v2",
    "model": "MiniMax-M2.5"
  },
  "gateway": {
    "host": "127.0.0.1",
    "port": 18789,
    "token": "test"
  },
  "workspace": {
    "path": "${HOME}/.openclaw-cpp/workspace"
  },
  "session": {
    "max_entries": 20,
    "prune_after_days": 30
  }
}
```

### 配置说明

| 字段 | 说明 |
|------|------|
| `model.provider` | 模型提供商 |
| `model.api_key` | API Key，支持 `${ENV_VAR}` 环境变量 |
| `model.base_url` | API 地址 |
| `model.model` | 模型名称 |
| `gateway.host` | Gateway 主机（预留） |
| `gateway.port` | Gateway 端口（预留） |
| `workspace.path` | 工作目录 |
| `session.max_entries` | 会话最大消息数 |
| `session.prune_after_days` | 会话保留天数 |

### 安全提示

- **不要**把 API Key 直接写在配置文件中提交到 Git
- 使用环境变量：`"api_key": "${OPENCLAW_CPP_API_KEY}"`
- 运行前设置：`export OPENCLAW_CPP_API_KEY="your-key-here"`

## 内部机制

### 架构

```
┌─────────────────┐
│   main.cpp      │  入口，REPL 循环
├─────────────────┤
│   AgentLoop     │  Agent 核心逻辑
├─────────────────┤
│   ModelClient   │  模型 API 调用
├─────────────────┤
│   ToolEngine    │  工具执行
├─────────────────┤
│ SessionManager  │  会话管理
└─────────────────┘
```

### 工作流程

1. 用户输入 → AgentLoop
2. AgentLoop 构建消息（系统 + 历史 + 用户）
3. ModelClient 调用 LLM API
4. 如果 LLM 返回工具调用 → ToolEngine 执行
5. 工具结果发回 LLM 生成最终回复
6. 回复保存到会话

### 消息格式

```cpp
struct Message {
    MessageRole role;       // system/user/assistant/tool
    std::string content;
    std::string tool_call_id;
    std::string name;
};
```

### 工具调用格式

LLM 返回的工具调用可能是标准格式或自定义格式：

**标准格式（OpenAI 兼容）：**
```json
{
  "tool_calls": [{
    "id": "call_exec_xxx",
    "type": "function",
    "function": {
      "name": "exec",
      "arguments": "{\"command\": \"ls /\"}"
    }
  }]
}
```

**自定义格式（MiniMax 等）：**
某些模型会把工具调用放在 content 中：
```
[TOOL_CALL]
{tool => "exec", args => {
  --command "ls /"
}}
[/TOOL_CALL]
```

执行结果格式：
```json
{
  "role": "tool",
  "tool_call_id": "call_exec_xxx",
  "content": "bin\nboot\ndev\n..."
}
```

## 扩展开发

### 添加新工具

在 `src/tools/tool_engine.cpp` 的 `register_builtin_tools()` 中添加：

```cpp
register_tool("my_tool", "My tool description", [](const std::string& params) {
    // 解析参数并执行
    return Result<std::string>::success("result");
});
```

### 添加新模型

在 `src/agent/model_client.cpp` 中修改 `build_request_body()` 和 `parse_response()`，或添加新的 ModelClient 子类。

## 命令行选项

- `-h, --help`：显示帮助
- `-c, --config`：指定配置文件路径

## LICENSE

MIT