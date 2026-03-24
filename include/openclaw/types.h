#ifndef OPENCLAW_TYPES_H
#define OPENCLAW_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>

namespace openclaw {

// ============ 基础类型 ============

using Json = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<Json>,
    std::map<std::string, Json>
>;

// ============ 结果类型 ============

template<typename T>
struct Result {
    bool ok;
    T value;
    std::string error;
    
    static Result<T> success(T val) {
        return {true, std::move(val), ""};
    }
    
    static Result<T> failure(std::string err) {
        return {false, T{}, std::move(err)};
    }
    
    explicit operator bool() const { return ok; }
};

using ResultVoid = Result<std::monostate>;

// ============ 配置结构 ============

struct Config {
    struct Gateway {
        std::string host = "0.0.0.0";
        int port = 18789;
        std::string token = "";
    } gateway;
    
    struct Model {
        std::string provider = "openai";
        std::string api_key = "";
        std::string base_url = "https://api.openai.com/v1";
        std::string model = "gpt-4o";
    } model;
    
    struct Workspace {
        std::string path = "~/.openclaw/workspace";
    } workspace;
    
    struct Session {
        std::string dm_scope = "main";
        int max_entries = 500;
        int prune_after_days = 30;
    } session;
};

} // namespace openclaw

#endif // OPENCLAW_TYPES_H
