#ifndef OPENCLAW_WS_SERVER_H
#define OPENCLAW_WS_SERVER_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#ifdef USE_BOOST
#include <boost/beast.hpp>
#endif

namespace openclaw {

// ============ WebSocket 消息 ============

struct WSMessage {
    enum class Type {
        Text,
        Binary,
        Close,
        Ping,
        Pong
    };
    
    Type type = Type::Text;
    std::string data;
    std::string client_id;
};

// ============ WebSocket 连接 ============

class WSConnection : public std::enable_shared_from_this<WSConnection> {
public:
    explicit WSConnection(const std::string& id);
    ~WSConnection();
    
    void set_message_handler(std::function<void(const WSMessage&)> handler);
    void set_close_handler(std::function<void(const std::string&)> handler);
    
    void send(const std::string& message);
    void send_binary(const std::string& data);
    void close();
    
    const std::string& id() const { return id_; }
    bool is_open() const;
    
#ifdef USE_BOOST
    void set_socket(boost::asio::ip::tcp::socket&& socket);
    boost::asio::io_context& get_io_context();
    void start();
    void read_loop();
#endif

private:
    std::string id_;
    std::function<void(const WSMessage&)> message_handler_;
    std::function<void(const std::string&)> close_handler_;
    
#ifdef USE_BOOST
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::asio::io_context& ioc_;
    bool closed_ = false;
#endif
};

// ============ WebSocket 服务器 ============

class WSServer {
public:
    using MessageHandler = std::function<void(const std::string& client_id, const WSMessage&)>;
    using ConnectHandler = std::function<void(const std::string& client_id)>;
    using DisconnectHandler = std::function<void(const std::string& client_id)>;
    
    WSServer(const std::string& host, int port);
    ~WSServer();
    
    // 设置回调
    void set_message_handler(MessageHandler handler);
    void set_connect_handler(ConnectHandler handler);
    void set_disconnect_handler(DisconnectHandler handler);
    
    // 启动/停止
    bool start();
    void stop();
    bool is_running() const;
    
    // 发送消息
    void send_to(const std::string& client_id, const std::string& message);
    void broadcast(const std::string& message);
    
private:
    std::string host_;
    int port_;
    std::atomic<bool> running_{false};
    
    std::thread acceptor_thread_;
    std::vector<std::shared_ptr<WSConnection>> connections_;
    std::mutex connections_mutex_;
    
    MessageHandler message_handler_;
    ConnectHandler connect_handler_;
    DisconnectHandler disconnect_handler_;
    
    // 连接管理
    void add_connection(std::shared_ptr<WSConnection> conn);
    void remove_connection(const std::string& id);
    
    // 接受连接（简化实现：单线程）
    void accept_loop();
    
#ifdef USE_BOOST
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
#endif
};

} // namespace openclaw

#endif // OPENCLAW_WS_SERVER_H
