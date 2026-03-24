#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

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
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    config_path_ = path;
    return load_from_string(content);
}

bool ConfigManager::load_from_string(const std::string& content) {
    try {
        parse_json(content);
        
        // 展开环境变量
        config_.model.api_key = expand_env(config_.model.api_key);
        
        LOG_INFO("Config loaded successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse config: ", e.what());
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
    
    // 简单 JSON 输出
    file << "{\n";
    file << "  \"gateway\": {\n";
    file << "    \"host\": \"" << config_.gateway.host << "\",\n";
    file << "    \"port\": " << config_.gateway.port << ",\n";
    file << "    \"token\": \"" << config_.gateway.token << "\"\n";
    file << "  },\n";
    file << "  \"model\": {\n";
    file << "    \"provider\": \"" << config_.model.provider << "\",\n";
    file << "    \"api_key\": \"" << config_.model.api_key << "\",\n";
    file << "    \"base_url\": \"" << config_.model.base_url << "\",\n";
    file << "    \"model\": \"" << config_.model.model << "\"\n";
    file << "  }\n";
    file << "}\n";
    
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
        }
        
        start = end + 1;
    }
    
    return result;
}

// 简单 JSON 解析
void ConfigManager::parse_json(const std::string& content) {
    size_t pos = 0;
    
    // 跳过空白
    while (pos < content.size() && std::isspace(content[pos])) pos++;
    
    if (pos >= content.size() || content[pos] != '{') {
        throw std::runtime_error("Invalid JSON: expected {");
    }
    pos++;
    
    while (pos < content.size()) {
        // 跳过空白
        while (pos < content.size() && std::isspace(content[pos])) pos++;
        
        if (pos >= content.size()) break;
        
        if (content[pos] == '}') {
            pos++;
            break;
        }
        
        if (content[pos] == ',') {
            pos++;
            continue;
        }
        
        // 解析 key
        std::string key = parse_string(content, pos);
        
        // 跳过空白和冒号
        while (pos < content.size() && (std::isspace(content[pos]) || content[pos] == ':')) pos++;
        
        // 解析 value
        Json value;
        parse_value(content, pos, value);
        
        // 存储到配置
        if (std::holds_alternative<std::string>(value)) {
            std::string full_key = "model." + key;
            set_string(full_key, std::get<std::string>(value));
        } else if (std::holds_alternative<int64_t>(value)) {
            std::string full_key = "model." + key;
            set_int(full_key, static_cast<int>(std::get<int64_t>(value)));
        }
    }
}

void ConfigManager::parse_value(const std::string& json, size_t& pos, Json& result) {
    while (pos < json.size() && std::isspace(json[pos])) pos++;
    
    if (pos >= json.size()) {
        result = nullptr;
        return;
    }
    
    char c = json[pos];
    
    if (c == '"') {
        result = parse_string(json, pos);
    } else if (c == '{') {
        // 对象 - 简化处理
        pos++;
        std::map<std::string, Json> obj;
        while (pos < json.size() && json[pos] != '}') {
            while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ',')) pos++;
            if (json[pos] == '}') break;
            std::string key = parse_string(json, pos);
            while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ':')) pos++;
            Json value;
            parse_value(json, pos, value);
            obj[key] = value;
        }
        if (pos < json.size() && json[pos] == '}') pos++;
        result = obj;
    } else if (c == '[') {
        // 数组 - 简化处理
        pos++;
        std::vector<Json> arr;
        while (pos < json.size() && json[pos] != ']') {
            while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ',')) pos++;
            if (json[pos] == ']') break;
            Json value;
            parse_value(json, pos, value);
            arr.push_back(value);
        }
        if (pos < json.size() && json[pos] == ']') pos++;
        result = arr;
    } else if (c == 't' || c == 'f') {
        // 布尔值
        std::string val = json.substr(pos, c == 't' ? 4 : 5);
        result = (val == "true");
        pos += (c == 't' ? 4 : 5);
    } else if (c == 'n') {
        // null
        result = nullptr;
        pos += 4;
    } else {
        // 数字
        size_t start = pos;
        while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) pos++;
        std::string num_str = json.substr(start, pos - start);
        if (num_str.find('.') != std::string::npos) {
            result = std::stod(num_str);
        } else {
            result = std::stoll(num_str);
        }
    }
}

std::string ConfigManager::parse_string(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') {
        return "";
    }
    pos++; // 跳过开始的引号
    
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    
    if (pos < json.size() && json[pos] == '"') pos++; // 跳过结束的引号
    
    return result;
}

} // namespace openclaw
