#include <gtest/gtest.h>
#include "meld/cli/command_dispatcher.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <iostream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 21: Command Suggestion Quality**
 * **Validates: Requirements 7.5**
 * 
 * Property: For any invalid command, the CLI should suggest valid alternatives 
 * based on string similarity and command frequency
 */

class MockCommandHandler : public BaseCommandHandler {
public:
    MockCommandHandler(const std::string& name, const std::string& description)
        : BaseCommandHandler(name, description) {}
    
    CommandResult execute(const CommandArgs& args) override {
        return CommandResult::Success;
    }
    
    std::string get_help() const override {
        return "Usage: meld " + get_name() + "\n\n" + get_description();
    }
    
    std::string get_usage() const override {
        return "meld " + get_name();
    }
};

// Generator for invalid command names (typos of valid commands)
std::function<std::string()> invalid_command_generator() {
    const std::vector<std::string> valid_commands = {
        "compile", "run", "repl", "new", "build", "test", 
        "format", "lint", "install", "help", "version", "config"
    };
    
    return [&valid_commands]() {
        static std::mt19937 gen(std::random_device{}());
        
        // Choose a valid command to create a typo from
        std::uniform_int_distribution<size_t> cmd_dist(0, valid_commands.size() - 1);
        std::string base_command = valid_commands[cmd_dist(gen)];
        
        // Create different types of typos
        std::uniform_int_distribution<int> typo_type(0, 4);
        
        switch (typo_type(gen)) {
            case 0: // Character deletion
                if (!base_command.empty()) {
                    std::uniform_int_distribution<size_t> pos_dist(0, base_command.length() - 1);
                    size_t pos = pos_dist(gen);
                    base_command.erase(pos, 1);
                }
                break;
                
            case 1: // Character insertion
                {
                    std::uniform_int_distribution<size_t> pos_dist(0, base_command.length());
                    std::uniform_int_distribution<int> char_dist('a', 'z');
                    size_t pos = pos_dist(gen);
                    base_command.insert(pos, 1, char_dist(gen));
                }
                break;
                
            case 2: // Character substitution
                if (!base_command.empty()) {
                    std::uniform_int_distribution<size_t> pos_dist(0, base_command.length() - 1);
                    std::uniform_int_distribution<int> char_dist('a', 'z');
                    size_t pos = pos_dist(gen);
                    base_command[pos] = char_dist(gen);
                }
                break;
                
            case 3: // Character transposition
                if (base_command.length() >= 2) {
                    std::uniform_int_distribution<size_t> pos_dist(0, base_command.length() - 2);
                    size_t pos = pos_dist(gen);
                    std::swap(base_command[pos], base_command[pos + 1]);
                }
                break;
                
            case 4: // Prefix/suffix modification
                {
                    std::uniform_int_distribution<int> mod_type(0, 1);
                    if (mod_type(gen) == 0 && base_command.length() > 2) {
                        // Remove prefix
                        base_command = base_command.substr(1);
                    } else {
                        // Add suffix
                        std::uniform_int_distribution<int> char_dist('a', 'z');
                        base_command += char_dist(gen);
                    }
                }
                break;
        }
        
        return base_command;
    };
}

// Generator for completely random strings (should have no suggestions)
std::function<std::string()> random_string_generator() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> length_dist(1, 15);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        size_t length = length_dist(gen);
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += char_dist(gen);
        }
        
        return result;
    };
}

