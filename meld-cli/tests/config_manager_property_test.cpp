#include <gtest/gtest.h>
#include "meld/cli/config_manager.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <algorithm>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 31: Configuration Storage Persistence**
 * **Validates: Requirements 11.1, 11.2**
 * 
 * Property: For any configuration key-value pair, storing a setting 
 * should persist it across CLI invocations
 */

// Generator for valid configuration keys
std::function<std::string()> valid_config_keys() {
    return []() {
        static std::vector<std::string> keys = {
            "default_target", "editor", "package_registry", "mcp_port",
            "log_level", "enable_colors", "enable_logging", "build_system",
            "test_framework", "optimization_level", "debug_info"
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, keys.size() - 1);
        return keys[dist(gen)];
    };
}

// Generator for configuration values
std::function<std::string()> config_values() {
    return []() {
        static std::vector<std::string> values = {
            "jvm", "go", "cpp", "wasm", "vim", "vscode", "emacs",
            "https://registry.meld-lang.org", "8080", "3000", "5000",
            "debug", "info", "warn", "error", "true", "false",
            "bazel", "cmake", "make", "jest", "gtest", "catch2"
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, values.size() - 1);
        return values[dist(gen)];
    };
}
// Generator for configuration levels
std::function<ConfigLevel()> config_levels() {
    return []() {
        static std::vector<ConfigLevel> levels = {
            ConfigLevel::Environment,
            ConfigLevel::CommandLine,
            ConfigLevel::ProjectLocal,
            ConfigLevel::ProjectGlobal,
            ConfigLevel::UserGlobal,
            ConfigLevel::SystemGlobal
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, levels.size() - 1);
        return levels[dist(gen)];
    };
}

TEST(ConfigManagerPropertyTest, ConfigurationStoragePersistence) {
    // Create a temporary directory for testing
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "meld_cli_test";
    std::filesystem::create_directories(temp_dir);
    
    // Property: For any configuration key-value pair, storing should persist it
    bool property_holds = PropertyTest::forall(
        valid_config_keys(),
        config_values(),
        config_levels(),
        [&](const std::string& key, const std::string& value, ConfigLevel level) {
            ConfigManager manager;
            
            // Set the configuration
            manager.set_config(key, value, level, "test");
            
            // Verify it can be retrieved immediately
            auto retrieved = manager.get_config(key);
            if (!retrieved.has_value() || retrieved.value() != value) {
                return false;
            }
            
            // Verify it appears in the key list
            auto all_keys = manager.get_all_keys();
            if (std::find(all_keys.begin(), all_keys.end(), key) == all_keys.end()) {
                return false;
            }
            
            // Verify source information is correct
            auto config_with_source = manager.get_config_with_source(key);
            if (!config_with_source.has_value() || 
                config_with_source->value != value ||
                config_with_source->level != level ||
                config_with_source->source != "test") {
                return false;
            }
            
            // Test file persistence for appropriate levels
            if (level == ConfigLevel::UserGlobal || level == ConfigLevel::ProjectLocal) {
                std::filesystem::path config_file = temp_dir / "test_config.toml";
                
                // Save to file
                if (!manager.save_config_file(config_file, level)) {
                    return false;
                }
                
                // Create new manager and load from file
                ConfigManager new_manager;
                if (!new_manager.load_config_file(config_file, level)) {
                    return false;
                }
                
                // Verify the value persisted
                auto persisted = new_manager.get_config(key);
                if (!persisted.has_value() || persisted.value() != value) {
                    return false;
                }
                
                // Clean up
                std::filesystem::remove(config_file);
            }
            
            return true;
        },
        100
    );
    
    // Clean up
    std::filesystem::remove_all(temp_dir);
    
    EXPECT_TRUE(property_holds);
}

TEST(ConfigManagerPropertyTest, ConfigurationKeyValidation) {
    ConfigManager manager;
    
    // Property: Valid keys should be accepted, invalid keys should be rejected
    bool property_holds = PropertyTest::forall(
        Generators::strings(20),
        [&](const std::string& key) {
            bool is_valid = manager.is_valid_key(key);
            
            // Test the validation logic
            if (key.empty()) {
                return !is_valid;  // Empty keys should be invalid
            }
            
            // Keys starting with non-alpha should be invalid
            if (!std::isalpha(key[0])) {
                return !is_valid;
            }
            
            // Keys with invalid characters should be invalid
            for (char c : key) {
                if (!std::isalnum(c) && c != '.' && c != '_') {
                    return !is_valid;
                }
            }
            
            // If we get here, the key should be valid
            return is_valid;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ConfigManagerPropertyTest, ConfigurationClearOperations) {
    ConfigManager manager;
    
    // Property: Clear operations should remove configurations correctly
    bool property_holds = PropertyTest::forall(
        valid_config_keys(),
        config_values(),
        config_levels(),
        [&](const std::string& key, const std::string& value, ConfigLevel level) {
            // Set a configuration
            manager.set_config(key, value, level, "test");
            
            // Verify it exists
            if (!manager.get_config(key).has_value()) {
                return false;
            }
            
            // Clear the specific level
            manager.clear_level(level);
            
            // Verify it's gone (assuming no other levels have this key)
            auto remaining = manager.get_config(key);
            
            // If there are other levels with this key, it might still exist
            // So we check that at least this level is cleared
            auto config_at_level = manager.get_config_at_level(level);
            if (config_at_level.find(key) != config_at_level.end()) {
                return false;
            }
            
            // Test clear all
            manager.set_config(key, value, level, "test");
            manager.clear_all();
            
            // Should be completely gone now
            if (manager.get_config(key).has_value()) {
                return false;
            }
            
            // Key list should be empty
            auto all_keys = manager.get_all_keys();
            if (!all_keys.empty()) {
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}