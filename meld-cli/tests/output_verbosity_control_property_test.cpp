#include <gtest/gtest.h>
#include "meld/cli/shell_integration_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <iostream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 34: Output Verbosity Control**
 * **Validates: Requirements 12.5, 12.6**
 * 
 * Property: For any command, using quiet flag should reduce output while verbose flag 
 * should increase output, with normal mode between them
 */

// Custom output capture for testing
class OutputCapture {
public:
    OutputCapture() : old_cout_buf_(std::cout.rdbuf()) {
        std::cout.rdbuf(buffer_.rdbuf());
    }
    
    ~OutputCapture() {
        std::cout.rdbuf(old_cout_buf_);
    }
    
    std::string get_output() const {
        return buffer_.str();
    }
    
    void clear() {
        buffer_.str("");
        buffer_.clear();
    }
    
private:
    std::ostringstream buffer_;
    std::streambuf* old_cout_buf_;
};

// Generator for message content
std::function<std::string()> message_generator() {
    static std::vector<std::string> message_templates = {
        "Processing file: {}",
        "Compilation completed successfully",
        "Warning: {} deprecated feature used",
        "Debug: {} function called",
        "Info: {} configuration loaded",
        "Verbose: {} detailed operation info",
        "Error: {} operation failed",
        "Status: {} items processed"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> template_dist(0, message_templates.size() - 1);
        std::uniform_int_distribution<size_t> str_len_dist(5, 30);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        std::string message_template = message_templates[template_dist(gen)];
        
        // Generate random details
        size_t detail_len = str_len_dist(gen);
        std::string detail;
        detail.reserve(detail_len);
        for (size_t i = 0; i < detail_len; ++i) {
            detail += char_dist(gen);
        }
        
        // Simple template replacement
        size_t pos = message_template.find("{}");
        if (pos != std::string::npos) {
            message_template.replace(pos, 2, detail);
        }
        
        return message_template;
    };
}

// Generator for verbosity levels
std::function<VerbosityLevel()> verbosity_level_generator() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> level_dist(0, 2);
        
        switch (level_dist(gen)) {
            case 0: return VerbosityLevel::Quiet;
            case 1: return VerbosityLevel::Normal;
            case 2: return VerbosityLevel::Verbose;
            default: return VerbosityLevel::Normal;
        }
    };
}

