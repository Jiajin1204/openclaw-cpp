#ifndef OPENCLAW_TYPES_H
#define OPENCLAW_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace openclaw {

// ============ JSON 类型 ============
using Json = nlohmann::json;

// ============ 配置结构体 ============
struct GatewayConfig {
    std::string host;
    int port = 0;
    std::string token;
};

struct ModelConfig {
    std::string provider;
    std::string api_key;
    std::string base_url;
    std::string model;
};

struct WorkspaceConfig {
    std::string path;
};

struct SessionConfig {
    std::string dm_scope;
    int max_entries = 20;
    int prune_after_days = 30;
};

struct Config {
    GatewayConfig gateway;
    ModelConfig model;
    WorkspaceConfig workspace;
    SessionConfig session;
};

// ============ 结果类型 ============

template<typename T>
struct Result {
    bool ok;
    T value;
    std::string error;
    
    static Result success(T val) {
        return {true, val, ""};
    }
    
    static Result failure(const std::string& err) {
        return {false, T{}, err};
    }
};

// ============ 可选类型 ============

template<typename T>
using Optional = std::optional<T>;

} // namespace openclaw

#endif // OPENCLAW_TYPES_H