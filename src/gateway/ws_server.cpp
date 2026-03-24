#include "ws_server.h"
#include "../utils/logger.h"
#include <sstream>
#include <random>
#include <chrono>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// Base64 编解码
static std::string base64_encode(const unsigned char* input, int length) {
    BIO* bio = NULL;
    BIO* b64 = NULL;
    BUF_MEM* bufferPtr = NULL;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    
    return result;
}

static std::string base64_decode(const std::string& input) {
    BIO* bio = NULL;
    BIO* b64 = NULL;
    int len = input.length();
    
    unsigned char* buffer = new unsigned char[len];
    memset(buffer, 0, len);
    
    bio = BIO_new_mem_buf(input.data(), len);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    len = BIO_read(bio, buffer, len);
    
    std::string result(reinterpret_cast<char*>(buffer), len);
    delete[] buffer;
    BIO_free_all(bio);
    
    return result;
}

namespace openclaw {

// ============ WSConnection ============

WSConnection::WSConnection(const std::string& id) 
    : id_(id)
#ifdef USE_BOOST
    , ioc_(ioc)
#endif
{
}

WSConnection::~WSConnection() {
    close();
}

void WSConnection::set_message_handler(std::function<void(const WSMessage&)> handler) {
    message_handler_ = handler;
}

void WSConnection::set_close_handler(std::function<void(const std::string&)> handler) {
    close_handler_ = handler;
}

void WSConnection::send(const std::string& message) {
#ifdef USE_BOOST
    if (!ws_.is_open()) return;
    try {
        ws_.text(true);
        ws_.write(boost::asio::buffer(message));
    } catch (const std::exception& e) {
        LOG_ERROR("WS send error: ", e.what());
        close();
    }
#endif
}

void WSConnection::send_binary(const std::string& data) {
#ifdef USE_BOOST
    if (!ws_.is_open()) return;
    try {
        ws_.binary(true);
        ws_.write(boost::asio::buffer(data));
    } catch (const std::exception& e) {
        LOG_ERROR("WS send binary error: ", e.what());
        close();
    }
#endif
}

void WSConnection::close() {
#ifdef USE_BOOST
    if (!closed_ && ws_.is_open()) {
        try {
            ws_.close(boost::beast::websocket::close_code::normal);
        } catch (...) {}
        closed_ = true;
    }
#endif
}

bool WSConnection::is_open() const {
#ifdef USE_BOOST
    return !closed_ && ws_.is_open();
#else
    return false;
#endif
}

#ifdef USE_BOOST
void WSConnection::set_socket(boost::asio::ip::tcp::socket&& socket) {
    ws_. Ownership(std::move(socket));
}

boost::asio::io_context& WSConnection::get_io_context() {
    return ioc_;
}

void WSConnection::start() {
    try {
        ws_.accept();
        LOG_INFO("WebSocket client connected: ", id_);
        read_loop();
    } catch (const std::exception& e) {
        LOG_ERROR("WS start error: ", e.what());
    }
}

void WSConnection::read_loop() {
    auto self = shared_from_this();
    
    ws_.async_read(
        buffer_,
        [self](boost::beast::error_code ec, std::size_t bytes) {
            if (ec) {
                if (ec != boost::beast::websocket::error::closed) {
                    LOG_ERROR("WS read error: ", ec.message());
                }
                if (self->close_handler_) {
                    self->close_handler_(self->id_);
                }
                return;
            }
            
            std::string data = boost::beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());
            
            if (self->message_handler_) {
                WSMessage msg;
                msg.type = WSMessage::Type::Text;
                msg.data = data;
                msg.client_id = self->id_;
                self->message_handler_(msg);
            }
            
            // 继续读取
            self->read_loop();
        }
    );
}
#endif

// ============ WSServer ============

WSServer::WSServer(const std::string& host, int port)
    : host_(host), port_(port)
#ifdef USE_BOOST
    , ioc_(1)
    , acceptor_(ioc_)
#endif
{
}

WSServer::~WSServer() {
    stop();
}

void WSServer::set_message_handler(MessageHandler handler) {
    message_handler_ = handler;
}

void WSServer::set_connect_handler(ConnectHandler handler) {
    connect_handler_ = handler;
}

void WSServer::set_disconnect_handler(DisconnectHandler handler) {
    disconnect_handler_ = handler;
}

bool WSServer::start() {
    if (running_.load()) {
        return true;
    }
    
#ifdef USE_BOOST
    try {
        boost::asio::ip::tcp::resolver resolver(ioc_);
        auto endpoints = resolver.resolve(host_, std::to_string(port_));
        
        acceptor_.open(endpoints->endpoint().protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoints->endpoint());
        acceptor_.listen();
        
        LOG_INFO("WebSocket server started on ", host_, ":", port_);
        
        running_ = true;
        
        // 启动 acceptor 线程
        acceptor_thread_ = std::thread([this]() {
            accept_loop();
        });
        
        // 启动 io_context 线程
        std::thread([this]() {
            ioc_.run();
        }).detach();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start WebSocket server: ", e.what());
        return false;
    }
#else
    LOG_ERROR("WebSocket server requires Boost.Beast (USE_BOOST=ON)");
    return false;
#endif
}

void WSServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    
    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->close();
        }
        connections_.clear();
    }
    
#ifdef USE_BOOST
    acceptor_.close();
    ioc_.stop();
#endif
    
    if (acceptor_thread_.joinable()) {
        acceptor_thread_.join();
    }
    
    LOG_INFO("WebSocket server stopped");
}

bool WSServer::is_running() const {
    return running_.load();
}

void WSServer::send_to(const std::string& client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& conn : connections_) {
        if (conn->id() == client_id && conn->is_open()) {
            conn->send(message);
            break;
        }
    }
}

void WSServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& conn : connections_) {
        if (conn->is_open()) {
            conn->send(message);
        }
    }
}

void WSServer::add_connection(std::shared_ptr<WSConnection> conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.push_back(conn);
    
    if (connect_handler_) {
        connect_handler_(conn->id());
    }
}

void WSServer::remove_connection(const std::string& id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if ((*it)->id() == id) {
            connections_.erase(it);
            break;
        }
    }
    
    if (disconnect_handler_) {
        disconnect_handler_(id);
    }
}

#ifdef USE_BOOST
void WSServer::accept_loop() {
    while (running_.load()) {
        try {
            boost::asio::ip::tcp::socket socket(ioc_);
            acceptor_.accept(socket);
            
            // 生成随机 ID
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 15);
            
            std::ostringstream id;
            id << "client_";
            for (int i = 0; i < 8; i++) {
                id << std::hex << dis(gen);
            }
            
            auto conn = std::make_shared<WSConnection>(id.str());
            conn->set_socket(std::move(socket));
            
            conn->set_message_handler([this](const WSMessage& msg) {
                if (message_handler_) {
                    message_handler_(msg.client_id, msg);
                }
            });
            
            conn->set_close_handler([this](const std::string& id) {
                remove_connection(id);
            });
            
            add_connection(conn);
            
            // 启动连接
            std::thread([conn]() {
                conn->start();
            }).detach();
            
        } catch (const std::exception& e) {
            if (running_.load()) {
                LOG_ERROR("Accept error: ", e.what());
            }
        }
    }
}
#endif

} // namespace openclaw
