/**
 * @file tools.cpp
 * @brief 工具模块实现
 */

#include "tools.h"
#include "../utils/logger.h"
#include "../session/session.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace openclaw {

ToolEngine::ToolEngine() {
    register_builtin_tools();
}

void ToolEngine::register_builtin_tools() {
    register_tool({"exec", "Execute shell command", exec_tool});
    register_tool({"read", "Read file content", read_tool});
    register_tool({"write", "Write content to file", write_tool});
    register_tool({"memory_search", "Search memory files for keyword", memory_search_tool});
    register_tool({"memory_get", "Get memory file content by date", memory_get_tool});
    LOG_INFO("Registered ", tools_.size(), " builtin tools");
}

void ToolEngine::register_tool(const Tool& tool) {
    tools_[tool.name] = tool;
    LOG_DEBUG("Registered tool: ", tool.name);
}

std::string ToolEngine::execute(const std::string& name, const std::string& params) {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return "Error: Tool not found: " + name;
    }
    
    try {
        return it->second.func(params);
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

std::vector<Tool> ToolEngine::list_tools() const {
    std::vector<Tool> result;
    for (const auto& [_, tool] : tools_) {
        result.push_back(tool);
    }
    return result;
}

bool ToolEngine::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

// ===== 内置工具实现 =====

std::string ToolEngine::exec_tool(const std::string& params) {
    std::string cmd = params;
    // 移除可能的引号
    if (cmd.size() >= 2 && cmd.front() == '"' && cmd.back() == '"') {
        cmd = cmd.substr(1, cmd.size() - 2);
    }
    
    // 移除末尾的反斜杠（JSON 转义遗留）
    while (!cmd.empty() && (cmd.back() == '\\' || cmd.back() == '\n' || cmd.back() == '\r')) {
        cmd.pop_back();
    }
    
    // 展开 ~ 为 home 目录
    if (cmd.find("~") != std::string::npos) {
        const char* home = getenv("HOME");
        if (home) {
            size_t pos;
            while ((pos = cmd.find("~")) != std::string::npos) {
                cmd.replace(pos, 1, home);
            }
        }
    }
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error: Failed to execute command";
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    
    if (output.empty()) {
        return "Command executed (no output)";
    }
    return output;
}

std::string ToolEngine::read_tool(const std::string& params) {
    std::string path = params;
    // 移除可能的引号
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = path.substr(1, path.size() - 2);
    }
    
    // 安全检查：防止路径遍历
    if (path.find("..") != std::string::npos) {
        return "Error: Path traversal not allowed";
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return "Error: Cannot open file: " + path;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // 限制输出长度
    if (content.length() > 10000) {
        content = content.substr(0, 10000) + "\n... (truncated)";
    }
    return content;
}

std::string ToolEngine::write_tool(const std::string& params) {
    // 格式: filepath|content
    size_t pos = params.find('|');
    if (pos == std::string::npos) {
        return "Error: Expected format: filepath|content";
    }
    
    std::string path = params.substr(0, pos);
    std::string content = params.substr(pos + 1);
    
    // 移除可能的引号
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = path.substr(1, path.size() - 2);
    }
    
    // 安全检查：防止路径遍历
    if (path.find("..") != std::string::npos) {
        return "Error: Path traversal not allowed";
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return "Error: Cannot write to file: " + path;
    }
    
    file << content;
    return "File written successfully: " + path;
}

std::string ToolEngine::memory_search_tool(const std::string& params) {
    std::string keyword = params;
    // 移除可能的引号
    if (keyword.size() >= 2 && keyword.front() == '"' && keyword.back() == '"') {
        keyword = keyword.substr(1, keyword.size() - 2);
    }
    
    if (keyword.empty()) {
        return "Error: Please provide a search keyword";
    }
    
    // 默认搜索 workspace 目录下的 memory 文件
    std::string memory_dir = "/home/jason/.openclaw/workspace/memory";
    
    // 尝试搜索
    std::string cmd = "find " + memory_dir + " -name '*.md' -type f 2>/dev/null | head -20";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return "Error: Cannot search memory directory";
    }
    
    std::string files;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        files += buffer;
    }
    pclose(pipe);
    
    if (files.empty()) {
        return "No memory files found in " + memory_dir;
    }
    
    // 在找到的文件中搜索关键词
    std::istringstream file_list(files);
    std::string file_path;
    std::string results;
    int match_count = 0;
    
    while (std::getline(file_list, file_path) && match_count < 5) {
        std::ifstream file(file_path);
        if (!file.is_open()) continue;
        
        std::string line;
        bool found_in_file = false;
        while (std::getline(file, line) && !found_in_file) {
            // 简单的大小写不敏感搜索
            std::string lower_line = line;
            std::string lower_keyword = keyword;
            std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
            std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(), ::tolower);
            
            if (lower_line.find(lower_keyword) != std::string::npos) {
                if (!found_in_file) {
                    results += "📄 " + file_path + "\n";
                    found_in_file = true;
                }
                // 限制每行的长度
                if (line.length() > 100) {
                    line = line.substr(0, 100) + "...";
                }
                results += "   " + line + "\n";
                match_count++;
            }
        }
    }
    
    if (results.empty()) {
        return "No matches found for: " + keyword;
    }
    
    return results;
}

std::string ToolEngine::memory_get_tool(const std::string& params) {
    std::string date = params;
    // 移除可能的引号
    if (date.size() >= 2 && date.front() == '"' && date.back() == '"') {
        date = date.substr(1, date.size() - 2);
    }
    
    if (date.empty()) {
        // 返回今天的日期
        time_t now = time(nullptr);
        std::tm* tm_now = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(tm_now, "%Y-%m-%d");
        date = oss.str();
    }
    
    // 构造文件路径
    std::string memory_file = "/home/jason/.openclaw/workspace/memory/" + date + ".md";
    
    std::ifstream file(memory_file);
    if (!file.is_open()) {
        // 尝试其他格式
        memory_file = "/home/jason/.openclaw/workspace/memory/" + date + ".md";
        
        // 尝试搜索最近的内存文件
        std::string cmd = "ls -t /home/jason/.openclaw/workspace/memory/*.md 2>/dev/null | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string latest = buffer;
                latest.erase(std::remove(latest.begin(), latest.end(), '\n'), latest.end());
                latest.erase(std::remove(latest.begin(), latest.end(), '\r'), latest.end());
                
                std::ifstream latest_file(latest);
                if (latest_file.is_open()) {
                    std::stringstream buffer;
                    buffer << latest_file.rdbuf();
                    pclose(pipe);
                    return "📅 " + latest + "\n\n" + buffer.str();
                }
            }
            pclose(pipe);
        }
        
        return "Error: Memory file not found: " + date + ".md";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return "📅 " + memory_file + "\n\n" + buffer.str();
}

} // namespace openclaw
