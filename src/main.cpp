/**
 * @file main.cpp
 * @brief 主程序入口 - REPL 客户端模式
 */

#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "utils/logger.h"
#include "utils/config.h"
#include "session/session.h"
#include "tools/tools.h"
#include "agent/agent.h"

using namespace openclaw;

// 打印带颜色的 banner
void print_banner() {
    std::cout << "\033[36m" << R"(
  ____            _        ____   ___  _
 / ___| _   _ ___| |_ ___|  _ \ / _ \| |
 \___ \| | | / __| __/ _ \ |_) | | | | |
  ___) | |_| \__ \ ||  __/  _ <| |_| | |___
 |____/ \__, |___/\__\___|_| \_\\___/|_____|
        |___/
)" << "\033[0m";
    std::cout << "\033[90m  OpenClaw C++ REPL Client\033[0m" << std::endl;
    std::cout << "\033[90m  Type 'help' for available commands\033[0m" << std::endl;
}

// 打印帮助信息
void print_help() {
    std::cout << R"(
\033[36m📖 Available Commands:\033[0m

  \033[33mhelp\033[0m              Show this help message
  \033[33mclear\033[0m             Clear the screen
  \033[33mtools\033[0m            List available tools
  \033[33msession\033[0m           Show current session info
  \033[33mquit/exit\033[0m        Exit the program

  \033[33m<message>\033[0m        Send a message to the agent

\033[36m🔧 Tool Examples:\033[0m

  \033[90m  read "/path/to/file"\033[0m
  \033[90m  write "/path/to/file|content"\033[0m
  \033[90m  exec "ls -la"\033[0m
  \033[90m  memory_search "keyword"\033[0m
  \033[90m  memory_get "2026-03-19"\033[0m
)" << std::endl;
}

// 打印可用工具列表
void print_tools(ToolEngine& engine) {
    auto tools = engine.list_tools();
    std::cout << "\n\033[36m🔧 Available Tools (\033[0m" << tools.size() << "\033[36m):\033[0m\n" << std::endl;

    for (const auto& tool : tools) {
        std::cout << "  \033[33m" << tool.name << "\033[0m" << std::endl;
        std::cout << "    \033[90m" << tool.description << "\033[0m" << std::endl;
    }
    std::cout << std::endl;
}

// 打印会话信息
void print_session_info(Session* session) {
    if (!session) {
        std::cout << "\033[31mNo active session\033[0m" << std::endl;
        return;
    }

    auto history = session->get_history(5);
    std::cout << "\n\033[36m📋 Session Info:\033[0m" << std::endl;
    std::cout << "  \033[33mKey:\033[0m " << session->key() << std::endl;
    std::cout << "  \033[33mMessages:\033[0m " << history.size() << std::endl;

    // 打印最后几条消息
    if (!history.empty()) {
        std::cout << "\n\033[90m  Recent messages:\033[0m" << std::endl;
        for (const auto& msg : history) {
            std::string role_str;
            switch (msg.role) {
                case MessageRole::User: role_str = "👤 User"; break;
                case MessageRole::Assistant: role_str = "🤖 Assistant"; break;
                case MessageRole::System: role_str = "⚙️ System"; break;
                case MessageRole::Tool: role_str = "🔧 Tool"; break;
                case MessageRole::ToolResult: role_str = "📤 Tool Result"; break;
                default: role_str = "❓"; break;
            }
            std::string preview = msg.content.length() > 50 ?
                msg.content.substr(0, 50) + "..." : msg.content;
            std::cout << "    " << role_str << ": " << preview << std::endl;
        }
    }
    std::cout << std::endl;
}

// 打印带时间戳的提示符
void print_prompt() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&now_time);

    std::cout << "\033[32m┌─[\033[0m";
    std::cout << "\033[36m" << std::put_time(tm_now, "%H:%M:%S") << "\033[0m";
    std::cout << "\033[32m]\033[0m ";
    std::cout << "\033[33m>\033[0m ";
}

// 打印分隔线
void print_separator() {
    std::cout << "\033[90m────────────────────────────────────────\033[0m" << std::endl;
}

void run_cli_mode(ToolEngine* tool_engine) {
    print_banner();
    print_separator();
    
    std::string session_key = "agent:main:main";
    Session* session = SessionManager::instance().get_session(session_key);
    
    // 创建真正的 Agent
    ModelClient model_client;
    AgentLoop agent(&model_client, tool_engine);
    
    // 设置系统提示词
    agent.set_system_prompt(R"(You are OpenClaw C++, a helpful AI assistant.

You have access to the following tools:
- exec: Execute shell commands
- read: Read file contents  
- write: Write content to files
- memory_search: Search memory files
- memory_get: Get memory file by date

Use these tools to help the user. Always execute tools when the user asks for file operations or shell commands.)");
    
    while (true) {
        print_prompt();
        
        std::string input;
        if (!std::getline(std::cin, input)) break;
        
        // 去除前后空白
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = input.find_last_not_of(" \t");
        input = input.substr(start, end - start + 1);

        // 处理命令
        if (input == "quit" || input == "exit") {
            std::cout << "\n\033[90m👋 Goodbye!\033[0m" << std::endl;
            break;
        }
        
        if (input == "help") {
            print_help();
            continue;
        }
        
        if (input == "clear") {
            std::cout << "\033[2J\033[H";
            print_banner();
            print_separator();
            continue;
        }
        
        if (input == "tools") {
            print_tools(*tool_engine);
            continue;
        }
        
        if (input == "session") {
            print_session_info(session);
            continue;
        }
        
        // 空消息跳过
        if (input.empty()) continue;
        
        // 尝试直接调用工具 (以 / 开头)
        if (input[0] == '/') {
            std::string rest = input.substr(1);
            size_t space_pos = rest.find(' ');
            
            std::string tool_name, tool_params;
            if (space_pos != std::string::npos) {
                tool_name = rest.substr(0, space_pos);
                tool_params = rest.substr(space_pos + 1);
            } else {
                tool_name = rest;
                tool_params = "";
            }
            
            if (tool_engine->has_tool(tool_name)) {
                print_separator();
                std::cout << "\033[36m🔧 Executing: \033[0m" << tool_name 
                          << " \033[90m(\033[0m" << tool_params << "\033[90m)\033[0m" << std::endl;
                std::string result = tool_engine->execute(tool_name, tool_params);
                std::cout << "\n\033[36m📤 Result:\033[0m" << std::endl;
                std::cout << "\033[37m" << result << "\033[0m" << std::endl;
                print_separator();
                continue;
            } else {
                std::cout << "\033[31m❌ Unknown tool: \033[0m" << tool_name << std::endl;
                continue;
            }
        }
        
        // 使用真正的 Agent 处理
        print_separator();
        std::cout << "\033[36m📝 You said:\033[0m " << input << std::endl;
        std::cout << "\n\033[36m💬 Assistant:\033[0m" << std::endl;
        
        std::string response = agent.run(session_key, input);
        std::cout << "\033[37m" << response << "\033[0m" << std::endl;
        print_separator();
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // 加载配置 - 优先级: 环境变量 > .env 文件 > 默认配置
    // 尝试多个可能的 .env 路径
    const char* env_config_path = std::getenv("OPENCLAW_CONFIG");
    if (env_config_path) {
        ConfigManager::instance().load(env_config_path);
    } else {
        // 尝试常见位置
        ConfigManager::instance().load(".env");
        ConfigManager::instance().load("../.env");
        ConfigManager::instance().load("../../.env");
    }
    // 环境变量会在 ModelClient 构造时自动覆盖（见 agent.cpp）

    // 信号处理
    std::signal(SIGINT, [](int){ exit(0); });
    std::signal(SIGTERM, [](int){ exit(0); });

    // 初始化组件
    ToolEngine tool_engine;

    // 运行 CLI 模式
    run_cli_mode(&tool_engine);

    return 0;
}
