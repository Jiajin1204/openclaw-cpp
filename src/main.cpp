/**
 * @file main.cpp
 * @brief OpenClaw C++ Unix Socket 客户端 - 连接到手机 Gateway
 */

#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <vector>

#include "openclaw/unix_socket_client.h"

using namespace openclaw;

// 全局变量
std::unique_ptr<UnixSocketClient> g_socket_client;
bool g_running = true;

// 打印 banner
void print_banner() {
    std::cout << "\033[36m" << R"(
  ____            _        ____   ___  _
 / ___| _   _ ___| |_ ___|  _ \ / _ \| |
 \___ \| | | / __| __/ _ \ |_) | | | | |
  ___) | |_| \__ \ ||  __/  _ <| |_| | |___
 |____/ \__, |___/\__\___|_| \_\\___/|_____|
        |___/
)" << "\033[0m";
    std::cout << "\033[90m  OpenClaw C++ Unix Socket Client\033[0m" << std::endl;
}

// 打印帮助
void print_help() {
    std::cout << R"(
\033[36m📖 Commands:\033[0m
  help           显示帮助
  connect <path> 连接 Unix socket
  disconnect     断开连接
  send <msg>     发送消息
  quit/exit      退出

\033[36m💡 示例:\033[0m
  connect /tmp/openclaw.sock
  send 你好
)" << std::endl;
}

// 打印提示符
void print_prompt() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&now_time);

    std::cout << "\033[32m┌─[\033[0m";
    std::cout << "\033[36m" << std::put_time(tm_now, "%H:%M:%S") << "\033[0m";
    std::cout << "\033[32m]\033[0m ";
    
    if (g_socket_client && g_socket_client->is_connected()) {
        std::cout << "\033[32m📱\033[0m ";
    }
    
    std::cout << "\033[33m>\033[0m ";
}

int main(int argc, char* argv[]) {
    // 信号处理
    std::signal(SIGINT, [](int){ g_running = false; });
    std::signal(SIGTERM, [](int){ g_running = false; });

    print_banner();
    
    // 如果提供了 socket 路径，尝试连接
    if (argc > 2 && std::string(argv[1]) == "-s") {
        std::string socket_path = argv[2];
        std::cout << "Connecting to " << socket_path << "..." << std::endl;
        
        g_socket_client = std::make_unique<UnixSocketClient>();
        
        g_socket_client->set_connect_handler([](bool connected) {
            std::cout << (connected ? "✅ Connected!" : "❌ Disconnected") << std::endl;
        });
        
        g_socket_client->set_message_handler([](const UnixSocketMessage& msg) {
            if (msg.type == "reply") {
                std::cout << "\n🤖 Agent: " << msg.text << std::endl;
            } else if (msg.type == "ack") {
                std::cout << "✓ Message acknowledged" << std::endl;
            }
        });
        
        g_socket_client->set_error_handler([](const std::string& error) {
            std::cerr << "❌ Error: " << error << std::endl;
        });
        
        if (!g_socket_client->connect(socket_path)) {
            std::cerr << "Failed to connect to " << socket_path << std::endl;
            return 1;
        }
        
        // 发送测试消息
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        g_socket_client->send_message("Hello from C++ client!", "cli");
        std::cout << "Sent greeting message" << std::endl;
        
        // 保持运行
        while (g_running && g_socket_client->is_connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        return 0;
    }

    // REPL 模式
    std::cout << "\033[90m输入 'help' 查看命令\033[0m" << std::endl;
    std::cout << "\033[90m或者用 -s <socket_path> 直接连接\033[0m" << std::endl << std::endl;
    
    while (g_running) {
        print_prompt();
        
        std::string input;
        if (!std::getline(std::cin, input)) break;
        
        // 去除空白
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = input.find_last_not_of(" \t");
        input = input.substr(start, end - start + 1);
        
        if (input.empty()) continue;
        
        // 命令处理
        if (input == "quit" || input == "exit") {
            std::cout << "\n👋 Goodbye!\n" << std::endl;
            if (g_socket_client) g_socket_client->disconnect();
            break;
        }
        
        if (input == "help") {
            print_help();
            continue;
        }
        
        if (input == "disconnect") {
            if (g_socket_client) {
                g_socket_client->disconnect();
                std::cout << "📴 已断开连接" << std::endl;
            }
            continue;
        }
        
        // connect 命令
        if (input.rfind("connect ", 0) == 0) {
            std::string socket_path = input.substr(8);
            
            g_socket_client = std::make_unique<UnixSocketClient>();
            
            g_socket_client->set_connect_handler([](bool connected) {
                std::cout << (connected ? "✅ 已连接!" : "❌ 已断开") << std::endl;
            });
            
            g_socket_client->set_message_handler([](const UnixSocketMessage& msg) {
                if (msg.type == "reply") {
                    std::cout << "\n🤖 Agent: " << msg.text << std::endl;
                }
            });
            
            g_socket_client->set_error_handler([](const std::string& error) {
                std::cerr << "❌ Error: " << error << std::endl;
            });
            
            if (g_socket_client->connect(socket_path)) {
                std::cout << "✅ 正在连接..." << socket_path << std::endl;
            } else {
                std::cout << "❌ 连接失败" << std::endl;
                g_socket_client.reset();
            }
            continue;
        }
        
        // send 命令
        if (input.rfind("send ", 0) == 0) {
            std::string text = input.substr(5);
            
            if (!g_socket_client || !g_socket_client->is_connected()) {
                std::cout << "❌ 未连接，请先使用 connect 命令" << std::endl;
                continue;
            }
            
            if (g_socket_client->send_message(text, "cli")) {
                std::cout << "📤 已发送..." << std::endl;
            } else {
                std::cout << "❌ 发送失败" << std::endl;
            }
            continue;
        }
        
        // 未连接时的处理
        if (!g_socket_client || !g_socket_client->is_connected()) {
            std::cout << "❌ 未连接到手机，请使用 'connect <socket_path>' 连接" << std::endl;
            std::cout << "\033[90m示例: connect /tmp/openclaw.sock\033[0m" << std::endl;
            continue;
        }
        
        // 直接发送消息
        if (g_socket_client->send_message(input, "cli")) {
            std::cout << "📤 已发送..." << std::endl;
        } else {
            std::cout << "❌ 发送失败" << std::endl;
        }
    }

    return 0;
}
