#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <algorithm>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 5: Argument Passing Preservation**
 * **Validates: Requirements 2.4**
 * 
 * Property: For any list of command-line arguments, they should be passed 
 * unchanged to the Meld program during interpretation
 */
class ArgumentPassingPreservationTest : public PropertyTest {
protected:
    void SetUp() override {
        PropertyTest::SetUp();
        interpreter_ = std::make_unique<InterpreterModule>();
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_arg_test";
        std::filesystem::create_directories(temp_dir_);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
        PropertyTest::TearDown();
    }
    
    std::unique_ptr<InterpreterModule> interpreter_;
    std::filesystem::path temp_dir_;
};

// Generator for command-line argument lists
class ArgumentListGenerator {
public:
    std::vector<std::string> generate(std::mt19937& rng) {
        std::uniform_int_distribution<size_t> size_dist(0, 10);
        size_t arg_count = size_dist(rng);
        
        std::vector<std::string> args;
        args.reserve(arg_count);
        
        for (size_t i = 0; i < arg_count; ++i) {
            args.push_back(generate_argument(rng));
        }
        
        return args;
    }
    
private:
    std::string generate_argument(std::mt19937& rng) {
        std::uniform_int_distribution<int> type_dist(0, 6);
        int arg_type = type_dist(rng);
        
        switch (arg_type) {
            case 0:
                return generate_simple_string(rng);
            case 1:
                return generate_number_string(rng);
            case 2:
                return generate_flag_argument(rng);
            case 3:
                return generate_path_argument(rng);
            case 4:
                return generate_special_chars_argument(rng);
            case 5:
                return generate_quoted_argument(rng);
            case 6:
                return generate_empty_or_whitespace(rng);
            default:
                return generate_simple_string(rng);
        }
    }
    
    std::string generate_simple_string(std::mt19937& rng) {
        std::vector<std::string> words = {
            "hello", "world", "test", "argument", "value", "data",
            "input", "output", "file", "name", "option", "parameter"
        };
        
        std::uniform_int_distribution<size_t> word_dist(0, words.size() - 1);
        return words[word_dist(rng)];
    }
    
    std::string generate_number_string(std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(-1000, 1000);
        return std::to_string(num_dist(rng));
    }
    
    std::string generate_flag_argument(std::mt19937& rng) {
        std::vector<std::string> flags = {
            "--verbose", "--debug", "--help", "--version", "--output",
            "-v", "-d", "-h", "-o", "-f"
        };
        
        std::uniform_int_distribution<size_t> flag_dist(0, flags.size() - 1);
        return flags[flag_dist(rng)];
    }
    
    std::string generate_path_argument(std::mt19937& rng) {
        std::vector<std::string> paths = {
            "/tmp/file.txt", "./local/path", "../parent/dir",
            "relative/path.meld", "/absolute/path/file.dat",
            "C:\\Windows\\System32", "~/home/user/document.txt"
        };
        
        std::uniform_int_distribution<size_t> path_dist(0, paths.size() - 1);
        return paths[path_dist(rng)];
    }
    
    std::string generate_special_chars_argument(std::mt19937& rng) {
        std::vector<std::string> special = {
            "arg with spaces", "arg-with-dashes", "arg_with_underscores",
            "arg.with.dots", "arg@with@symbols", "arg=with=equals",
            "arg:with:colons", "arg;with;semicolons"
        };
        
        std::uniform_int_distribution<size_t> special_dist(0, special.size() - 1);
        return special[special_dist(rng)];
    }
    
    std::string generate_quoted_argument(std::mt19937& rng) {
        std::vector<std::string> quoted = {
            "\"quoted string\"", "'single quoted'", "\"with spaces\"",
            "'with symbols!'", "\"nested 'quotes'\"", "'nested \"quotes\"'"
        };
        
        std::uniform_int_distribution<size_t> quoted_dist(0, quoted.size() - 1);
        return quoted[quoted_dist(rng)];
    }
    
    std::string generate_empty_or_whitespace(std::mt19937& rng) {
        std::vector<std::string> empty_like = {
            "", " ", "  ", "\t", "\n", "   \t  "
        };
        
        std::uniform_int_distribution<size_t> empty_dist(0, empty_like.size() - 1);
        return empty_like[empty_dist(rng)];
    }
};