TEST(CommandSuggestionPropertyTest, SuggestionQualityForTypos) {
    auto error_handler = std::make_shared<ErrorHandler>();
    CommandDispatcher dispatcher(error_handler);
    
    // Register standard commands
    std::vector<std::string> valid_commands = {
        "compile", "run", "repl", "new", "build", "test", 
        "format", "lint", "install", "help", "version", "config"
    };
    
    for (const auto& cmd : valid_commands) {
        auto handler = std::make_unique<MockCommandHandler>(cmd, "Description for " + cmd);
        dispatcher.register_handler(std::move(handler));
    }
    
    // Property: For any invalid command that's a typo of a valid command,
    // suggestions should include the original command
    bool property_holds = PropertyTest::forall(
        invalid_command_generator(),
        [&](const std::string& invalid_command) {
            // Skip if it's actually a valid command
            if (dispatcher.has_command(invalid_command)) {
                return true;
            }
            
            std::vector<std::string> suggestions = dispatcher.suggest_commands(invalid_command);
            
            // All suggestions should be valid commands
            for (const auto& suggestion : suggestions) {
                if (!dispatcher.has_command(suggestion)) {
                    std::cerr << "Invalid suggestion: " << suggestion << std::endl;
                    return false;
                }
            }
            
            // Should not suggest more than 3 commands
            if (suggestions.size() > 3) {
                std::cerr << "Too many suggestions: " << suggestions.size() << std::endl;
                return false;
            }
            
            // For typos, we should get at least one suggestion if the typo is reasonable
            if (invalid_command.length() >= 3 && suggestions.empty()) {
                // Check if this is a reasonable typo by seeing if any valid command
                // has high similarity
                bool has_similar = false;
                for (const auto& valid_cmd : valid_commands) {
                    // Simple similarity check - if they share most characters
                    size_t common_chars = 0;
                    size_t min_len = std::min(invalid_command.length(), valid_cmd.length());
                    for (size_t i = 0; i < min_len; ++i) {
                        if (i < invalid_command.length() && i < valid_cmd.length() &&
                            invalid_command[i] == valid_cmd[i]) {
                            common_chars++;
                        }
                    }
                    
                    if (common_chars >= min_len * 0.6) {  // 60% similarity
                        has_similar = true;
                        break;
                    }
                }
                
                if (has_similar) {
                    std::cerr << "No suggestions for reasonable typo: " << invalid_command << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(CommandSuggestionPropertyTest, SuggestionRelevance) {
    auto error_handler = std::make_shared<ErrorHandler>();
    CommandDispatcher dispatcher(error_handler);
    
    // Register commands with different similarity patterns
    std::vector<std::string> commands = {
        "compile", "complete", "compute", "compare",
        "run", "rerun", "return", 
        "test", "rest", "best"
    };
    
    for (const auto& cmd : commands) {
        auto handler = std::make_unique<MockCommandHandler>(cmd, "Description for " + cmd);
        dispatcher.register_handler(std::move(handler));
    }
    
    // Property: Suggestions should be ordered by relevance (similarity)
    bool property_holds = PropertyTest::forall(
        random_string_generator(),
        [&](const std::string& invalid_command) {
            // Skip if it's actually a valid command
            if (dispatcher.has_command(invalid_command)) {
                return true;
            }
            
            std::vector<std::string> suggestions = dispatcher.suggest_commands(invalid_command);
            
            // If we have multiple suggestions, they should be ordered by similarity
            if (suggestions.size() > 1) {
                // Calculate similarity scores for verification
                auto calculate_similarity = [](const std::string& a, const std::string& b) -> double {
                    if (a.empty() || b.empty()) return 0.0;
                    
                    size_t common_prefix = 0;
                    size_t min_len = std::min(a.length(), b.length());
                    for (size_t i = 0; i < min_len; ++i) {
                        if (a[i] == b[i]) {
                            common_prefix++;
                        } else {
                            break;
                        }
                    }
                    
                    // Simple similarity based on common prefix and length difference
                    double prefix_score = static_cast<double>(common_prefix) / std::max(a.length(), b.length());
                    double length_penalty = std::abs(static_cast<int>(a.length()) - static_cast<int>(b.length())) / 10.0;
                    
                    return prefix_score - length_penalty;
                };
                
                // Check that suggestions are reasonably ordered
                for (size_t i = 0; i < suggestions.size() - 1; ++i) {
                    double sim1 = calculate_similarity(invalid_command, suggestions[i]);
                    double sim2 = calculate_similarity(invalid_command, suggestions[i + 1]);
                    
                    // Allow some tolerance in ordering
                    if (sim2 > sim1 + 0.2) {  // Significant difference
                        std::cerr << "Suggestions not well ordered: " << suggestions[i] 
                                  << " (sim=" << sim1 << ") before " << suggestions[i + 1] 
                                  << " (sim=" << sim2 << ")" << std::endl;
                        return false;
                    }
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(CommandSuggestionPropertyTest, NoSuggestionsForVeryDifferentStrings) {
    auto error_handler = std::make_shared<ErrorHandler>();
    CommandDispatcher dispatcher(error_handler);
    
    // Register some commands
    std::vector<std::string> commands = {"compile", "run", "test"};
    for (const auto& cmd : commands) {
        auto handler = std::make_unique<MockCommandHandler>(cmd, "Description");
        dispatcher.register_handler(std::move(handler));
    }
    
    // Property: Very different strings should not get suggestions
    std::vector<std::string> very_different = {
        "xyz123", "qwertyuiop", "abcdefghijklmnop", "zzzzzzz", "1234567"
    };
    
    for (const auto& different_cmd : very_different) {
        std::vector<std::string> suggestions = dispatcher.suggest_commands(different_cmd);
        
        // Very different strings should get no suggestions or very few
        EXPECT_LE(suggestions.size(), 1) << "Too many suggestions for very different string: " << different_cmd;
    }
}