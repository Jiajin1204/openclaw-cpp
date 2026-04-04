#include "config.h"
#include "logger.h"
#include <fstream>
#include <algorithm>
#include <cstdlib>

namespace openclaw {

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file: ", path);
        return false;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        config_path_ = path;
        return parse_json(j);
        
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("Failed to parse config: ", e.what());
        return false;
    }
}

bool ConfigManager::load_from_string(const std::string& content) {
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        return parse_json(j);
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("Failed to parse config: ", e.what());
        return false;
    }
}

bool ConfigManager::parse_json(const nlohmann::json& j) {
    try {
        // gateway
        if (j.contains("gateway")) {
            const auto& gw = j["gateway"];
            if (gw.contains("host")) config_.gateway.host = gw["host"].get<std::string>();
            if (gw.contains("port")) config_.gateway.port = gw["port"].get<int>();
            if (gw.contains("token")) config_.gateway.token = gw["token"].get<std::string>();
        }
        
        // model
        if (j.contains("model")) {
            const auto& m = j["model"];
            if (m.contains("provider")) config_.model.provider = m["provider"].get<std::string>();
            if (m.contains("api_key")) {
                config_.model.api_key = m["api_key"].get<std::string>();
                config_.model.api_key = expand_env(config_.model.api_key);
            }
            if (m.contains("base_url")) config_.model.base_url = m["base_url"].get<std::string>();
            if (m.contains("model")) config_.model.model = m["model"].get<std::string>();
        }
        
        // workspace
        if (j.contains("workspace")) {
            const auto& w = j["workspace"];
            if (w.contains("path")) {
                config_.workspace.path = w["path"].get<std::string>();
                config_.workspace.path = expand_env(config_.workspace.path);
            }
        }
        
        // session
        if (j.contains("session")) {
            const auto& s = j["session"];
            if (s.contains("dm_scope")) config_.session.dm_scope = s["dm_scope"].get<std::string>();
            if (s.contains("max_entries")) config_.session.max_entries = s["max_entries"].get<int>();
            if (s.contains("prune_after_days")) config_.session.prune_after_days = s["prune_after_days"].get<int>();
        }
        
        LOG_INFO("Config loaded successfully");
        return true;
        
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to parse config values: ", e.what());
        return false;
    }
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_value) {
    // 简化实现：直接读取已知配置
    if (key == "gateway.host") return config_.gateway.host;
    if (key == "gateway.token") return config_.gateway.token;
    if (key == "model.provider") return config_.model.provider;
    if (key == "model.api_key") return config_.model.api_key;
    if (key == "model.base_url") return config_.model.base_url;
    if (key == "model.model") return config_.model.model;
    if (key == "workspace.path") return config_.workspace.path;
    if (key == "session.dm_scope") return config_.session.dm_scope;
    
    return default_value;
}

int ConfigManager::get_int(const std::string& key, int default_value) {
    if (key == "gateway.port") return config_.gateway.port;
    if (key == "session.max_entries") return config_.session.max_entries;
    if (key == "session.prune_after_days") return config_.session.prune_after_days;
    return default_value;
}

bool ConfigManager::get_bool(const std::string& key, bool default_value) {
    // 简化实现
    return default_value;
}

void ConfigManager::set_string(const std::string& key, const std::string& value) {
    if (key == "gateway.host") config_.gateway.host = value;
    else if (key == "gateway.token") config_.gateway.token = value;
    else if (key == "model.provider") config_.model.provider = value;
    else if (key == "model.api_key") config_.model.api_key = value;
    else if (key == "model.base_url") config_.model.base_url = value;
    else if (key == "model.model") config_.model.model = value;
    else if (key == "workspace.path") config_.workspace.path = value;
    else if (key == "session.dm_scope") config_.session.dm_scope = value;
}

void ConfigManager::set_int(const std::string& key, int value) {
    if (key == "gateway.port") config_.gateway.port = value;
    else if (key == "session.max_entries") config_.session.max_entries = value;
    else if (key == "session.prune_after_days") config_.session.prune_after_days = value;
}

void ConfigManager::set_bool(const std::string& key, bool value) {
    // 简化实现
}

bool ConfigManager::save(const std::string& path) {
    std::string out_path = path.empty() ? config_path_ : path;
    if (out_path.empty()) {
        LOG_ERROR("No config path specified");
        return false;
    }
    
    std::ofstream file(out_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to save config to: ", out_path);
        return false;
    }
    
    nlohmann::json j;
    j["gateway"] = nlohmann::json::object({
        {"host", config_.gateway.host},
        {"port", config_.gateway.port},
        {"token", config_.gateway.token}
    });
    j["model"] = nlohmann::json::object({
        {"provider", config_.model.provider},
        {"api_key", config_.model.api_key},
        {"base_url", config_.model.base_url},
        {"model", config_.model.model}
    });
    
    file << j.dump(4);
    
    LOG_INFO("Config saved to: ", out_path);
    return true;
}

std::string ConfigManager::expand_env(const std::string& value) const {
    if (value.empty() || value.find("${") == std::string::npos) {
        return value;
    }
    
    std::string result = value;
    size_t start = 0;
    
    while ((start = result.find("${", start)) != std::string::npos) {
        size_t end = result.find("}", start);
        if (end == std::string::npos) break;
        
        std::string env_var = result.substr(start + 2, end - start - 2);
        const char* env_val = std::getenv(env_var.c_str());
        
        if (env_val) {
            result.replace(start, end - start + 1, env_val);
        } else {
            // 环境变量不存在，删除占位符
            result.replace(start, end - start + 1, "");
        }
        
        start = end + 1;
    }
    
    return result;
}

} // namespace openclaw