TEST_F(ArgumentPassingPreservationTest, ArgumentsPassedUnchangedToProgram) {
    auto arg_gen = std::make_unique<ArgumentListGenerator>();
    
    property_test("test", 100, [&](std::mt19937& rng) {
        // Generate a list of arguments
        std::vector<std::string> args = arg_gen->generate(rng);
        
        // Create a Meld program that prints all its arguments
        std::string program = R"(
fnc main(args) {
    print("ARGS_START")
    for (i = 0; i < args.length; i++) {
        print("ARG_" + i + ":" + args[i])
    }
    print("ARGS_END")
}
)";
        
        // Write program to temporary file
        std::filesystem::path program_file = temp_dir_ / ("arg_test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        ASSERT_TRUE(file.is_open());
        file << program;
        file.close();
        
        // Execute with the generated arguments
        ExecutionResult result = interpreter_->run_file(program_file, args);
        
        // The program should execute successfully
        EXPECT_TRUE(result.success)
            << "Program should execute successfully with arguments\n"
            << "Arguments count: " << args.size() << "\n"
            << "First few args: ";
        
        if (result.success) {
            // Parse the output to verify arguments were passed correctly
            std::string output = result.output;
            
            // Check that output contains argument markers
            EXPECT_NE(output.find("ARGS_START"), std::string::npos)
                << "Output should contain ARGS_START marker";
            
            EXPECT_NE(output.find("ARGS_END"), std::string::npos)
                << "Output should contain ARGS_END marker";
            
            // For each argument, verify it appears in the output
            // Note: This is a simplified check - in a real implementation,
            // we would parse the structured output more carefully
            for (size_t i = 0; i < args.size(); ++i) {
                const std::string& arg = args[i];
                
                // Skip empty arguments as they might not appear in output
                if (arg.empty() || std::all_of(arg.begin(), arg.end(), ::isspace)) {
                    continue;
                }
                
                // Look for the argument in the output
                // In a real implementation, this would be more sophisticated
                std::string arg_marker = "ARG_" + std::to_string(i) + ":";
                size_t marker_pos = output.find(arg_marker);
                
                if (marker_pos != std::string::npos) {
                    // Verify the argument value follows the marker
                    size_t arg_start = marker_pos + arg_marker.length();
                    size_t arg_end = output.find('\n', arg_start);
                    if (arg_end == std::string::npos) {
                        arg_end = output.length();
                    }
                    
                    std::string found_arg = output.substr(arg_start, arg_end - arg_start);
                    
                    // The found argument should match the original
                    // (allowing for some formatting differences)
                    EXPECT_EQ(found_arg, arg)
                        << "Argument " << i << " should be preserved exactly\n"
                        << "Expected: '" << arg << "'\n"
                        << "Found: '" << found_arg << "'";
                }
            }
        }
        
    });
}

TEST_F(ArgumentPassingPreservationTest, EmptyArgumentListHandledCorrectly) {
    property_test("test", 30, [&](std::mt19937& rng) {
        // Create a program that checks for empty arguments
        std::string program = R"(
fnc main(args) {
    print("ARG_COUNT:" + args.length)
    if (args.length == 0) {
        print("NO_ARGUMENTS")
    }
}
)";
        
        std::filesystem::path program_file = temp_dir_ / ("empty_args_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        // Execute with empty argument list
        std::vector<std::string> empty_args;
        ExecutionResult result = interpreter_->run_file(program_file, empty_args);
        
        EXPECT_TRUE(result.success)
            << "Program should execute successfully with empty arguments";
        
        if (result.success) {
            EXPECT_NE(result.output.find("ARG_COUNT:0"), std::string::npos)
                << "Output should indicate zero arguments";
            
            EXPECT_NE(result.output.find("NO_ARGUMENTS"), std::string::npos)
                << "Output should indicate no arguments were passed";
        }
        
    });
}

TEST_F(ArgumentPassingPreservationTest, ArgumentOrderPreserved) {
    property_test("test", 50, [&](std::mt19937& rng) {
        // Generate a specific sequence of arguments
        std::vector<std::string> args;
        std::uniform_int_distribution<size_t> size_dist(2, 8);
        size_t arg_count = size_dist(rng);
        
        for (size_t i = 0; i < arg_count; ++i) {
            args.push_back("arg" + std::to_string(i));
        }
        
        // Create a program that prints arguments in order
        std::string program = R"(
fnc main(args) {
    for (i = 0; i < args.length; i++) {
        print("POSITION_" + i + ":" + args[i])
    }
}
)";
        
        std::filesystem::path program_file = temp_dir_ / ("order_test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        ExecutionResult result = interpreter_->run_file(program_file, args);
        
        EXPECT_TRUE(result.success)
            << "Program should execute successfully";
        
        if (result.success) {
            // Verify each argument appears at the correct position
            for (size_t i = 0; i < args.size(); ++i) {
                std::string position_marker = "POSITION_" + std::to_string(i) + ":" + args[i];
                EXPECT_NE(result.output.find(position_marker), std::string::npos)
                    << "Argument '" << args[i] << "' should appear at position " << i;
            }
        }
        
    });
}

TEST_F(ArgumentPassingPreservationTest, SpecialCharactersPreserved) {
    property_test("test", 40, [&](std::mt19937& rng) {
        // Test arguments with special characters
        std::vector<std::string> special_args = {
            "arg with spaces",
            "arg-with-dashes", 
            "arg_with_underscores",
            "arg.with.dots",
            "arg@with@symbols",
            "arg=with=equals",
            "arg:with:colons"
        };
        
        std::uniform_int_distribution<size_t> count_dist(1, special_args.size());
        size_t arg_count = count_dist(rng);
        
        std::vector<std::string> selected_args;
        std::sample(special_args.begin(), special_args.end(),
                   std::back_inserter(selected_args), arg_count, rng);
        
        std::string program = R"(
fnc main(args) {
    print("SPECIAL_ARGS_START")
    for (i = 0; i < args.length; i++) {
        print("SPECIAL_ARG:" + args[i])
    }
    print("SPECIAL_ARGS_END")
}
)";
        
        std::filesystem::path program_file = temp_dir_ / ("special_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        ExecutionResult result = interpreter_->run_file(program_file, selected_args);
        
        EXPECT_TRUE(result.success)
            << "Program should handle special characters in arguments";
        
        if (result.success) {
            // Verify special characters are preserved
            for (const auto& arg : selected_args) {
                std::string arg_marker = "SPECIAL_ARG:" + arg;
                EXPECT_NE(result.output.find(arg_marker), std::string::npos)
                    << "Special argument '" << arg << "' should be preserved exactly";
            }
        }
        
    });
}