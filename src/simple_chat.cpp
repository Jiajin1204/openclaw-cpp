/**
 * @file simple_chat.cpp
 * @brief Simple chat client using Unix Socket - for Android
 */

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

#include "openclaw/unix_socket_client.h"
#include "openclaw/message.h"
#include "utils/logger.h"

using namespace openclaw;

std::atomic<bool> g_running(true);

void signal_handler(int) {
    g_running = false;
}

void print_banner() {
    std::cout << R"(
  ____            _        ____   ___  _
 / ___| _   _ ___| |_ ___|  _ \ / _ \| |
 \___ \| | | / __| __/ _ \ |_) | | | | |
  ___) | |_| \__ \ ||  __/  _ <| |_| | |___
 |____/ \__, |___/\__\___|_| \_\\___/|_____|
        |___/
)";
    std::cout << "  OpenClaw C++ Chat (Android)\n";
    std::cout << "  Socket: " << (getenv("OPENCLAW_SOCKET") ?: "/tmp/openclaw.sock") << "\n\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, signal_handler);

    print_banner();

    // Get socket path from env or default
    std::string socket_path = getenv("OPENCLAW_SOCKET") ? getenv("OPENCLAW_SOCKET") : "/tmp/openclaw.sock";
    if (argc > 1) {
        socket_path = argv[1];
    }

    // Create client
    UnixSocketClient client;

    // Set handlers
    client.set_connect_handler([](bool connected) {
        if (connected) {
            std::cout << "[INFO] Connected\n";
        } else {
            std::cout << "[INFO] Disconnected\n";
        }
    });

    client.set_message_handler([](const UnixSocketMessage& msg) {
        if (msg.type == "reply" || msg.type == "chunk") {
            std::cout << msg.text << std::flush;
        } else if (msg.type == "done") {
            std::cout << "\n";
        } else if (msg.type == "ack") {
            std::cout << "[ACK] id=" << msg.id << "\n";
        } else if (msg.type == "pong") {
            // ignore
        } else {
            LOG_DEBUG("Unknown message type: ", msg.type);
        }
    });

    client.set_error_handler([](const std::string& err) {
        std::cerr << "[ERROR] " << err << "\n";
    });

    // Connect
    if (!client.connect(socket_path)) {
        std::cerr << "[ERROR] Failed to connect to " << socket_path << "\n";
        return 1;
    }

    std::cout << "Type 'quit' to exit\n\n";

    std::string user_id = "android_user";

    // Main loop - simple input
    while (g_running) {
        std::cout << "> " << std::flush;

        std::string input;
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == "quit" || input == "exit") {
            std::cout << "Bye!\n";
            break;
        }

        if (input.empty()) {
            continue;
        }

        if (!client.send_message(input, user_id)) {
            std::cerr << "[ERROR] Failed to send message\n";
        }
    }

    client.disconnect();
    return 0;
}
