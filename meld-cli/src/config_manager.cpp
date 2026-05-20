#include "meld/cli/config_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <set>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace meld::cli {

ConfigManager::ConfigManager() {
    // Initialize with empty configuration
}

void ConfigManager::load_all_configs() {
    // Load in reverse precedence order (lowest to highest)
    
    // System global config
    auto system_paths = get_default_config_paths();
    for (const auto& path : system_paths) {
        if (std::filesystem::exists(path)) {
            load_config_file(path, ConfigLevel::SystemGlobal);
        }
    }
    
    // User global config
    auto user_config_path = get_user_config_dir() / "meld" / "config.toml";
    if (std::filesystem::exists(user_config_path)) {
        load_config_file(user_config_path, ConfigLevel::UserGlobal);
    }
    
    // Project configs
    if (project_root_.has_value()) {
        auto project_config_path = get_project_config_path();
        if (std::filesystem::exists(project_config_path)) {
            load_config_file(project_config_path, ConfigLevel::ProjectLocal);
        }
    }
    
    // Environment variables (highest precedence except command line)
    load_environment_config();
}

bool ConfigManager::load_config_file(const std::filesystem::path& config_path, ConfigLevel level) {
    std::map<std::string, std::string> config;
    if (!parse_config_file(config_path, config)) {
        return false;
    }
    
    std::string source = config_path.string();
    for (const auto& [key, value] : config) {
        if (is_valid_key(key)) {
            set_config(key, value, level, source);
        }
    }
    
    return true;
}

void ConfigManager::load_environment_config() {
    // Load environment variables with MELD_ prefix
    const char* env_vars[] = {
        "MELD_DEFAULT_TARGET",
        "MELD_EDITOR",
        "MELD_PACKAGE_REGISTRY",
        "MELD_MCP_PORT",
        "MELD_LOG_LEVEL",
        "MELD_CONFIG_FILE",
        nullptr
    };
    
    for (const char** env_var = env_vars; *env_var != nullptr; ++env_var) {
        const char* value = std::getenv(*env_var);
        if (value != nullptr) {
            std::string key = std::string(*env_var);
            // Convert MELD_DEFAULT_TARGET to default_target
            if (key.substr(0, 5) == "MELD_") {
                key = key.substr(5);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                std::replace(key.begin(), key.end(), '_', '.');
            }
            set_config(key, value, ConfigLevel::Environment, "environment");
        }
    }
}

void ConfigManager::set_config(const std::string& key, const std::string& value, ConfigLevel level, const std::string& source) {
    if (!is_valid_key(key)) {
        return;
    }
    
    ConfigValue config_value;
    config_value.value = value;
    config_value.level = level;
    config_value.source = source.empty() ? "programmatic" : source;
    
    config_by_level_[level][key] = config_value;
}

std::optional<std::string> ConfigManager::get_config(const std::string& key) const {
    auto config_value = get_config_with_source(key);
    if (config_value.has_value()) {
        return config_value->value;
    }
    return std::nullopt;
}

std::optional<ConfigValue> ConfigManager::get_config_with_source(const std::string& key) const {
    // Check levels in precedence order (highest to lowest)
    const std::vector<ConfigLevel> precedence_order = {
        ConfigLevel::CommandLine,
        ConfigLevel::Environment,
        ConfigLevel::ProjectLocal,
        ConfigLevel::ProjectGlobal,
        ConfigLevel::UserGlobal,
        ConfigLevel::SystemGlobal
    };
    
    for (ConfigLevel level : precedence_order) {
        auto level_it = config_by_level_.find(level);
        if (level_it != config_by_level_.end()) {
            auto key_it = level_it->second.find(key);
            if (key_it != level_it->second.end()) {
                return key_it->second;
            }
        }
    }
    
    return std::nullopt;
}

std::vector<std::string> ConfigManager::get_all_keys() const {
    std::set<std::string> unique_keys;
    
    for (const auto& [level, config_map] : config_by_level_) {
        for (const auto& [key, value] : config_map) {
            unique_keys.insert(key);
        }
    }
    
    return std::vector<std::string>(unique_keys.begin(), unique_keys.end());
}

std::map<std::string, std::string> ConfigManager::get_config_at_level(ConfigLevel level) const {
    std::map<std::string, std::string> result;
    
    auto level_it = config_by_level_.find(level);
    if (level_it != config_by_level_.end()) {
        for (const auto& [key, config_value] : level_it->second) {
            result[key] = config_value.value;
        }
    }
    
    return result;
}

bool ConfigManager::save_config_file(const std::filesystem::path& config_path, ConfigLevel level) {
    auto config = get_config_at_level(level);
    return write_config_file(config_path, config);
}

void ConfigManager::clear_all() {
    config_by_level_.clear();
}

void ConfigManager::clear_level(ConfigLevel level) {
    config_by_level_.erase(level);
}

void ConfigManager::set_project_root(const std::filesystem::path& project_root) {
    project_root_ = project_root;
}

