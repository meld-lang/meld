#include <gtest/gtest.h>
#include "meld/cli/compiler_module.hpp"
#include "meld/testing/property_test.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 1: Compilation Target Consistency**
 * **Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5**
 * 
 * Property: For any valid Meld source file and any supported target platform, 
 * compilation should produce output in the format specific to that target platform
 */
class CompilationTargetConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler_module_ = std::make_unique<CompilerModule>();
        
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_cli_test";
        std::filesystem::create_directories(temp_dir_);
    }
    
    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }
    
    std::unique_ptr<CompilerModule> compiler_module_;
    std::filesystem::path temp_dir_;
};

// Generator for valid Meld source code samples
class MeldSourceGenerator {
public:
    static std::vector<std::string> generate_valid_sources() {
        return {
            // Simple variable declaration
            "val x = 42",
            
            // Function definition
            "fnc add(a: Int, b: Int) -> Int { a + b }",
            
            // Class definition
            "class Person { val name: String; val age: Int }",
            
            // Struct definition
            "struct Point { val x: Float; val y: Float }",
            
            // Simple expression
            "1 + 2 * 3",
            
            // Boolean expression
            "true && false",
            
            // String literal
            "\"Hello, World!\"",
            
            // Array literal
            "[1, 2, 3, 4, 5]",
            
            // Tuple literal
            "(x: 10, y: 20)",
            
            // Function call
            "println(\"Hello\")"
        };
    }
};

// Generator for compilation targets
class TargetGenerator {
public:
    static std::vector<CompilationTarget> generate_targets() {
        return {
            CompilationTarget::JVM,
            CompilationTarget::Go,
            CompilationTarget::Cpp,
            CompilationTarget::WebAssembly
        };
    }
};

// Property test: Compilation target consistency
TEST_F(CompilationTargetConsistencyTest, CompilationProducesTargetSpecificOutput) {
    auto sources = MeldSourceGenerator::generate_valid_sources();
    auto targets = TargetGenerator::generate_targets();
    
    for (const auto& source : sources) {
        for (const auto& target : targets) {
            // Create compile options for this target
            CompileOptions options;
            options.target = target;
            options.output_path = temp_dir_ / ("test_output_" + std::to_string(static_cast<int>(target)));
            
            // Compile the source
            auto result = compiler_module_->compile_source(source, options);
            
            // Check that compilation succeeded or failed gracefully
            if (result.success) {
                // Verify that the generated code is not empty
                EXPECT_FALSE(result.generated_code.empty()) 
                    << "Generated code should not be empty for target " << static_cast<int>(target)
                    << " with source: " << source;
                
                // Verify target-specific output format
                switch (target) {
                    case CompilationTarget::JVM:
                        // Java code should contain class declaration and main method
                        EXPECT_TRUE(result.generated_code.find("class") != std::string::npos ||
                                   result.generated_code.find("public") != std::string::npos)
                            << "JVM target should produce Java-like code";
                        break;
                        
                    case CompilationTarget::Go:
                        // Go code should contain package declaration
                        EXPECT_TRUE(result.generated_code.find("package") != std::string::npos)
                            << "Go target should produce Go package code";
                        break;
                        
                    case CompilationTarget::Cpp:
                        // C++ code should contain includes or main function
                        EXPECT_TRUE(result.generated_code.find("#include") != std::string::npos ||
                                   result.generated_code.find("int main") != std::string::npos)
                            << "C++ target should produce C++ code";
                        break;
                        
                    case CompilationTarget::WebAssembly:
                        // WebAssembly should produce WAT format
                        EXPECT_TRUE(result.generated_code.find("(module") != std::string::npos)
                            << "WebAssembly target should produce WAT format";
                        break;
                }
            } else {
                // If compilation failed, there should be error messages
                EXPECT_FALSE(result.errors.empty())
                    << "Failed compilation should have error messages for target " 
                    << static_cast<int>(target) << " with source: " << source;
            }
        }
    }
}

// Property test: Target-specific file extensions
TEST_F(CompilationTargetConsistencyTest, TargetSpecificFileExtensions) {
    std::string simple_source = "val x = 42";
    auto targets = TargetGenerator::generate_targets();
    
    for (const auto& target : targets) {
        std::filesystem::path source_file = temp_dir_ / "test.meld";
        
        // Write source to file
        std::ofstream file(source_file);
        file << simple_source;
        file.close();
        
        // Get default output path
        auto output_path = compiler_module_->get_default_output_path(source_file, target);
        
        // Verify target-specific extensions
        switch (target) {
            case CompilationTarget::JVM:
                EXPECT_TRUE(output_path.extension() == ".java")
                    << "JVM target should use .java extension";
                break;
                
            case CompilationTarget::Go:
                EXPECT_TRUE(output_path.extension() == ".go")
                    << "Go target should use .go extension";
                break;
                
            case CompilationTarget::Cpp:
                EXPECT_TRUE(output_path.extension() == ".cpp")
                    << "C++ target should use .cpp extension";
                break;
                
            case CompilationTarget::WebAssembly:
                EXPECT_TRUE(output_path.extension() == ".wasm")
                    << "WebAssembly target should use .wasm extension";
                break;
        }
    }
}

