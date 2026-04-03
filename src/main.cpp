/**
 * @file main.cpp
 * @brief OpenClaw C++ - 简化版主程序
 */

#include <iostream>
#include <string>
#include <csignal>
#include <chrono>
#include <ctime>
#include <iomanip>

#include "utils/logger.h"
#include "utils/config.h"

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
    std::cout << "  OpenClaw C++ v1.0\n";
    std::cout << "  Type 'help' for commands\n\n";
}

void print_prompt() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&now_time);
    std::cout << "\033[32m[\033[0m" << std::put_time(tm_now, "%H:%M:%S") << "\033[32m]\033[0m \033[33m>\033[0m ";
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    std::signal(SIGINT, [](int){ g_running = false; });
    
    print_banner();
    
    while (g_running) {
        print_prompt();
        
        std::string input;
        if (!std::getline(std::cin, input)) break;
        
        if (input == "quit" || input == "exit") {
            std::cout << "Bye!\n";
            break;
        }
        
        if (input == "help") {
            std::cout << R"(
Commands:
  help     - Show this help
  quit     - Exit
  echo <x> - Echo back
)";
            continue;
        }
        
        if (input.empty()) continue;
        
        if (input.rfind("echo ", 0) == 0) {
            std::cout << input.substr(5) << "\n";
            continue;
        }
        
        std::cout << "Say: " << input << "\n";
    }
    
    return 0;
}
