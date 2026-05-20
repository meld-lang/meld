#include <gtest/gtest.h>
#include "meld/cli/project_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <set>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 12: Build System Detection**
 * **Validates: Requirements 5.1, 5.2, 5.3**
 * 
 * For any project directory, the CLI should select the build system based on 
 * the presence of specific configuration files in a deterministic order
 */

class BuildSystemDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        project_module_ = std::make_unique<ProjectModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_build_detection_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::shared_ptr<ErrorHandler> error_handler_;
    std::unique_ptr<ProjectModule> project_module_;
    std::filesystem::path test_dir_;
};

// Generator for build file combinations
class BuildFileGenerator {
public:
    struct BuildFileSet {
        std::set<std::string> files;
        BuildSystem expected_system;
    };
    
    static std::vector<BuildFileSet> getAllCombinations() {
        return {
            // Single build system files
            {{"BUILD.bazel"}, BuildSystem::Bazel},
            {{"BUILD"}, BuildSystem::Bazel},
            {{"meld.yaml"}, BuildSystem::Native},
            
            // Multiple files - Bazel should take precedence
            {{"BUILD.bazel", "meld.yaml"}, BuildSystem::Bazel},
            {{"BUILD", "meld.yaml"}, BuildSystem::Bazel},
            {{"BUILD.bazel", "BUILD"}, BuildSystem::Bazel},
            {{"BUILD.bazel", "BUILD", "meld.yaml"}, BuildSystem::Bazel},
            
            // No build files
            {{}, BuildSystem::Unknown},
            
            // Other files that shouldn't affect detection
            {{"README.md", "src/main.cpp"}, BuildSystem::Unknown},
            {{"BUILD.bazel", "README.md", "src/main.cpp"}, BuildSystem::Bazel},
            {{"meld.yaml", "README.md", "src/main.cpp"}, BuildSystem::Native},
        };
    }
    
    static BuildFileSet generate(std::mt19937& gen) {
        auto combinations = getAllCombinations();
        std::uniform_int_distribution<> dist(0, combinations.size() - 1);
        return combinations[dist(gen)];
    }
};

// Generator for additional project files
class ProjectFileGenerator {
public:
    static std::vector<std::string> generate(std::mt19937& gen) {
        std::vector<std::string> possible_files = {
            "README.md",
            "LICENSE",
            ".gitignore",
            "src/main.cpp",
            "src/lib.cpp",
            "include/header.hpp",
            "tests/test.cpp",
            "docs/README.md",
            "scripts/build.sh",
            "config.json",
            "Makefile",  // Should not affect detection
            "CMakeLists.txt",  // Should not affect detection
            "package.json",  // Should not affect detection
        };
        
        std::uniform_int_distribution<> count_dist(0, 5);
        std::uniform_int_distribution<> file_dist(0, possible_files.size() - 1);
        
        int file_count = count_dist(gen);
        std::vector<std::string> selected_files;
        std::set<std::string> used_files;
        
        for (int i = 0; i < file_count; ++i) {
            std::string file = possible_files[file_dist(gen)];
            if (used_files.find(file) == used_files.end()) {
                selected_files.push_back(file);
                used_files.insert(file);
            }
        }
        
        return selected_files;
    }
};

