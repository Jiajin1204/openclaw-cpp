/**
 * @file unix_socket_client.cpp
 * @brief Unix Socket 客户端实现
 */

#include "openclaw/unix_socket_client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <cstring>

namespace openclaw {

// ============ UnixSocketMessage 实现 ============

std::string UnixSocketMessage::to_json() const {
    std::string json = "{\"type\":\"" + type + "\"";
    if (!from.empty()) json += ",\"from\":\"" + from + "\"";
    if (!to.empty()) json += ",\"to\":\"" + to + "\"";
    if (!text.empty()) json += ",\"text\":\"" + text + "\"";
    if (id > 0) json += ",\"id\":" + std::to_string(id);
    json += "}";
    return json;
}

UnixSocketMessage UnixSocketMessage::from_json(const std::string& json) {
    UnixSocketMessage msg;
    
    // 解析 type
    size_t type_start = json.find("\"type\":\"");
    if (type_start != std::string::npos) {
        type_start += 8;
        size_t type_end = json.find("\"", type_start);
        if (type_end != std::string::npos) {
            msg.type = json.substr(type_start, type_end - type_start);
        }
    }
    
    // 解析 from
    size_t from_start = json.find("\"from\":\"");
    if (from_start != std::string::npos) {
        from_start += 8;
        size_t from_end = json.find("\"", from_start);
        if (from_end != std::string::npos) {
            msg.from = json.substr(from_start, from_end - from_start);
        }
    }
    
    // 解析 to
    size_t to_start = json.find("\"to\":\"");
    if (to_start != std::string::npos) {
        to_start += 6;
        size_t to_end = json.find("\"", to_start);
        if (to_end != std::string::npos) {
            msg.to = json.substr(to_start, to_end - to_start);
        }
    }
    
    // 解析 text
    size_t text_start = json.find("\"text\":\"");
    if (text_start != std::string::npos) {
        text_start += 8;
        size_t text_end = json.find("\"", text_start);
        if (text_end != std::string::npos) {
            msg.text = json.substr(text_start, text_end - text_start);
        }
    }
    
    // 解析 id
    size_t id_start = json.find("\"id\":");
    if (id_start != std::string::npos) {
        id_start += 5;
        size_t id_end = json.find(",", id_start);
        if (id_end == std::string::npos) id_end = json.find("}", id_start);
        if (id_end != std::string::npos) {
            msg.id = std::stoi(json.substr(id_start, id_end - id_start));
        }
    }
    
    return msg;
}

// ============ UnixSocketClient 实现 ============

class UnixSocketClient::Impl {
public:
    std::thread read_thread;
    std::string buffer;
    std::mutex buffer_mutex;
};

UnixSocketClient::UnixSocketClient()
    : pImpl(std::make_unique<Impl>())
    , connected_(false)
    , running_(false)
    , next_message_id_(1)
    , sockfd_(-1) {
}

UnixSocketClient::~UnixSocketClient() {
    disconnect();
}

bool UnixSocketClient::connect(const std::string& socket_path) {
    if (connected_) {
        std::cout << "[WARN] Already connected to " << socket_path << std::endl;
        return true;
    }
    
    socket_path_ = socket_path;
    
    // 创建 Unix domain socket
    sockfd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "[ERROR] Failed to create socket: " << strerror(errno) << std::endl;
        if (error_handler_) error_handler_(strerror(errno));
        return false;
    }
    
    // 设置地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    // 连接
    if (::connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ERROR] Failed to connect to " << socket_path << ": " << strerror(errno) << std::endl;
        close(sockfd_);
        sockfd_ = -1;
        if (error_handler_) error_handler_(strerror(errno));
        return false;
    }
    
    // 设置为非阻塞模式
    int flags = fcntl(sockfd_, F_GETFL, 0);
    fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);
    
    connected_ = true;
    running_ = true;
    
    std::cout << "[INFO] Connected to Unix socket: " << socket_path << std::endl;
    
    if (connect_handler_) connect_handler_(true);
    
    // 启动读取线程
    pImpl->read_thread = std::thread([this]() {
        char buffer[4096];
        
        while (running_) {
            ssize_t n = read(sockfd_, buffer, sizeof(buffer) - 1);
            
            if (n > 0) {
                buffer[n] = '\0';
                
                // 处理接收到的数据
                std::lock_guard<std::mutex> lock(pImpl->buffer_mutex);
                pImpl->buffer += buffer;
                
                // 按行分割处理
                size_t pos;
                while ((pos = pImpl->buffer.find('\n')) != std::string::npos) {
                    std::string line = pImpl->buffer.substr(0, pos);
                    pImpl->buffer = pImpl->buffer.substr(pos + 1);
                    
                    if (!line.empty()) {
                        std::cout << "[DEBUG] Received: " << line << std::endl;
                        
                        auto msg = UnixSocketMessage::from_json(line);
                        
                        if (msg.type == "pong") {
                            std::cout << "[INFO] Received pong" << std::endl;
                        } else if (msg.type == "reply" || msg.type == "ack") {
                            if (message_handler_) {
                                message_handler_(msg);
                            }
                        }
                    }
                }
            } else if (n == 0) {
                // 连接关闭
                std::cout << "[INFO] Connection closed by server" << std::endl;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "[ERROR] Read error: " << strerror(errno) << std::endl;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        connected_ = false;
        if (connect_handler_) connect_handler_(false);
    });
    
    return true;
}

void UnixSocketClient::disconnect() {
    if (!connected_) {
        return;
    }
    
    running_ = false;
    
    if (pImpl->read_thread.joinable()) {
        pImpl->read_thread.join();
    }
    
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
    
    connected_ = false;
    std::cout << "[INFO] Disconnected from Unix socket" << std::endl;
}

bool UnixSocketClient::send_message(const std::string& text, const std::string& from) {
    if (!connected_) {
        std::cerr << "[ERROR] Not connected" << std::endl;
        return false;
    }
    
    UnixSocketMessage msg;
    msg.type = "send";
    msg.from = from;
    msg.text = text;
    msg.id = next_message_id_++;
    
    std::string json = msg.to_json();
    json += "\n";
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    ssize_t n = write(sockfd_, json.c_str(), json.length());
    if (n < 0) {
        std::cerr << "[ERROR] Failed to send message: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[INFO] Sent message: " << text << std::endl;
    return true;
}

bool UnixSocketClient::send_ping() {
    if (!connected_) {
        return false;
    }
    
    std::string json = "{\"type\":\"ping\"}\n";
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    ssize_t n = write(sockfd_, json.c_str(), json.length());
    return n > 0;
}

void UnixSocketClient::set_message_handler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void UnixSocketClient::set_connect_handler(ConnectHandler handler) {
    connect_handler_ = std::move(handler);
}

void UnixSocketClient::set_error_handler(ErrorHandler handler) {
    error_handler_ = std::move(handler);
}

} // namespace openclaw
