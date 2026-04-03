# openclaw-cpp 开发日记

## 项目背景

### 发起人
- **罗同学** - SOC 厂商底层开发
- 使用旧笔记本（华硕 X555LJ）运行 Ubuntu

### 需求
用纯 C++ 实现精简版 OpenClaw Gateway，可在旧笔记本上运行。

### 技术方案
- **语言**: C++17
- **HTTP**: libcurl
- **JSON**: nlohmann/json（单头文件）
- **编译**: CMake

### 当前状态（2026-04-03）
- ✅ 项目初始化完成
- ✅ nlohmann/json 集成
- ✅ 简化版 REPL（可编译运行）
- ⚠️ Agent/ModelClient/Session 等模块代码存在但编译不完整

---

## 调试环境

### 本机
- **CPU**: Intel i3-4005U (双核 1.7GHz)
- **内存**: 4GB
- **系统**: Ubuntu 24.04 (Linux 6.8.0-106-generic)
- **工作目录**: `~/.openclaw/workspace/openclaw-cpp`

### 编译命令
```bash
cd ~/.openclaw/workspace/openclaw-cpp
mkdir -p build && cd build
cmake .. && make -j4
./bin/openclaw-cpp
```

### 常用命令
```bash
# 清理编译
rm -rf build && mkdir build

# 重新配置
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

---

## 问题解决

### 问题 1：JSON 解析复杂
**时间**: 2026-04-03 15:35

**问题**: config.cpp 手写 JSON 解析代码复杂难维护

**解决**: 
- 集成 nlohmann/json（从 GitHub clone v3.11.3）
- 重写 config.cpp 使用 nlohmann/json
- 删除旧的自定义 Json 类型

**经验**: 有成熟的库就用，别重复造轮子

---

### 问题 2：编译失败（缺少头文件）
**时间**: 2026-04-03 17:08

**问题**: fe18d70 版本的 main.cpp 引用了很多不存在的头文件

**原因**: 
- `session/session.h` 不存在（只有 include/openclaw/session.h）
- `tools/tools.h` 不存在
- `agent/agent.h` 不存在

**解决**:
- 复制 include/openclaw/*.h 到 src/
- 创建空的 tools/tools.h、agent/agent.h
- 多次迭代修复 include 路径

---

### 问题 3：接口不匹配
**时间**: 2026-04-03 17:29

**问题**: agent.cpp 使用了 IModelClient、IToolEngine 接口，但代码里没有定义

**原因**: 早期代码版本不一致

**解决**:
- 暂时跳过 agent.cpp，创建一个简化版的 main.cpp
- 后续需要重写 Agent 模块

---

### 问题 4：ResultVoid 未定义
**时间**: 2026-04-03 17:51

**问题**: model_client.h 用了 ResultVoid 但没定义

**解决**: 在 model_client.h 添加 `using ResultVoid = Result<bool>;`

---

## 编译状态（2026-04-03 18:34）

```
openclaw-cpp ✅ 可编译运行
├── main.cpp ✅ REPL 入口（简化版）
├── utils/ ✅
│   ├── logger.cpp ✅
│   ├── config.cpp ✅ (nlohmann/json)
│   └── ...
├── agent/ ⚠️ 代码存在但需修复
│   ├── agent.cpp ❌ 编译失败
│   ├── agent.h ✅
│   ├── agent_loop.cpp ✅
│   ├── agent_loop.h ✅
│   ├── model_client.cpp ⚠️ 小问题已修复
│   └── model_client.h ✅
├── session/ ⚠️
│   ├── session.h ✅
│   └── session_manager.cpp ✅
├── tools/ ⚠️
│   ├── tool.h ✅
│   ├── tool_engine.h ✅
│   ├── tools.cpp ❌ 引用不存在的方法
│   └── tools.h ✅
└── gateway/ ✅ 代码存在但未集成
```

---

## 经验总结

1. **不要重复造轮子**: nlohmann/json 比手写解析香
2. **Git revert 很必要**: 经常 `git checkout HEAD~1 -- file` 回退
3. **分步验证**: 每修一个小问题就编译一次
4. **简化优先**: 先让项目跑起来，再加功能

---

## 待完成

- [ ] 修复 tools.cpp 编译问题
- [ ] 集成 Session 模块
- [ ] 集成 ToolEngine 模块
- [ ] 集成 AgentLoop 模块
- [ ] 添���实际的命令执行（exec tool）
- [ ] 测试运行 `ls /home/jason`

---

_2026-04-03 创建_