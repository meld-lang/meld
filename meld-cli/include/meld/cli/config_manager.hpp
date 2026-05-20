#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <filesystem>

namespace meld::cli {

/**
 * Configuration precedence levels
 */
enum class ConfigLevel {
    Environment,    // Highest precedence
    CommandLine,
    ProjectLocal,
    ProjectGlobal,
    UserGlobal,
    SystemGlobal    // Lowest precedence
};

/**
 * Configuration value with source tracking
 */
struct ConfigValue {
    std::string value;
    ConfigLevel level;
    std::string source;  // File path or "environment" or "command-line"
};

/**
 * Configuration management system with precedence rules
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager() = default;

    /**
     * Load configuration from all sources with proper precedence
     */
    void load_all_configs();

    /**
     * Load configuration from a specific file
     */
    bool load_config_file(const std::filesystem::path& config_path, ConfigLevel level);

    /**
     * Load configuration from environment variables
     */
    void load_environment_config();

    /**
     * Set configuration value programmatically
     */
    void set_config(const std::string& key, const std::string& value, ConfigLevel level, const std::string& source = "");

    /**
     * Get configuration value with precedence resolution
     */
    std::optional<std::string> get_config(const std::string& key) const;

    /**
     * Get configuration value with source information
     */
    std::optional<ConfigValue> get_config_with_source(const std::string& key) const;

    /**
     * Get all configuration keys
     */
    std::vector<std::string> get_all_keys() const;

    /**
     * Get all configuration values at a specific level
     */
    std::map<std::string, std::string> get_config_at_level(ConfigLevel level) const;

    /**
     * Save configuration to file
     */
    bool save_config_file(const std::filesystem::path& config_path, ConfigLevel level);

    /**
     * Clear all configuration
     */
    void clear_all();

    /**
     * Clear configuration at specific level
     */
    void clear_level(ConfigLevel level);

    /**
     * Set project root directory for project-local config
     */
    void set_project_root(const std::filesystem::path& project_root);

    /**
     * Get default configuration file paths
     */
    std::vector<std::filesystem::path> get_default_config_paths() const;

    /**
     * Validate configuration key format
     */
    bool is_valid_key(const std::string& key) const;

    /**
     * Get configuration hierarchy for debugging
     */
    std::map<std::string, std::vector<ConfigValue>> get_config_hierarchy() const;

private:
    // Configuration storage by level
    std::map<ConfigLevel, std::map<std::string, ConfigValue>> config_by_level_;
    
    // Project root for project-local configuration
    std::optional<std::filesystem::path> project_root_;

    // Helper methods
    std::filesystem::path get_user_config_dir() const;
    std::filesystem::path get_system_config_dir() const;
    std::filesystem::path get_project_config_path() const;
    std::string get_env_var_name(const std::string& key) const;
    bool parse_config_file(const std::filesystem::path& path, std::map<std::string, std::string>& config);
    bool write_config_file(const std::filesystem::path& path, const std::map<std::string, std::string>& config);
    ConfigLevel get_highest_precedence_level(const std::string& key) const;
};

} // namespace meld::cli