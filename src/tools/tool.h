#ifndef OPENCLAW_TOOL_H
#define OPENCLAW_TOOL_H

#include <string>
#include <functional>
#include <map>

#include "openclaw/types.h"

namespace openclaw {

// ============ 工具定义 ============

using ToolFunc = std::function<Result<std::string>(const std::string& params)>;

struct Tool {
    std::string name;
    std::string description;
    std::string schema;  // JSON Schema
    ToolFunc func;
};

// ============ 工具引擎 ============

class ToolEngine {
public:
    ToolEngine();
    ~ToolEngine();
    
    // 注册工具
    void register_tool(const Tool& tool);
    void register_tool(const std::string& name, 
                       const std::string& description,
                       ToolFunc func);
    
    // 执行工具
    Result<std::string> execute(const std::string& tool_name, 
                                const std::string& params);
    
    // 列出工具
    std::vector<Tool> list_tools() const;
    
    // 工具是否存在
    bool has_tool(const std::string& name) const;
    
private:
    std::map<std::string, Tool> tools_;
    
    // 内置工具
    void register_builtin_tools();
    
    // 工具实现
    static Result<std::string> exec_tool(const std::string& params);
    static Result<std::string> read_tool(const std::string& params);
    static Result<std::string> write_tool(const std::string& params);
};

} // namespace openclaw

#endif // OPENCLAW_TOOL_H
