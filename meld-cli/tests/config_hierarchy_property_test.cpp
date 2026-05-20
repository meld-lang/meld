#include <gtest/gtest.h>
#include "meld/cli/config_manager.hpp"
#include "meld/testing/property_test.hpp"
#include <algorithm>
#include <set>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 32: Configuration Loading Hierarchy**
 * **Validates: Requirements 11.3, 11.4, 11.5, 11.6**
 * 
 * Property: For any configuration setting, the CLI should load values 
 * according to a clear precedence order (environment > project > global)
 */

// Generator for multiple configuration values at different levels
std::function<std::vector<std::pair<ConfigLevel, std::string>>()> config_hierarchy_values() {
    return []() {
        std::vector<std::pair<ConfigLevel, std::string>> values;
        
        // Generate 2-5 different levels with different values
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> count_dist(2, 5);
        int count = count_dist(gen);
        
        std::vector<ConfigLevel> all_levels = {
            ConfigLevel::Environment,
            ConfigLevel::CommandLine,
            ConfigLevel::ProjectLocal,
            ConfigLevel::ProjectGlobal,
            ConfigLevel::UserGlobal,
            ConfigLevel::SystemGlobal
        };
        
        std::shuffle(all_levels.begin(), all_levels.end(), gen);
        
        for (int i = 0; i < count && i < static_cast<int>(all_levels.size()); ++i) {
            std::string value = "value_" + std::to_string(i) + "_" + std::to_string(static_cast<int>(all_levels[i]));
            values.emplace_back(all_levels[i], value);
        }
        
        return values;
    };
}
// Helper function to get precedence order (higher number = higher precedence)
int get_precedence_order(ConfigLevel level) {
    switch (level) {
        case ConfigLevel::CommandLine: return 6;    // Highest
        case ConfigLevel::Environment: return 5;
        case ConfigLevel::ProjectLocal: return 4;
        case ConfigLevel::ProjectGlobal: return 3;
        case ConfigLevel::UserGlobal: return 2;
        case ConfigLevel::SystemGlobal: return 1;   // Lowest
        default: return 0;
    }
}

