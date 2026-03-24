#ifndef OPENCLAW_TOOL_ENGINE_H
#define OPENCLAW_TOOL_ENGINE_H

#include <string>
#include <vector>
#include <functional>
#include <map>

#include "tool.h"

namespace openclaw {

// ToolEngine 已在上一个头文件中定义
// 这里是完整声明

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
    
    // exec 工具
    static Result<std::string> exec_tool(const std::string& params);
    
    // read 工具
    static Result<std::string> read_tool(const std::string& params);
    
    // write 工具
    static Result<std::string> write_tool(const std::string& params);
};

} // namespace openclaw

#endif // OPENCLAW_TOOL_ENGINE_H