std::vector<std::filesystem::path> ConfigManager::get_default_config_paths() const {
    std::vector<std::filesystem::path> paths;
    
#ifdef _WIN32
    paths.push_back("C:\\ProgramData\\Meld\\config.toml");
#else
    paths.push_back("/etc/meld/config.toml");
    paths.push_back("/usr/local/etc/meld/config.toml");
#endif
    
    return paths;
}

bool ConfigManager::is_valid_key(const std::string& key) const {
    // Key must be non-empty and contain only alphanumeric characters, dots, and underscores
    if (key.empty()) {
        return false;
    }
    
    std::regex key_pattern("^[a-zA-Z][a-zA-Z0-9._]*$");
    return std::regex_match(key, key_pattern);
}

std::map<std::string, std::vector<ConfigValue>> ConfigManager::get_config_hierarchy() const {
    std::map<std::string, std::vector<ConfigValue>> hierarchy;
    
    for (const std::string& key : get_all_keys()) {
        std::vector<ConfigValue> values;
        
        // Collect all values for this key across all levels
        for (const auto& [level, config_map] : config_by_level_) {
            auto key_it = config_map.find(key);
            if (key_it != config_map.end()) {
                values.push_back(key_it->second);
            }
        }
        
        // Sort by precedence (highest first)
        std::sort(values.begin(), values.end(), [](const ConfigValue& a, const ConfigValue& b) {
            return static_cast<int>(a.level) < static_cast<int>(b.level);
        });
        
        hierarchy[key] = values;
    }
    
    return hierarchy;
}

std::filesystem::path ConfigManager::get_user_config_dir() const {
#ifdef _WIN32
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path path(appdata);
        free(appdata);
        return path;
    }
    return std::filesystem::path("C:\\Users\\Default\\AppData\\Roaming");
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return std::filesystem::path(home) / ".config";
    }
    
    // Fallback to getpwuid
    struct passwd* pw = getpwuid(getuid());
    if (pw != nullptr) {
        return std::filesystem::path(pw->pw_dir) / ".config";
    }
    
    return std::filesystem::path("/tmp");
#endif
}

std::filesystem::path ConfigManager::get_system_config_dir() const {
#ifdef _WIN32
    return std::filesystem::path("C:\\ProgramData");
#else
    return std::filesystem::path("/etc");
#endif
}

std::filesystem::path ConfigManager::get_project_config_path() const {
    if (!project_root_.has_value()) {
        return std::filesystem::path();
    }
    
    return project_root_.value() / ".meld" / "config.toml";
}

std::string ConfigManager::get_env_var_name(const std::string& key) const {
    std::string env_name = "MELD_" + key;
    std::transform(env_name.begin(), env_name.end(), env_name.begin(), ::toupper);
    std::replace(env_name.begin(), env_name.end(), '.', '_');
    return env_name;
}

bool ConfigManager::parse_config_file(const std::filesystem::path& path, std::map<std::string, std::string>& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        line = std::regex_replace(line, std::regex("^\\s+|\\s+$"), ""); // trim
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key = value format
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim whitespace
            key = std::regex_replace(key, std::regex("^\\s+|\\s+$"), "");
            value = std::regex_replace(value, std::regex("^\\s+|\\s+$"), "");
            
            // Remove quotes from value if present
            if (value.length() >= 2 && 
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.length() - 2);
            }
            
            config[key] = value;
        }
    }
    
    return true;
}

bool ConfigManager::write_config_file(const std::filesystem::path& path, const std::map<std::string, std::string>& config) {
    // Create directory if it doesn't exist
    std::filesystem::create_directories(path.parent_path());
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Meld CLI Configuration" << std::endl;
    file << "# Generated automatically" << std::endl;
    file << std::endl;
    
    for (const auto& [key, value] : config) {
        // Quote values that contain spaces or special characters
        std::string quoted_value = value;
        if (value.find(' ') != std::string::npos || 
            value.find('\t') != std::string::npos ||
            value.find('#') != std::string::npos) {
            quoted_value = "\"" + value + "\"";
        }
        
        file << key << " = " << quoted_value << std::endl;
    }
    
    return true;
}

ConfigLevel ConfigManager::get_highest_precedence_level(const std::string& key) const {
    const std::vector<ConfigLevel> precedence_order = {
        ConfigLevel::CommandLine,
        ConfigLevel::Environment,
        ConfigLevel::ProjectLocal,
        ConfigLevel::ProjectGlobal,
        ConfigLevel::UserGlobal,
        ConfigLevel::SystemGlobal
    };
    
    for (ConfigLevel level : precedence_order) {
        auto level_it = config_by_level_.find(level);
        if (level_it != config_by_level_.end()) {
            auto key_it = level_it->second.find(key);
            if (key_it != level_it->second.end()) {
                return level;
            }
        }
    }
    
    return ConfigLevel::SystemGlobal; // Default fallback
}

} // namespace meld::cli