#include "tool_engine.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

// Windows/Linux 兼容
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace openclaw {

ToolEngine::ToolEngine() {
    register_builtin_tools();
}

ToolEngine::~ToolEngine() {
}

void ToolEngine::register_builtin_tools() {
    // exec - 执行命令
    register_tool("exec", "Execute a shell command", [](const std::string& params) -> Result<std::string> {
        return ToolEngine::exec_tool(params);
    });
    
    // read - 读文件
    register_tool("read", "Read a file", [](const std::string& params) -> Result<std::string> {
        return ToolEngine::read_tool(params);
    });
    
    // write - 写文件
    register_tool("write", "Write to a file", [](const std::string& params) -> Result<std::string> {
        return ToolEngine::write_tool(params);
    });
    
    LOG_INFO("Registered ", tools_.size(), " builtin tools");
}

void ToolEngine::register_tool(const Tool& tool) {
    tools_[tool.name] = tool;
    LOG_DEBUG("Registered tool: ", tool.name);
}

void ToolEngine::register_tool(const std::string& name, 
                               const std::string& description,
                               ToolFunc func) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.func = func;
    register_tool(tool);
}

Result<std::string> ToolEngine::execute(const std::string& tool_name, 
                                         const std::string& params) {
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) {
        return Result<std::string>::failure("Tool not found: " + tool_name);
    }
    
    try {
        return it->second.func(params);
    } catch (const std::exception& e) {
        return Result<std::string>::failure(std::string("Tool error: ") + e.what());
    }
}

std::vector<Tool> ToolEngine::list_tools() const {
    std::vector<Tool> result;
    for (const auto& [name, tool] : tools_) {
        result.push_back(tool);
    }
    return result;
}

bool ToolEngine::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

// ============ 内置工具实现 ============

Result<std::string> ToolEngine::exec_tool(const std::string& params) {
    // 解析 JSON 参数
    std::string command;
    
    try {
        auto j = nlohmann::json::parse(params);
        
        // 尝试多个可能的字段名
        if (j.contains("command") && j["command"].is_string()) {
            command = j["command"];
        } else if (j.contains("cmd") && j["cmd"].is_string()) {
            command = j["cmd"];
        } else if (j.contains("shell") && j["shell"].is_string()) {
            command = j["shell"];
        } else {
            // 遍历所有 string 值
            for (auto& [key, val] : j.items()) {
                if (val.is_string()) {
                    command = val;
                    break;
                }
            }
        }
        
        if (command.empty()) {
            command = params;  // 回退到原始字符串
        }
    } catch (...) {
        // 解析失败，当作普通命令
        command = params;
    }
    
    // 移除首尾空白和引号
    while (!command.empty() && (command.front() == ' ' || command.front() == '"' || command.front() == '\'')) {
        command = command.substr(1);
    }
    while (!command.empty() && (command.back() == ' ' || command.back() == '"' || command.back() == '\'')) {
        command.pop_back();
    }
    
    LOG_INFO("Executing command: ", command);
    
    // 使用 popen 执行命令
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return Result<std::string>::failure("Failed to execute command");
    }
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int status = pclose(pipe);
    
    if (status != 0) {
        output += "\n[Exit code: " + std::to_string(status) + "]";
    }
    
    // 截断过长输出
    if (output.size() > 10000) {
        output = output.substr(0, 10000) + "\n[Output truncated...]";
    }
    
    return Result<std::string>::success(output);
}

Result<std::string> ToolEngine::read_tool(const std::string& params) {
    // 解析 JSON 参数
    std::string filepath;
    
    try {
        auto j = nlohmann::json::parse(params);
        
        if (j.contains("path") && j["path"].is_string()) {
            filepath = j["path"];
        } else if (j.contains("file") && j["file"].is_string()) {
            filepath = j["file"];
        } else if (j.contains("filename") && j["filename"].is_string()) {
            filepath = j["filename"];
        } else {
            filepath = params;  // 回退
        }
    } catch (...) {
        filepath = params;
    }
    
    // 移除首尾空白和引号
    while (!filepath.empty() && (filepath.front() == ' ' || filepath.front() == '"' || filepath.front() == '\'')) {
        filepath = filepath.substr(1);
    }
    while (!filepath.empty() && (filepath.back() == ' ' || filepath.back() == '"' || filepath.back() == '\'')) {
        filepath.pop_back();
    }
    
    // 安全检查：防止路径遍历
    if (filepath.find("..") != std::string::npos) {
        return Result<std::string>::failure("Path traversal not allowed");
    }
    
    LOG_INFO("Reading file: ", filepath);
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return Result<std::string>::failure("Failed to open file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // 截断过长内容
    if (content.size() > 50000) {
        content = content.substr(0, 50000) + "\n[Content truncated...]";
    }
    
    return Result<std::string>::success(content);
}

Result<std::string> ToolEngine::write_tool(const std::string& params) {
    // 解析参数：期望格式 "filepath|content" 或 JSON
    size_t pipe_pos = params.find("|");
    if (pipe_pos == std::string::npos) {
        return Result<std::string>::failure("Invalid params format: expected filepath|content");
    }
    
    std::string filepath = params.substr(0, pipe_pos);
    std::string content = params.substr(pipe_pos + 1);
    
    // 移除引号
    if (filepath.size() >= 2 && 
        filepath.front() == '"' && filepath.back() == '"') {
        filepath = filepath.substr(1, filepath.size() - 2);
    }
    
    // 安全检查
    if (filepath.find("..") != std::string::npos) {
        return Result<std::string>::failure("Path traversal not allowed");
    }
    
    LOG_INFO("Writing file: ", filepath);
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return Result<std::string>::failure("Failed to open file for writing: " + filepath);
    }
    
    file << content;
    
    if (!file.good()) {
        return Result<std::string>::failure("Failed to write content");
    }
    
    return Result<std::string>::success("File written successfully: " + filepath);
}

} // namespace openclaw
