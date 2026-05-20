#include <gtest/gtest.h>
#include "meld/cli/shell_integration_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <map>
#include <iostream>

using namespace meld::cli;
using namespace meld::testing;
using json = nlohmann::json;

/**
 * **Feature: meld-cli, Property 33: JSON Output Validity**
 * **Validates: Requirements 12.4**
 * 
 * Property: For any command that supports JSON output, the output should be 
 * valid JSON with a consistent schema
 */

// Generator for command names
std::function<std::string()> command_name_generator() {
    static const std::vector<std::string> commands = {
        "compile", "run", "repl", "new", "build", "test", "clean",
        "format", "lint", "install", "publish", "search", "config",
        "lsp", "mcp", "help", "version", "completion"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, commands.size() - 1);
        return commands[dist(gen)];
    };
}

// Generator for key-value data maps
std::function<std::map<std::string, std::string>()> data_map_generator() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> size_dist(0, 10);
        std::uniform_int_distribution<size_t> str_len_dist(1, 20);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        std::map<std::string, std::string> data;
        size_t map_size = size_dist(gen);
        
        for (size_t i = 0; i < map_size; ++i) {
            // Generate random key
            size_t key_len = str_len_dist(gen);
            std::string key;
            key.reserve(key_len);
            for (size_t j = 0; j < key_len; ++j) {
                key += char_dist(gen);
            }
            
            // Generate random value
            size_t val_len = str_len_dist(gen);
            std::string value;
            value.reserve(val_len);
            for (size_t j = 0; j < val_len; ++j) {
                value += char_dist(gen);
            }
            
            data[key] = value;
        }
        
        return data;
    };
}

// Generator for string lists
std::function<std::vector<std::string>()> string_list_generator() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> size_dist(0, 15);
        std::uniform_int_distribution<size_t> str_len_dist(1, 30);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        std::vector<std::string> items;
        size_t list_size = size_dist(gen);
        
        for (size_t i = 0; i < list_size; ++i) {
            size_t str_len = str_len_dist(gen);
            std::string item;
            item.reserve(str_len);
            for (size_t j = 0; j < str_len; ++j) {
                item += char_dist(gen);
            }
            items.push_back(item);
        }
        
        return items;
    };
}

// Generator for error messages
std::function<std::string()> error_message_generator() {
    static const std::vector<std::string> error_templates = {
        "File not found: {}",
        "Compilation failed with {} errors",
        "Invalid argument: {}",
        "Network error: {}",
        "Permission denied: {}",
        "Syntax error at line {}: {}",
        "Unknown command: {}"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> template_dist(0, error_templates.size() - 1);
        std::uniform_int_distribution<size_t> str_len_dist(5, 50);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        std::string error_template = error_templates[template_dist(gen)];
        
        // Generate random details
        size_t detail_len = str_len_dist(gen);
        std::string detail;
        detail.reserve(detail_len);
        for (size_t i = 0; i < detail_len; ++i) {
            detail += char_dist(gen);
        }
        
        // Simple template replacement
        size_t pos = error_template.find("{}");
        if (pos != std::string::npos) {
            error_template.replace(pos, 2, detail);
        }
        
        return error_template;
    };
}