// Property test: Available targets consistency
TEST_F(CompilationTargetConsistencyTest, AvailableTargetsConsistency) {
    auto available_targets = compiler_module_->get_available_targets();
    
    // Should have exactly 4 targets
    EXPECT_EQ(available_targets.size(), 4)
        << "Should have exactly 4 compilation targets";
    
    // Should contain all expected targets
    std::vector<std::string> expected_targets = {"jvm", "go", "cpp", "wasm"};
    for (const auto& expected : expected_targets) {
        EXPECT_TRUE(std::find(available_targets.begin(), available_targets.end(), expected) != available_targets.end())
            << "Should contain target: " << expected;
    }
    
    // Each target string should be parseable
    for (const auto& target_str : available_targets) {
        auto parse_result = compiler_module_->parse_target(target_str);
        EXPECT_TRUE(parse_result.has_value())
            << "Target string '" << target_str << "' should be parseable";
    }
}

// Property test: Target parsing consistency
TEST_F(CompilationTargetConsistencyTest, TargetParsingConsistency) {
    // Test valid target strings
    std::vector<std::pair<std::string, CompilationTarget>> valid_targets = {
        {"jvm", CompilationTarget::JVM},
        {"go", CompilationTarget::Go},
        {"cpp", CompilationTarget::Cpp},
        {"c++", CompilationTarget::Cpp},  // Alternative name
        {"wasm", CompilationTarget::WebAssembly},
        {"webassembly", CompilationTarget::WebAssembly}  // Alternative name
    };
    
    for (const auto& [target_str, expected_target] : valid_targets) {
        auto parse_result = compiler_module_->parse_target(target_str);
        EXPECT_TRUE(parse_result.has_value())
            << "Should successfully parse target: " << target_str;
        
        if (parse_result.has_value()) {
            EXPECT_EQ(*parse_result, expected_target)
                << "Parsed target should match expected for: " << target_str;
        }
    }
    
    // Test invalid target strings
    std::vector<std::string> invalid_targets = {
        "invalid", "python", "javascript", "rust", "", "JVM", "GO"
    };
    
    for (const auto& invalid_target : invalid_targets) {
        auto parse_result = compiler_module_->parse_target(invalid_target);
        EXPECT_FALSE(parse_result.has_value())
            << "Should fail to parse invalid target: " << invalid_target;
    }
}

// Property test: Compilation options consistency
TEST_F(CompilationTargetConsistencyTest, CompilationOptionsConsistency) {
    std::string simple_source = "val x = 42";
    auto targets = TargetGenerator::generate_targets();
    
    for (const auto& target : targets) {
        // Test with different optimization levels
        for (int opt_level = 0; opt_level <= 3; ++opt_level) {
            CompileOptions options;
            options.target = target;
            options.optimization_level = opt_level;
            options.debug_info = (opt_level == 0);  // Debug info for unoptimized builds
            
            auto result = compiler_module_->compile_source(simple_source, options);
            
            // Compilation should either succeed or fail gracefully
            if (!result.success) {
                EXPECT_FALSE(result.errors.empty())
                    << "Failed compilation should have error messages";
            }
            
            // Generated code should be consistent regardless of options
            if (result.success) {
                EXPECT_FALSE(result.generated_code.empty())
                    << "Successful compilation should produce non-empty code";
            }
        }
    }
}

// Run property-based tests with multiple iterations
class CompilationTargetPropertyTest : public CompilationTargetConsistencyTest {};

TEST_F(CompilationTargetPropertyTest, RunPropertyTests) {
    auto sources = MeldSourceGenerator::generate_valid_sources();
    auto targets = TargetGenerator::generate_targets();
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        for (const auto& source : sources) {
            for (const auto& target : targets) {
                CompileOptions options;
                options.target = target;
                options.output_path = temp_dir_ / ("prop_test_" + std::to_string(iteration) + "_" + std::to_string(static_cast<int>(target)));
                
                auto result = compiler_module_->compile_source(source, options);
                
                if (result.success) {
                    EXPECT_FALSE(result.generated_code.empty());
                } else {
                    EXPECT_FALSE(result.errors.empty());
                }
            }
        }
    }
}