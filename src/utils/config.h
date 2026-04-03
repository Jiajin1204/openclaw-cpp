#ifndef OPENCLAW_CONFIG_H
#define OPENCLAW_CONFIG_H

#include <string>
#include <memory>
#include "openclaw/types.h"

namespace openclaw {

// ============ 配置管理器 ============

class ConfigManager {
public:
    static ConfigManager& instance();
    
    // 加载配置
    bool load(const std::string& path);
    bool load_from_string(const std::string& content);
    
    // 获取配置
    const Config& get() const { return config_; }
    Config& get() { return config_; }
    
    // 获取值
    std::string get_string(const std::string& key, const std::string& default_value = "");
    int get_int(const std::string& key, int default_value = 0);
    bool get_bool(const std::string& key, bool default_value = false);
    
    // 设置值
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    
    // 保存配置
    bool save(const std::string& path = "");
    
    // 环境变量替换
    std::string expand_env(const std::string& value) const;
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    Config config_;
    std::string config_path_;
    
    // 使用 nlohmann/json 解析
    bool parse_json(const nlohmann::json& j);
};

} // namespace openclaw

#endif // OPENCLAW_CONFIG_H