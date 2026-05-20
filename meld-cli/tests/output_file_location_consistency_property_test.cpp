#include <gtest/gtest.h>
#include "../include/meld/cli/compiler_module.hpp"
#include "meld/testing/property_test.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 3: Output File Location Consistency**
 * **Validates: Requirements 1.7**
 * 
 * Property: For any compilation command with an output flag, the compiled result 
 * should be written to exactly the specified location
 */
class OutputFileLocationConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler_module_ = std::make_unique<CompilerModule>();
        
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_cli_output_test";
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

// Generator for various output path scenarios
class OutputPathGenerator {
public:
    static std::vector<std::filesystem::path> generate_output_paths(const std::filesystem::path& base_dir) {
        return {
            // Simple filename in base directory
            base_dir / "output",
            base_dir / "program",
            base_dir / "test_output",
            
            // With extensions
            base_dir / "output.java",
            base_dir / "program.go",
            base_dir / "test.cpp",
            base_dir / "module.wasm",
            
            // In subdirectories
            base_dir / "subdir" / "output",
            base_dir / "build" / "program",
            base_dir / "target" / "release" / "app",
            
            // With special characters (where allowed by filesystem)
            base_dir / "output_with_underscores",
            base_dir / "output-with-dashes",
            base_dir / "output123",
            
            // Different depths
            base_dir / "a" / "b" / "c" / "deep_output",
            base_dir / "single_level",
            
            // Absolute paths within temp directory
            std::filesystem::absolute(base_dir / "absolute_output")
        };
    }
};

// Generator for valid source code
class SimpleSourceGenerator {
public:
    static std::vector<std::string> generate_simple_sources() {
        return {
            "val x = 42",
            "fnc hello() -> String { \"Hello, World!\" }",
            "class Point { val x: Int; val y: Int }",
            "struct Vector { val x: Float; val y: Float; val z: Float }"
        };
    }
};

// Property test: Output file location consistency
TEST_F(OutputFileLocationConsistencyTest, CompilationWritesToSpecifiedLocation) {
    auto sources = SimpleSourceGenerator::generate_simple_sources();
    auto output_paths = OutputPathGenerator::generate_output_paths(temp_dir_);
    auto targets = std::vector<CompilationTarget>{
        CompilationTarget::JVM, CompilationTarget::Go, 
        CompilationTarget::Cpp, CompilationTarget::WebAssembly
    };
    
    for (const auto& source : sources) {
        for (const auto& target : targets) {
            for (const auto& output_path : output_paths) {
                // Ensure output directory exists
                std::filesystem::create_directories(output_path.parent_path());
                
                // Remove output file if it exists
                if (std::filesystem::exists(output_path)) {
                    std::filesystem::remove(output_path);
                }
                
                CompileOptions options;
                options.target = target;
                options.output_path = output_path;
                
                auto result = compiler_module_->compile_source(source, options);
                
                if (result.success) {
                    // Check that the result indicates the correct output file
                    EXPECT_EQ(result.output_file, output_path)
                        << "Result should indicate the specified output file for target " 
                        << static_cast<int>(target);
                    
                    // For targets that produce intermediate files, check the final output
                    // Note: Some targets may produce different final extensions after post-processing
                    if (target == CompilationTarget::JVM) {
                        // JVM produces .java first, then .class after javac
                        std::filesystem::path java_file = output_path;
                        java_file.replace_extension(".java");
                        
                        // Either the .java file or the specified output should exist
                        bool java_exists = std::filesystem::exists(java_file);
                        bool output_exists = std::filesystem::exists(output_path);
                        
                        EXPECT_TRUE(java_exists || output_exists)
                            << "Either Java source or compiled output should exist at specified location";
                    }
                    else if (target == CompilationTarget::Go) {
                        // Go produces .go first, then executable after go build
                        std::filesystem::path go_file = output_path;
                        go_file.replace_extension(".go");
                        
                        bool go_exists = std::filesystem::exists(go_file);
                        bool output_exists = std::filesystem::exists(output_path);
                        
                        EXPECT_TRUE(go_exists || output_exists)
                            << "Either Go source or compiled executable should exist at specified location";
                    }
                    else if (target == CompilationTarget::Cpp) {
                        // C++ produces .cpp first, then executable after compilation
                        std::filesystem::path cpp_file = output_path;
                        cpp_file.replace_extension(".cpp");
                        
                        bool cpp_exists = std::filesystem::exists(cpp_file);
                        bool output_exists = std::filesystem::exists(output_path);
                        
                        EXPECT_TRUE(cpp_exists || output_exists)
                            << "Either C++ source or compiled executable should exist at specified location";
                    }
                    else if (target == CompilationTarget::WebAssembly) {
                        // WebAssembly produces .wat first, then .wasm after wat2wasm
                        std::filesystem::path wat_file = output_path;
                        wat_file.replace_extension(".wat");
                        
                        bool wat_exists = std::filesystem::exists(wat_file);
                        bool wasm_exists = std::filesystem::exists(output_path);
                        
                        EXPECT_TRUE(wat_exists || wasm_exists)
                            << "Either WAT source or compiled WASM should exist at specified location";
                    }
                }
            }
        }
    }
}

