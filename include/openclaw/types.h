#ifndef OPENCLAW_TYPES_H
#define OPENCLAW_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace openclaw {

// ============ 简单 JSON 类型 ============
// 使用 std::string 存储 JSON 字符串，避免复杂的模板依赖

using Json = std::string;

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
