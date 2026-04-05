# 开发日志

## 2026-04-04

### 项目：openclaw-cpp

一个轻量级的 C++ 版 AI 助手，简化自 OpenClaw 原版。

### 当前状态

**已完成：**
- ✅ REPL 交互模式
- ✅ 工具系统（exec, read, write）
- ✅ 模型接入（MiniMax 等 OpenAI 兼容 API）
- ✅ 会话管理（自动保存对话历史）
- ✅ 环境变量配置支持
- ✅ 模型验证（实际调用 API 测试）
- ✅ 自定义 tool_calls 格式解析（MiniMax 模型的 `[TOOL_CALL]` 格式）

**待完善：**
- 第二次模型调用（让模型生成更自然的回复）
- 流式输出支持
- 插件/Channel 机制

### 调试方法

1. 编译：`cd build && cmake .. && make -j4`
2. 运行：`export OPENCLAW_CPP_API_KEY="your-key" && ./bin/openclaw-cpp`
3. 测试工具：`echo "ls /" | ./bin/openclaw-cpp`

### 关键路径

- 源码：`~/.openclaw/workspace/openclaw-cpp/src/`
- 可执行文件：`~/.openclaw/workspace/openclaw-cpp/build/bin/openclaw-cpp`
- 配置文件：`~/.openclaw/workspace/openclaw-cpp/build/bin/config.json`
- 会话目录：`~/.openclaw-cpp/sessions/`

### 问题解决

1. **工具调用崩溃**：nlohmann/json 在处理某些参数格式时触发 `basic_string::_M_create` 错误  
   解决：简化 parameters 为空对象 `{}`

2. **模型不返回 tool_calls**：MiniMax 模型把工具调用放在 content 中而非标准的 JSON 字段  
   解决：解析 `[TOOL_CALL]...[/TOOL_CALL]` 自定义格式

3. **路径硬编码**：原来使用 `/home/jason/...`  
   解决：改为可执行文件同目录查找配置

### 教训总结

1. 不同模型的 tool_calls 格式可能不同，需要兼容多种格式
2. nlohmann/json 在构造复杂对象时可能触发异常，需要 try-catch 保护
3. API Key 不应提交到 Git，使用环境变量

---

_下次开发提示：实现第二次模型调用，使回复更自然_