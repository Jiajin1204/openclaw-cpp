/**
 * @file unix_socket_client.h
 * @brief Unix Socket 客户端 - 连接手机上的 OpenClaw Gateway plugin
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace openclaw {

// 消息结构
struct UnixSocketMessage {
    std::string type;      // "send", "ping", "pong", "reply", "ack"
    std::string from;     // 发送者 ID
    std::string to;       // 接收者 ID
    std::string text;     // 消息内容
    int id = 0;          // 消息 ID
    
    std::string to_json() const;
    static UnixSocketMessage from_json(const std::string& json);
};

// 消息回调
using MessageHandler = std::function<void(const UnixSocketMessage& msg)>;
using ConnectHandler = std::function<void(bool connected)>;
using ErrorHandler = std::function<void(const std::string& error)>;

/**
 * @brief Unix Socket 客户端
 * 
 * 连接到手机的 OpenClaw Gateway plugin，通过 Unix Socket 通信
 */
class UnixSocketClient {
public:
    UnixSocketClient();
    ~UnixSocketClient();
    
    // 连接到 Unix Socket
    bool connect(const std::string& socket_path);
    
    // 断开连接
    void disconnect();
    
    // 是否已连接
    bool is_connected() const { return connected_; }
    
    // 发送消息
    bool send_message(const std::string& text, const std::string& from = "cpp-client");
    
    // 发送 ping
    bool send_ping();
    
    // 设置回调
    void set_message_handler(MessageHandler handler);
    void set_connect_handler(ConnectHandler handler);
    void set_error_handler(ErrorHandler handler);
    
    // 获取 socket 路径
    const std::string& socket_path() const { return socket_path_; }

private:
    // 内部实现
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    std::string socket_path_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    
    MessageHandler message_handler_;
    ConnectHandler connect_handler_;
    ErrorHandler error_handler_;
    
    std::mutex send_mutex_;
    int next_message_id_ = 1;
    int sockfd_ = -1;  // Socket 文件描述符
};

} // namespace openclaw
