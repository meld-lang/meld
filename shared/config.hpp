#pragma once

/**
 * @file config.hpp
 * @brief Shared configuration utilities for all Meld packages
 */

#include <string>
#include <map>
#include <optional>

namespace meld::shared {

/**
 * @brief Shared configuration manager for all packages
 */
class Config {
public:
    /**
     * @brief Load configuration from file
     * @param config_file Path to configuration file
     * @return True if loaded successfully
     */
    static bool load_from_file(const std::string& config_file);
    
    /**
     * @brief Get configuration value
     * @param key Configuration key
     * @return Configuration value if found
     */
    static std::optional<std::string> get(const std::string& key);
    
    /**
     * @brief Set configuration value
     * @param key Configuration key
     * @param value Configuration value
     */
    static void set(const std::string& key, const std::string& value);
    
    /**
     * @brief Get configuration value with default
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    static std::string get_or_default(const std::string& key, 
                                     const std::string& default_value);

private:
    static std::map<std::string, std::string> config_map_;
};

} // namespace meld::shared