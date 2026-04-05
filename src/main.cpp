/**
 * @file main.cpp
 * @brief OpenClaw C++ - Agent REPL
 */

#include <iostream>
#include <string>
#include <csignal>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>

#include "utils/logger.h"
#include "utils/config.h"
#include "session/session_manager.h"
#include "tools/tool_engine.h"
#include "agent/model_client.h"
#include "agent/agent_loop.h"

using namespace openclaw;

bool g_running = true;

void print_banner() {
    std::cout << R"(
  ____            _        ____   ___  _
 / ___| _   _ ___| |_ ___|  _ \ / _ \| |
 \___ \| | | / __| __/ _ \ |_) | | | | |
  ___) | |_| \__ \ ||  __/  _ <| |_| | |___
 |____/ \__, |___/\__\___|_| \_\\___/|_____|
        |___/
)";
    std::cout << "  OpenClaw C++ v1.0 - Agent REPL\n";
    std::cout << "  Type 'help' for commands\n\n";
}

void print_help() {
    std::cout << R"(
Commands:
  help       - Show this help
  quit       - Exit
  clear      - Clear screen
  tools      - List available tools
)";
}

void print_prompt() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&now_time);
    std::cout << "\033[32m[\033[0m" << std::put_time(tm_now, "%H:%M:%S") << "\033[32m]\033[0m \033[33m>\033[0m ";
}

// 获取可执行文件所在目录
std::string get_exe_dir() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        size_t pos = path.find_last_of('/');
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
    }
    return ".";
}

// 验证模型是否可用 - 实际调用模型测试
bool verify_model() {
    const auto& config = ConfigManager::instance().get();
    
    // 检查 API Key
    if (config.model.api_key.empty()) {
        std::cerr << "\n[ERROR] API Key is empty or not set!\n";
        std::cerr << "Please set environment variable OPENCLAW_CPP_API_KEY\n";
        std::cerr << "Example: export OPENCLAW_CPP_API_KEY=\"your-key-here\"\n";
        return false;
    }
    
    // 检查 base_url
    if (config.model.base_url.empty()) {
        std::cerr << "\n[ERROR] Model base_url is not configured!\n";
        return false;
    }
    
    // 检查 model
    if (config.model.model.empty()) {
        std::cerr << "\n[ERROR] Model name is not configured!\n";
        return false;
    }
    
    // 实际调用模型验证
    std::cout << "[INFO] Testing model connection...\n";
    
    try {
        ModelClient test_client;
        std::vector<Message> test_msgs;
        test_msgs.push_back(Message(MessageRole::User, "hi"));
        
        auto result = test_client.chat(test_msgs);
        
        if (!result.ok) {
            std::cerr << "\n[ERROR] Model call failed: " << result.error << "\n";
            return false;
        }
        
        // 检查返回内容是否有错误标记
        if (result.value.content.find("Error:") == 0 || 
            result.value.content.find("API Error:") == 0) {
            std::cerr << "\n[ERROR] Model returned error: " << result.value.content << "\n";
            return false;
        }
        
        std::cout << "[INFO] Model connection OK!\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Model exception: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    std::signal(SIGINT, [](int){ g_running = false; });
    
    print_banner();
    
    // 获取可执行文件目录，查找 config.json
    std::string exe_dir = get_exe_dir();
    std::string config_path = exe_dir + "/config.json";
    
    // 也检查当前目录
    if (access(config_path.c_str(), F_OK) != 0) {
        config_path = "./config.json";
    }
    
    // 加载配置
    if (!ConfigManager::instance().load(config_path)) {
        std::cerr << "Failed to load config from: " << config_path << "\n";
        return 1;
    }
    
    // 验证模型配置
    if (!verify_model()) {
        return 1;
    }
    
    // 初始化组件
    SessionManager session_manager;
    ToolEngine tool_engine;
    ModelClient model_client;
    AgentLoop agent_loop(&session_manager, &tool_engine);
    
    agent_loop.set_system_prompt("You are OpenClaw C++, a helpful AI assistant. You have access to tools: exec (run shell commands), read (read files), write (write files). When user asks to run commands or access files, use the appropriate tool.");
    
    std::string session_key = "main";
    
    while (g_running) {
        print_prompt();
        
        std::string input;
        if (!std::getline(std::cin, input)) break;
        
        if (input == "quit" || input == "exit") {
            std::cout << "Bye!\n";
            break;
        }
        
        if (input == "help") {
            print_help();
            continue;
        }
        
        if (input == "clear") {
            std::cout << "\033[2J\033[H";
            print_banner();
            continue;
        }
        
        if (input == "tools") {
            auto tools = tool_engine.list_tools();
            std::cout << "\nAvailable tools (" << tools.size() << "):\n";
            for (const auto& t : tools) {
                std::cout << "  - " << t.name << ": " << t.description << "\n";
            }
            continue;
        }
        
        if (input.empty()) continue;
        
        // 运行 Agent（流式输出）
        std::cout << "\n🤖 ";
        std::string response;
        agent_loop.run_stream(session_key, input, [&response](const std::string& chunk) {
            std::cout << chunk << std::flush;
            response += chunk;
        });
        std::cout << "\n\n";
    }
    
    return 0;
}