TEST_F(BuildSystemDetectionTest, BuildSystemDetection) {
    PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& gen) {
        // Generate test inputs
        auto build_file_set = BuildFileGenerator::generate(gen);
        auto additional_files = ProjectFileGenerator::generate(gen);
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("test_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create build system files
            for (const auto& file : build_file_set.files) {
                std::filesystem::path file_path = iteration_dir / file;
                std::filesystem::create_directories(file_path.parent_path());
                
                std::ofstream file_stream(file_path);
                if (file == "BUILD.bazel" || file == "BUILD") {
                    file_stream << "# Bazel BUILD file\n";
                    file_stream << "load(\"@rules_cc//cc:defs.bzl\", \"cc_library\")\n";
                    file_stream << "cc_library(name = \"test\")\n";
                } else if (file == "meld.yaml") {
                    file_stream << "name: test\n";
                    file_stream << "version: 1.0.0\n";
                    file_stream << "type: library\n";
                }
                file_stream.close();
                
                EXPECT_TRUE(std::filesystem::exists(file_path))
                    << "Failed to create build file: " << file_path;
            }
            
            // Create additional files
            for (const auto& file : additional_files) {
                std::filesystem::path file_path = iteration_dir / file;
                std::filesystem::create_directories(file_path.parent_path());
                
                std::ofstream file_stream(file_path);
                file_stream << "# Additional file content\n";
                file_stream.close();
            }
            
            // Test build system detection
            BuildSystem detected_system = project_module_->detect_build_system(iteration_dir);
            
            // Property: Detection should match expected system
            EXPECT_EQ(detected_system, build_file_set.expected_system)
                << "Build system detection mismatch. Expected: " << static_cast<int>(build_file_set.expected_system)
                << ", Detected: " << static_cast<int>(detected_system)
                << ", Files: ";
            
            // Property: Detection should be deterministic (run multiple times)
            for (int i = 0; i < 3; ++i) {
                BuildSystem repeated_detection = project_module_->detect_build_system(iteration_dir);
                EXPECT_EQ(repeated_detection, detected_system)
                    << "Build system detection is not deterministic on iteration " << i;
            }
            
            // Property: Detection should be based only on build files, not other files
            // This is implicitly tested by including additional files that shouldn't affect detection
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during build system detection: " << e.what();
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test build system detection precedence rules
TEST_F(BuildSystemDetectionTest, BuildSystemPrecedence) {
    // Test that Bazel takes precedence over Native when both are present
    std::vector<std::pair<std::vector<std::string>, BuildSystem>> precedence_tests = {
        // Bazel files take precedence
        {{"BUILD.bazel", "meld.yaml"}, BuildSystem::Bazel},
        {{"BUILD", "meld.yaml"}, BuildSystem::Bazel},
        {{"BUILD.bazel", "BUILD", "meld.yaml"}, BuildSystem::Bazel},
        
        // BUILD.bazel takes precedence over BUILD
        {{"BUILD.bazel", "BUILD"}, BuildSystem::Bazel},
        
        // Single file detection
        {{"BUILD.bazel"}, BuildSystem::Bazel},
        {{"BUILD"}, BuildSystem::Bazel},
        {{"meld.yaml"}, BuildSystem::Native},
        
        // No build files
        {{"README.md", "src/main.cpp"}, BuildSystem::Unknown},
        {{}, BuildSystem::Unknown},
    };
    
    for (size_t test_idx = 0; test_idx < precedence_tests.size(); ++test_idx) {
        const auto& test_case = precedence_tests[test_idx];
        const auto& files = test_case.first;
        BuildSystem expected = test_case.second;
        
        std::filesystem::path iteration_dir = test_dir_ / ("precedence_" + std::to_string(test_idx));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create the specified files
            for (const auto& file : files) {
                std::filesystem::path file_path = iteration_dir / file;
                std::filesystem::create_directories(file_path.parent_path());
                
                std::ofstream file_stream(file_path);
                if (file == "BUILD.bazel" || file == "BUILD") {
                    file_stream << "# Bazel BUILD file\n";
                } else if (file == "meld.yaml") {
                    file_stream << "name: test\nversion: 1.0.0\n";
                } else {
                    file_stream << "# Other file\n";
                }
                file_stream.close();
            }
            
            BuildSystem detected = project_module_->detect_build_system(iteration_dir);
            EXPECT_EQ(detected, expected)
                << "Precedence test failed for test case " << test_idx
                << ". Expected: " << static_cast<int>(expected)
                << ", Detected: " << static_cast<int>(detected);
            
        } catch (const std::exception& e) {
            FAIL() << "Exception in precedence test " << test_idx << ": " << e.what();
        }
        
        // Clean up
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    }
}

// Test build system detection with nested directories
TEST_F(BuildSystemDetectionTest, NestedDirectoryDetection) {
    // Test that detection only looks at the specified directory, not subdirectories
    std::filesystem::path iteration_dir = test_dir_ / "nested_test";
    std::filesystem::create_directories(iteration_dir);
    
    try {
        // Create build file in subdirectory (should not be detected)
        std::filesystem::path subdir = iteration_dir / "subproject";
        std::filesystem::create_directories(subdir);
        
        std::ofstream sub_build(subdir / "BUILD.bazel");
        sub_build << "# Subdirectory BUILD file\n";
        sub_build.close();
        
        // Detection should return Unknown since no build files in root
        BuildSystem detected = project_module_->detect_build_system(iteration_dir);
        EXPECT_EQ(detected, BuildSystem::Unknown)
            << "Detection should not find build files in subdirectories";
        
        // Now add build file to root directory
        std::ofstream root_build(iteration_dir / "meld.yaml");
        root_build << "name: test\nversion: 1.0.0\n";
        root_build.close();
        
        // Detection should now find the root build file
        detected = project_module_->detect_build_system(iteration_dir);
        EXPECT_EQ(detected, BuildSystem::Native)
            << "Detection should find build files in root directory";
        
    } catch (const std::exception& e) {
        FAIL() << "Exception in nested directory test: " << e.what();
    }
    
    // Clean up
    if (std::filesystem::exists(iteration_dir)) {
        std::filesystem::remove_all(iteration_dir);
    }
}