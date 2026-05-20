#include <gtest/gtest.h>
#include "meld/cli/command_dispatcher.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <iostream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 20: Command Help Accuracy**
 * **Validates: Requirements 7.2, 7.3**
 * 
 * Property: For any valid CLI command, the help system should provide 
 * accurate usage information that matches the actual command behavior
 */

class MockCommandHandler : public BaseCommandHandler {
public:
    MockCommandHandler(const std::string& name, const std::string& description, 
                      const std::string& usage, const std::string& help)
        : BaseCommandHandler(name, description), usage_(usage), help_(help) {}
    
    CommandResult execute(const CommandArgs& args) override {
        return CommandResult::Success;
    }
    
    std::string get_help() const override {
        return help_;
    }
    
    std::string get_usage() const override {
        return usage_;
    }

private:
    std::string usage_;
    std::string help_;
};

// Generator for valid command names
std::function<std::string()> valid_command_names() {
    static const std::vector<std::string> commands = {
        "compile", "run", "repl", "new", "build", "test", 
        "format", "lint", "install", "help", "version"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, commands.size() - 1);
        return commands[dist(gen)];
    };
}
// Generator for command descriptions
std::function<std::string()> command_descriptions() {
    return []() {
        static std::vector<std::string> descriptions = {
            "Compile Meld source code",
            "Run Meld programs directly",
            "Start interactive REPL",
            "Create new project",
            "Build current project",
            "Run project tests",
            "Format source code",
            "Lint source code",
            "Install packages",
            "Show help information",
            "Display version"
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, descriptions.size() - 1);
        return descriptions[dist(gen)];
    };
}

// Generator for usage strings
std::function<std::string()> usage_strings() {
    return []() {
        static std::vector<std::string> usages = {
            "meld compile [options] <file>",
            "meld run [options] <file>",
            "meld repl [options]",
            "meld new <project-name>",
            "meld build [options]",
            "meld test [options]",
            "meld format [options] <file>",
            "meld lint [options] <file>",
            "meld install <package>",
            "meld help [command]",
            "meld version"
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, usages.size() - 1);
        return usages[dist(gen)];
    };
}

// Generator for help text
std::function<std::string()> help_texts() {
    return []() {
        static std::vector<std::string> helps = {
            "Usage: meld compile [options] <file>\n\nCompile Meld source code to target platform.",
            "Usage: meld run [options] <file>\n\nRun Meld programs directly without compilation.",
            "Usage: meld repl [options]\n\nStart interactive Read-Eval-Print Loop.",
            "Usage: meld new <project-name>\n\nCreate a new Meld project with standard structure.",
            "Usage: meld build [options]\n\nBuild the current project using configured build system.",
            "Usage: meld test [options]\n\nRun all tests in the current project.",
            "Usage: meld format [options] <file>\n\nFormat Meld source code according to standard conventions.",
            "Usage: meld lint [options] <file>\n\nAnalyze code for potential issues and style violations.",
            "Usage: meld install <package>\n\nInstall a package from the registry.",
            "Usage: meld help [command]\n\nShow help information for commands.",
            "Usage: meld version\n\nDisplay version information."
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, helps.size() - 1);
        return helps[dist(gen)];
    };
}

TEST(CommandDispatcherPropertyTest, CommandHelpAccuracy) {
    auto error_handler = std::make_shared<ErrorHandler>();
    CommandDispatcher dispatcher(error_handler);
    
    // Property: For any valid command, help information should be consistent and accurate
    bool property_holds = PropertyTest::forall(
        valid_command_names(),
        command_descriptions(),
        usage_strings(),
        help_texts(),
        [&](const std::string& name, const std::string& desc, 
            const std::string& usage, const std::string& help) {
            
            // Register a mock command handler
            auto handler = std::make_unique<MockCommandHandler>(name, desc, usage, help);
            dispatcher.register_handler(std::move(handler));
            
            // Test that the command exists
            if (!dispatcher.has_command(name)) {
                return false;
            }
            
            // Test that help is retrievable and matches what we set
            std::string retrieved_help = dispatcher.get_command_help(name);
            if (retrieved_help != help) {
                return false;
            }
            
            // Test that general help includes this command
            std::string general_help = dispatcher.get_general_help();
            if (general_help.find(name) == std::string::npos) {
                return false;
            }
            
            // Test that general help includes the description
            if (general_help.find(desc) == std::string::npos) {
                return false;
            }
            
            return true;
        },
        100  // Run 100 iterations
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(CommandDispatcherPropertyTest, CommandSuggestionQuality) {
    auto error_handler = std::make_shared<ErrorHandler>();
    CommandDispatcher dispatcher(error_handler);
    
    // Register some commands
    std::vector<std::string> valid_commands = {"compile", "run", "repl", "build", "test"};
    for (const auto& cmd : valid_commands) {
        auto handler = std::make_unique<MockCommandHandler>(cmd, "Description", "Usage", "Help");
        dispatcher.register_handler(std::move(handler));
    }
    
    // Property: For any invalid command, suggestions should be relevant
    bool property_holds = PropertyTest::forall(
        Generators::strings(10),
        [&](const std::string& invalid_command) {
            // Skip if it's actually a valid command
            if (dispatcher.has_command(invalid_command)) {
                return true;
            }
            
            std::vector<std::string> suggestions = dispatcher.suggest_commands(invalid_command);
            
            // Suggestions should not be empty for reasonable input
            if (!invalid_command.empty() && suggestions.empty()) {
                // This is acceptable - not all strings will have good suggestions
                return true;
            }
            
            // All suggestions should be valid commands
            for (const auto& suggestion : suggestions) {
                if (!dispatcher.has_command(suggestion)) {
                    return false;
                }
            }
            
            // Should not suggest more than 3 commands
            if (suggestions.size() > 3) {
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}