TEST(JsonOutputValidityPropertyTest, CommandDataFormatting) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: For any command and data map, JSON output should be valid and well-formed
    bool property_holds = PropertyTest::forall(
        command_name_generator(),
        data_map_generator(),
        [&](const std::string& command, const std::map<std::string, std::string>& data) {
            
            json result = shell_module.format_as_json(command, data);
            
            // Check that result is valid JSON (nlohmann::json throws on invalid JSON)
            try {
                std::string json_str = result.dump();
                json parsed = json::parse(json_str);
                
                // Verify required schema fields
                if (!result.contains("command")) {
                    std::cerr << "Missing 'command' field in JSON output" << std::endl;
                    return false;
                }
                
                if (!result.contains("timestamp")) {
                    std::cerr << "Missing 'timestamp' field in JSON output" << std::endl;
                    return false;
                }
                
                if (!result.contains("data")) {
                    std::cerr << "Missing 'data' field in JSON output" << std::endl;
                    return false;
                }
                
                // Verify field types
                if (!result["command"].is_string()) {
                    std::cerr << "Command field is not a string" << std::endl;
                    return false;
                }
                
                if (!result["timestamp"].is_number()) {
                    std::cerr << "Timestamp field is not a number" << std::endl;
                    return false;
                }
                
                if (!result["data"].is_object()) {
                    std::cerr << "Data field is not an object" << std::endl;
                    return false;
                }
                
                // Verify command value matches input
                if (result["command"].get<std::string>() != command) {
                    std::cerr << "Command field value doesn't match input" << std::endl;
                    return false;
                }
                
                // Verify data content matches input
                for (const auto& [key, value] : data) {
                    if (!result["data"].contains(key)) {
                        std::cerr << "Missing data key: " << key << std::endl;
                        return false;
                    }
                    
                    if (!result["data"][key].is_string()) {
                        std::cerr << "Data value is not a string for key: " << key << std::endl;
                        return false;
                    }
                    
                    if (result["data"][key].get<std::string>() != value) {
                        std::cerr << "Data value doesn't match for key: " << key << std::endl;
                        return false;
                    }
                }
                
                return true;
                
            } catch (const json::exception& e) {
                std::cerr << "Invalid JSON generated: " << e.what() << std::endl;
                return false;
            }
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(JsonOutputValidityPropertyTest, ErrorFormatting) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: For any error message, JSON error output should be valid and well-formed
    bool property_holds = PropertyTest::forall(
        error_message_generator(),
        [&](const std::string& error_message) {
            json result = shell_module.format_error_as_json(error_message);
            
            try {
                std::string json_str = result.dump();
                json parsed = json::parse(json_str);
                
                // Verify required schema fields for errors
                if (!result.contains("success")) {
                    std::cerr << "Missing 'success' field in error JSON" << std::endl;
                    return false;
                }
                
                if (!result.contains("error")) {
                    std::cerr << "Missing 'error' field in error JSON" << std::endl;
                    return false;
                }
                
                // Verify success is false for errors
                if (!result["success"].is_boolean() || result["success"].get<bool>() != false) {
                    std::cerr << "Success field should be false for errors" << std::endl;
                    return false;
                }
                
                // Verify error object structure
                if (!result["error"].is_object()) {
                    std::cerr << "Error field is not an object" << std::endl;
                    return false;
                }
                
                auto error_obj = result["error"];
                if (!error_obj.contains("message") || !error_obj["message"].is_string()) {
                    std::cerr << "Error object missing or invalid message field" << std::endl;
                    return false;
                }
                
                if (!error_obj.contains("code") || !error_obj["code"].is_string()) {
                    std::cerr << "Error object missing or invalid code field" << std::endl;
                    return false;
                }
                
                if (!error_obj.contains("timestamp") || !error_obj["timestamp"].is_number()) {
                    std::cerr << "Error object missing or invalid timestamp field" << std::endl;
                    return false;
                }
                
                // Verify message content
                if (error_obj["message"].get<std::string>() != error_message) {
                    std::cerr << "Error message doesn't match input" << std::endl;
                    return false;
                }
                
                return true;
                
            } catch (const json::exception& e) {
                std::cerr << "Invalid JSON generated for error: " << e.what() << std::endl;
                return false;
            }
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(JsonOutputValidityPropertyTest, ListFormatting) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: For any list of strings, JSON list output should be valid and well-formed
    bool property_holds = PropertyTest::forall(
        string_list_generator(),
        [&](const std::vector<std::string>& items) {
            json result = shell_module.format_list_as_json(items);
            
            try {
                std::string json_str = result.dump();
                json parsed = json::parse(json_str);
                
                // Verify required schema fields
                if (!result.contains("items")) {
                    std::cerr << "Missing 'items' field in list JSON" << std::endl;
                    return false;
                }
                
                if (!result.contains("count")) {
                    std::cerr << "Missing 'count' field in list JSON" << std::endl;
                    return false;
                }
                
                if (!result.contains("timestamp")) {
                    std::cerr << "Missing 'timestamp' field in list JSON" << std::endl;
                    return false;
                }
                
                // Verify field types
                if (!result["items"].is_array()) {
                    std::cerr << "Items field is not an array" << std::endl;
                    return false;
                }
                
                if (!result["count"].is_number_integer()) {
                    std::cerr << "Count field is not an integer" << std::endl;
                    return false;
                }
                
                if (!result["timestamp"].is_number()) {
                    std::cerr << "Timestamp field is not a number" << std::endl;
                    return false;
                }
                
                // Verify count matches array size
                if (result["count"].get<size_t>() != items.size()) {
                    std::cerr << "Count doesn't match items array size" << std::endl;
                    return false;
                }
                
                // Verify array contents
                auto json_items = result["items"];
                if (json_items.size() != items.size()) {
                    std::cerr << "JSON items array size doesn't match input" << std::endl;
                    return false;
                }
                
                for (size_t i = 0; i < items.size(); ++i) {
                    if (!json_items[i].is_string()) {
                        std::cerr << "Item " << i << " is not a string" << std::endl;
                        return false;
                    }
                    
                    if (json_items[i].get<std::string>() != items[i]) {
                        std::cerr << "Item " << i << " doesn't match input" << std::endl;
                        return false;
                    }
                }
                
                return true;
                
            } catch (const json::exception& e) {
                std::cerr << "Invalid JSON generated for list: " << e.what() << std::endl;
                return false;
            }
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(JsonOutputValidityPropertyTest, ResultFormatting) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Generator for success/failure results
    auto result_generator = []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> bool_dist(0, 1);
        std::uniform_int_distribution<size_t> str_len_dist(5, 50);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        bool success = bool_dist(gen) == 1;
        
        size_t msg_len = str_len_dist(gen);
        std::string message;
        message.reserve(msg_len);
        for (size_t i = 0; i < msg_len; ++i) {
            message += char_dist(gen);
        }
        
        return std::make_pair(success, message);
    };
    
    // Property: For any success/failure result, JSON output should be valid and well-formed
    bool property_holds = PropertyTest::forall(
        std::function<std::pair<bool, std::string>()>(result_generator),
        [&](const std::pair<bool, std::string>& input) {
            const auto& [success, message] = input;
            
            json result = shell_module.format_result_as_json(success, message);
            
            try {
                std::string json_str = result.dump();
                json parsed = json::parse(json_str);
                
                // Verify required schema fields
                if (!result.contains("success")) {
                    std::cerr << "Missing 'success' field in result JSON" << std::endl;
                    return false;
                }
                
                if (!result.contains("message")) {
                    std::cerr << "Missing 'message' field in result JSON" << std::endl;
                    return false;
                }
                
                if (!result.contains("timestamp")) {
                    std::cerr << "Missing 'timestamp' field in result JSON" << std::endl;
                    return false;
                }
                
                // Verify field types
                if (!result["success"].is_boolean()) {
                    std::cerr << "Success field is not a boolean" << std::endl;
                    return false;
                }
                
                if (!result["message"].is_string()) {
                    std::cerr << "Message field is not a string" << std::endl;
                    return false;
                }
                
                if (!result["timestamp"].is_number()) {
                    std::cerr << "Timestamp field is not a number" << std::endl;
                    return false;
                }
                
                // Verify field values
                if (result["success"].get<bool>() != success) {
                    std::cerr << "Success field value doesn't match input" << std::endl;
                    return false;
                }
                
                if (result["message"].get<std::string>() != message) {
                    std::cerr << "Message field value doesn't match input" << std::endl;
                    return false;
                }
                
                return true;
                
            } catch (const json::exception& e) {
                std::cerr << "Invalid JSON generated for result: " << e.what() << std::endl;
                return false;
            }
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(JsonOutputValidityPropertyTest, SchemaConsistency) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Test that all JSON outputs follow consistent schema patterns
    
    // Test command data format
    json cmd_result = shell_module.format_as_json("test", {{"key", "value"}});
    EXPECT_TRUE(cmd_result.contains("command"));
    EXPECT_TRUE(cmd_result.contains("timestamp"));
    EXPECT_TRUE(cmd_result.contains("data"));
    
    // Test error format
    json error_result = shell_module.format_error_as_json("test error");
    EXPECT_TRUE(error_result.contains("success"));
    EXPECT_TRUE(error_result.contains("error"));
    EXPECT_FALSE(error_result["success"].get<bool>());
    
    // Test list format
    json list_result = shell_module.format_list_as_json({"item1", "item2"});
    EXPECT_TRUE(list_result.contains("items"));
    EXPECT_TRUE(list_result.contains("count"));
    EXPECT_TRUE(list_result.contains("timestamp"));
    
    // Test result format
    json result_success = shell_module.format_result_as_json(true, "success");
    EXPECT_TRUE(result_success.contains("success"));
    EXPECT_TRUE(result_success.contains("message"));
    EXPECT_TRUE(result_success.contains("timestamp"));
    EXPECT_TRUE(result_success["success"].get<bool>());
}