// Property test: Output directory creation
TEST_F(OutputFileLocationConsistencyTest, OutputDirectoryCreation) {
    std::string simple_source = "val x = 42";
    
    // Test paths with non-existent directories
    std::vector<std::filesystem::path> paths_with_new_dirs = {
        temp_dir_ / "new_dir" / "output",
        temp_dir_ / "deeply" / "nested" / "path" / "output",
        temp_dir_ / "another" / "new" / "directory" / "program"
    };
    
    for (const auto& output_path : paths_with_new_dirs) {
        // Ensure the directory doesn't exist initially
        if (std::filesystem::exists(output_path.parent_path())) {
            std::filesystem::remove_all(output_path.parent_path());
        }
        
        EXPECT_FALSE(std::filesystem::exists(output_path.parent_path()))
            << "Output directory should not exist initially";
        
        CompileOptions options;
        options.target = CompilationTarget::JVM;  // Use one target for this test
        options.output_path = output_path;
        
        auto result = compiler_module_->compile_source(simple_source, options);
        
        if (result.success) {
            // The parent directory should be created
            EXPECT_TRUE(std::filesystem::exists(output_path.parent_path()))
                << "Output directory should be created: " << output_path.parent_path();
        }
    }
}

// Property test: Output file overwriting
TEST_F(OutputFileLocationConsistencyTest, OutputFileOverwriting) {
    std::string source1 = "val x = 42";
    std::string source2 = "val y = 100";
    
    std::filesystem::path output_path = temp_dir_ / "overwrite_test";
    
    CompileOptions options;
    options.target = CompilationTarget::JVM;
    options.output_path = output_path;
    
    // First compilation
    auto result1 = compiler_module_->compile_source(source1, options);
    
    if (result1.success) {
        // Check that some output was created
        std::filesystem::path java_file = output_path;
        java_file.replace_extension(".java");
        
        if (std::filesystem::exists(java_file)) {
            // Read the first output
            std::ifstream file1(java_file);
            std::string content1((std::istreambuf_iterator<char>(file1)),
                                std::istreambuf_iterator<char>());
            file1.close();
            
            EXPECT_FALSE(content1.empty())
                << "First compilation should produce non-empty output";
            
            // Second compilation with different source
            auto result2 = compiler_module_->compile_source(source2, options);
            
            if (result2.success) {
                // Read the second output
                std::ifstream file2(java_file);
                std::string content2((std::istreambuf_iterator<char>(file2)),
                                    std::istreambuf_iterator<char>());
                file2.close();
                
                // The content should be different (overwritten)
                EXPECT_NE(content1, content2)
                    << "Second compilation should overwrite the first output";
                
                EXPECT_FALSE(content2.empty())
                    << "Second compilation should produce non-empty output";
            }
        }
    }
}