TEST(OutputVerbosityControlPropertyTest, VerbosityLevelFiltering) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: Messages should be filtered according to verbosity level
    bool property_holds = PropertyTest::forall(
        message_generator(),
        verbosity_level_generator(),
        [&](const std::string& message, const VerbosityLevel& target_level) {
            
            // Test all combinations of module verbosity and message levels
            std::vector<VerbosityLevel> all_levels = {
                VerbosityLevel::Quiet, VerbosityLevel::Normal, VerbosityLevel::Verbose
            };
            
            for (VerbosityLevel module_level : all_levels) {
                shell_module.set_verbosity(module_level);
                
                for (VerbosityLevel message_level : all_levels) {
                    OutputCapture capture;
                    
                    shell_module.output_message(message, message_level);
                    std::string output = capture.get_output();
                    
                    bool should_show = shell_module.should_output_message(message_level);
                    bool actually_shown = !output.empty();
                    
                    if (should_show != actually_shown) {
                        std::cerr << "Verbosity filtering failed: module=" 
                                  << static_cast<int>(module_level)
                                  << ", message=" << static_cast<int>(message_level)
                                  << ", should_show=" << should_show
                                  << ", actually_shown=" << actually_shown << std::endl;
                        return false;
                    }
                    
                    // If shown, verify the message content is present
                    if (actually_shown && output.find(message) == std::string::npos) {
                        std::cerr << "Message content not found in output" << std::endl;
                        return false;
                    }
                }
            }
            
            return true;
        },
        50  // Reduced iterations due to nested loops
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, QuietModeSuppressionProperty) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: In quiet mode, only quiet-level messages should be shown
    bool property_holds = PropertyTest::forall(
        message_generator(),
        [&](const std::string& message) {
            shell_module.set_verbosity(VerbosityLevel::Quiet);
            
            // Test normal level message (should be suppressed)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Normal);
                std::string output = capture.get_output();
                
                if (!output.empty()) {
                    std::cerr << "Normal message shown in quiet mode: " << message << std::endl;
                    return false;
                }
            }
            
            // Test verbose level message (should be suppressed)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Verbose);
                std::string output = capture.get_output();
                
                if (!output.empty()) {
                    std::cerr << "Verbose message shown in quiet mode: " << message << std::endl;
                    return false;
                }
            }
            
            // Test quiet level message (should be shown)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Quiet);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "Quiet message not shown in quiet mode: " << message << std::endl;
                    return false;
                }
                
                if (output.find(message) == std::string::npos) {
                    std::cerr << "Quiet message content not found in output" << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, VerboseModeInclusionProperty) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: In verbose mode, all message levels should be shown
    bool property_holds = PropertyTest::forall(
        message_generator(),
        [&](const std::string& message) {
            shell_module.set_verbosity(VerbosityLevel::Verbose);
            
            // Test all message levels (all should be shown)
            std::vector<VerbosityLevel> levels = {
                VerbosityLevel::Quiet, VerbosityLevel::Normal, VerbosityLevel::Verbose
            };
            
            for (VerbosityLevel level : levels) {
                OutputCapture capture;
                shell_module.output_message(message, level);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "Message not shown in verbose mode: level=" 
                              << static_cast<int>(level) << ", message=" << message << std::endl;
                    return false;
                }
                
                if (output.find(message) == std::string::npos) {
                    std::cerr << "Message content not found in verbose mode output" << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, NormalModeBalanceProperty) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: In normal mode, quiet and normal messages should be shown, verbose should not
    bool property_holds = PropertyTest::forall(
        message_generator(),
        [&](const std::string& message) {
            shell_module.set_verbosity(VerbosityLevel::Normal);
            
            // Test quiet level message (should be shown)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Quiet);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "Quiet message not shown in normal mode: " << message << std::endl;
                    return false;
                }
            }
            
            // Test normal level message (should be shown)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Normal);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "Normal message not shown in normal mode: " << message << std::endl;
                    return false;
                }
            }
            
            // Test verbose level message (should not be shown)
            {
                OutputCapture capture;
                shell_module.output_message(message, VerbosityLevel::Verbose);
                std::string output = capture.get_output();
                
                if (!output.empty()) {
                    std::cerr << "Verbose message shown in normal mode: " << message << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, VerbosityOrderingProperty) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: Verbosity levels should follow ordering: Quiet ⊆ Normal ⊆ Verbose
    bool property_holds = PropertyTest::forall(
        message_generator(),
        [&](const std::string& message) {
            std::map<VerbosityLevel, std::set<VerbosityLevel>> shown_messages;
            
            // Collect which message levels are shown for each verbosity setting
            std::vector<VerbosityLevel> all_levels = {
                VerbosityLevel::Quiet, VerbosityLevel::Normal, VerbosityLevel::Verbose
            };
            
            for (VerbosityLevel module_level : all_levels) {
                shell_module.set_verbosity(module_level);
                
                for (VerbosityLevel message_level : all_levels) {
                    OutputCapture capture;
                    shell_module.output_message(message, message_level);
                    std::string output = capture.get_output();
                    
                    if (!output.empty()) {
                        shown_messages[module_level].insert(message_level);
                    }
                }
            }
            
            // Verify ordering: Quiet ⊆ Normal ⊆ Verbose
            auto quiet_shown = shown_messages[VerbosityLevel::Quiet];
            auto normal_shown = shown_messages[VerbosityLevel::Normal];
            auto verbose_shown = shown_messages[VerbosityLevel::Verbose];
            
            // Check Quiet ⊆ Normal
            for (VerbosityLevel level : quiet_shown) {
                if (normal_shown.find(level) == normal_shown.end()) {
                    std::cerr << "Quiet subset property violated: level " 
                              << static_cast<int>(level) << " shown in quiet but not normal" << std::endl;
                    return false;
                }
            }
            
            // Check Normal ⊆ Verbose
            for (VerbosityLevel level : normal_shown) {
                if (verbose_shown.find(level) == verbose_shown.end()) {
                    std::cerr << "Normal subset property violated: level " 
                              << static_cast<int>(level) << " shown in normal but not verbose" << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        50  // Reduced iterations due to complexity
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, SpecialMethodsConsistency) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Property: Special output methods should respect verbosity settings
    bool property_holds = PropertyTest::forall(
        message_generator(),
        [&](const std::string& message) {
            // Test output_verbose method
            {
                shell_module.set_verbosity(VerbosityLevel::Quiet);
                OutputCapture capture;
                shell_module.output_verbose(message);
                std::string output = capture.get_output();
                
                if (!output.empty()) {
                    std::cerr << "output_verbose shown in quiet mode" << std::endl;
                    return false;
                }
            }
            
            {
                shell_module.set_verbosity(VerbosityLevel::Verbose);
                OutputCapture capture;
                shell_module.output_verbose(message);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "output_verbose not shown in verbose mode" << std::endl;
                    return false;
                }
                
                // Should contain both the verbose prefix and the message
                if (output.find("[VERBOSE]") == std::string::npos) {
                    std::cerr << "output_verbose missing verbose prefix" << std::endl;
                    return false;
                }
                
                if (output.find(message) == std::string::npos) {
                    std::cerr << "output_verbose missing message content" << std::endl;
                    return false;
                }
            }
            
            // Test output_quiet method
            {
                shell_module.set_verbosity(VerbosityLevel::Normal);
                OutputCapture capture;
                shell_module.output_quiet(message);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "output_quiet not shown in normal mode" << std::endl;
                    return false;
                }
            }
            
            {
                shell_module.set_verbosity(VerbosityLevel::Quiet);
                OutputCapture capture;
                shell_module.output_quiet(message);
                std::string output = capture.get_output();
                
                if (output.empty()) {
                    std::cerr << "output_quiet not shown in quiet mode" << std::endl;
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(OutputVerbosityControlPropertyTest, VerbosityLevelPersistence) {
    auto error_handler = std::make_shared<ErrorHandler>();
    ShellIntegrationModule shell_module(error_handler);
    
    // Test that verbosity level settings persist correctly
    shell_module.set_verbosity(VerbosityLevel::Quiet);
    EXPECT_EQ(shell_module.get_verbosity(), VerbosityLevel::Quiet);
    
    shell_module.set_verbosity(VerbosityLevel::Normal);
    EXPECT_EQ(shell_module.get_verbosity(), VerbosityLevel::Normal);
    
    shell_module.set_verbosity(VerbosityLevel::Verbose);
    EXPECT_EQ(shell_module.get_verbosity(), VerbosityLevel::Verbose);
}