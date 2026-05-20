#include "config.hpp"
#include <fstream>
#include <sstream>

namespace meld::shared {

std::map<std::string, std::string> Config::config_map_;

bool Config::load_from_file(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value format
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config_map_[key] = value;
        }
    }
    
    return true;
}

std::optional<std::string> Config::get(const std::string& key) {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Config::set(const std::string& key, const std::string& value) {
    config_map_[key] = value;
}

std::string Config::get_or_default(const std::string& key, 
                                  const std::string& default_value) {
    auto value = get(key);
    return value ? *value : default_value;
}

} // namespace meld::shared