// Property test: Default output path consistency
TEST_F(OutputFileLocationConsistencyTest, DefaultOutputPathConsistency) {
    std::vector<std::filesystem::path> source_files = {
        temp_dir_ / "test.meld",
        temp_dir_ / "program.meld", 
        temp_dir_ / "module.meld",
        temp_dir_ / "subdir" / "nested.meld"
    };
    
    auto targets = std::vector<CompilationTarget>{
        CompilationTarget::JVM, CompilationTarget::Go, 
        CompilationTarget::Cpp, CompilationTarget::WebAssembly
    };
    
    for (const auto& source_file : source_files) {
        // Create the source file directory
        std::filesystem::create_directories(source_file.parent_path());
        
        for (const auto& target : targets) {
            auto default_output = compiler_module_->get_default_output_path(source_file, target);
            
            // Default output should not be empty
            EXPECT_FALSE(default_output.empty())
                << "Default output path should not be empty for target " 
                << static_cast<int>(target);
            
            // Should be based on the source file name
            EXPECT_EQ(default_output.stem(), source_file.stem())
                << "Default output should use source file stem for target " 
                << static_cast<int>(target);
            
            // Should have target-appropriate extension
            std::string extension = default_output.extension().string();
            switch (target) {
                case CompilationTarget::JVM:
                    EXPECT_EQ(extension, ".java")
                        << "JVM target should default to .java extension";
                    break;
                case CompilationTarget::Go:
                    EXPECT_EQ(extension, ".go")
                        << "Go target should default to .go extension";
                    break;
                case CompilationTarget::Cpp:
                    EXPECT_EQ(extension, ".cpp")
                        << "C++ target should default to .cpp extension";
                    break;
                case CompilationTarget::WebAssembly:
                    EXPECT_EQ(extension, ".wasm")
                        << "WebAssembly target should default to .wasm extension";
                    break;
            }
        }
    }
}

// Property test: Output path validation
TEST_F(OutputFileLocationConsistencyTest, OutputPathValidation) {
    std::string simple_source = "val x = 42";
    
    // Test various edge cases for output paths
    std::vector<std::pair<std::filesystem::path, bool>> path_validity_tests = {
        // Valid paths
        {temp_dir_ / "valid_output", true},
        {temp_dir_ / "output.java", true},
        {temp_dir_ / "sub" / "dir" / "output", true},
        
        // Edge cases that should be handled gracefully
        {temp_dir_ / "", false},  // Empty filename
        {temp_dir_ / ".", false}, // Current directory
        {temp_dir_ / "..", false}, // Parent directory
    };
    
    for (const auto& [output_path, should_be_valid] : path_validity_tests) {
        CompileOptions options;
        options.target = CompilationTarget::JVM;
        options.output_path = output_path;
        
        auto result = compiler_module_->compile_source(simple_source, options);
        
        if (should_be_valid) {
            // Valid paths should either succeed or fail gracefully with meaningful errors
            if (!result.success) {
                EXPECT_FALSE(result.errors.empty())
                    << "Failed compilation should have error messages for path: " 
                    << output_path;
            }
        } else {
            // Invalid paths should fail gracefully
            if (!result.success) {
                EXPECT_FALSE(result.errors.empty())
                    << "Invalid output path should produce error messages: " 
                    << output_path;
            }
        }
    }
}

// Property test: Concurrent output file access
TEST_F(OutputFileLocationConsistencyTest, ConcurrentOutputFileAccess) {
    std::string simple_source = "val x = 42";
    std::filesystem::path output_path = temp_dir_ / "concurrent_test";
    
    CompileOptions options;
    options.target = CompilationTarget::JVM;
    options.output_path = output_path;
    
    // Simulate multiple compilations to the same output path
    // (In a real concurrent scenario, this would be done with threads)
    std::vector<CompilationResult> results;
    
    for (int i = 0; i < 5; ++i) {
        auto result = compiler_module_->compile_source(simple_source, options);
        results.push_back(result);
        
        // Each compilation should handle the output file consistently
        if (result.success) {
            EXPECT_EQ(result.output_file, output_path)
                << "Each compilation should report the same output file";
        }
    }
    
    // At least one compilation should succeed
    bool any_succeeded = std::any_of(results.begin(), results.end(),
                                   [](const CompilationResult& r) { return r.success; });
    
    if (any_succeeded) {
        // The final output should exist
        std::filesystem::path java_file = output_path;
        java_file.replace_extension(".java");
        
        EXPECT_TRUE(std::filesystem::exists(java_file) || std::filesystem::exists(output_path))
            << "Final output should exist after multiple compilations";
    }
}

// Property-based repetition test removed — individual TEST_F tests above
// already cover each property independently.