TEST(ConfigHierarchyPropertyTest, ConfigurationLoadingHierarchy) {
    // Property: Configuration values should follow precedence hierarchy
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return "test_key_" + std::to_string(Generators::integers(1, 1000)()); }),
        config_hierarchy_values(),
        [&](const std::string& key, const std::vector<std::pair<ConfigLevel, std::string>>& level_values) {
            ConfigManager manager;
            
            // Set configuration values at different levels
            for (const auto& [level, value] : level_values) {
                manager.set_config(key, value, level, "test_source");
            }
            
            // Find the highest precedence level and its value
            ConfigLevel highest_level = ConfigLevel::SystemGlobal;
            std::string expected_value;
            int highest_precedence = 0;
            
            for (const auto& [level, value] : level_values) {
                int precedence = get_precedence_order(level);
                if (precedence > highest_precedence) {
                    highest_precedence = precedence;
                    highest_level = level;
                    expected_value = value;
                }
            }
            
            // Get the configuration value
            auto retrieved = manager.get_config(key);
            if (!retrieved.has_value()) {
                return false;
            }
            
            // Should match the highest precedence value
            if (retrieved.value() != expected_value) {
                return false;
            }
            
            // Verify source information shows the correct level
            auto config_with_source = manager.get_config_with_source(key);
            if (!config_with_source.has_value() || 
                config_with_source->level != highest_level ||
                config_with_source->value != expected_value) {
                return false;
            }
            
            // Test hierarchy information
            auto hierarchy = manager.get_config_hierarchy();
            auto hierarchy_it = hierarchy.find(key);
            if (hierarchy_it == hierarchy.end()) {
                return false;
            }
            
            // Should have all the values we set
            if (hierarchy_it->second.size() != level_values.size()) {
                return false;
            }
            
            // Values should be sorted by precedence (highest first)
            for (size_t i = 1; i < hierarchy_it->second.size(); ++i) {
                int prev_precedence = get_precedence_order(hierarchy_it->second[i-1].level);
                int curr_precedence = get_precedence_order(hierarchy_it->second[i].level);
                if (prev_precedence < curr_precedence) {
                    return false;  // Not properly sorted
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ConfigHierarchyPropertyTest, ConfigurationOverrideConsistency) {
    ConfigManager manager;
    
    // Property: Higher precedence levels should always override lower ones
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return "override_key_" + std::to_string(Generators::integers(1, 1000)()); }),
        std::function<std::string()>([]() { return "low_value_" + std::to_string(Generators::integers(1, 1000)()); }),
        std::function<std::string()>([]() { return "high_value_" + std::to_string(Generators::integers(1, 1000)()); }),
        [&](const std::string& key, const std::string& low_value, const std::string& high_value) {
            // Test all combinations of precedence levels
            std::vector<ConfigLevel> levels = {
                ConfigLevel::SystemGlobal,
                ConfigLevel::UserGlobal,
                ConfigLevel::ProjectGlobal,
                ConfigLevel::ProjectLocal,
                ConfigLevel::Environment,
                ConfigLevel::CommandLine
            };
            
            for (size_t i = 0; i < levels.size(); ++i) {
                for (size_t j = i + 1; j < levels.size(); ++j) {
                    ConfigLevel lower_level = levels[i];
                    ConfigLevel higher_level = levels[j];
                    
                    // Clear previous state
                    manager.clear_all();
                    
                    // Set lower precedence value first
                    manager.set_config(key, low_value, lower_level, "test");
                    
                    // Verify it's retrieved
                    auto retrieved = manager.get_config(key);
                    if (!retrieved.has_value() || retrieved.value() != low_value) {
                        return false;
                    }
                    
                    // Set higher precedence value
                    manager.set_config(key, high_value, higher_level, "test");
                    
                    // Should now retrieve the higher precedence value
                    retrieved = manager.get_config(key);
                    if (!retrieved.has_value() || retrieved.value() != high_value) {
                        return false;
                    }
                    
                    // Source should indicate the higher precedence level
                    auto with_source = manager.get_config_with_source(key);
                    if (!with_source.has_value() || with_source->level != higher_level) {
                        return false;
                    }
                }
            }
            
            return true;
        },
        50  // Fewer iterations since this tests many combinations internally
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ConfigHierarchyPropertyTest, ConfigurationLevelIsolation) {
    ConfigManager manager;
    
    // Property: Clearing one level should not affect other levels
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return "isolation_key_" + std::to_string(Generators::integers(1, 1000)()); }),
        std::function<std::string()>([]() { return "value1_" + std::to_string(Generators::integers(1, 1000)()); }),
        std::function<std::string()>([]() { return "value2_" + std::to_string(Generators::integers(1, 1000)()); }),
        [&](const std::string& key, const std::string& value1, const std::string& value2) {
            // Set values at two different levels
            ConfigLevel level1 = ConfigLevel::UserGlobal;
            ConfigLevel level2 = ConfigLevel::ProjectLocal;
            
            manager.set_config(key, value1, level1, "test1");
            manager.set_config(key, value2, level2, "test2");
            
            // Should get the higher precedence value (ProjectLocal > UserGlobal)
            auto retrieved = manager.get_config(key);
            if (!retrieved.has_value() || retrieved.value() != value2) {
                return false;
            }
            
            // Clear the higher precedence level
            manager.clear_level(level2);
            
            // Should now get the lower precedence value
            retrieved = manager.get_config(key);
            if (!retrieved.has_value() || retrieved.value() != value1) {
                return false;
            }
            
            // Verify the cleared level is actually empty
            auto level2_config = manager.get_config_at_level(level2);
            if (level2_config.find(key) != level2_config.end()) {
                return false;
            }
            
            // Verify the other level still has the value
            auto level1_config = manager.get_config_at_level(level1);
            if (level1_config.find(key) == level1_config.end() || 
                level1_config[key] != value1) {
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}