#include <gtest/gtest.h>
#include "meld/cli/project_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <regex>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 11: Build File Generation**
 * **Validates: Requirements 4.3, 4.4**
 * 
 * For any new project, the generated build files should be syntactically valid 
 * and contain appropriate project metadata
 */

class BuildFileGenerationTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        project_module_ = std::make_unique<ProjectModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_build_test";
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

// Generator for project configurations
class ProjectConfigGenerator {
public:
    static ProjectConfig generate(std::mt19937& gen) {
        ProjectConfig config;
        
        // Generate project name
        std::uniform_int_distribution<> length_dist(3, 15);
        std::uniform_int_distribution<> char_dist(0, 35); // a-z, 0-9
        
        int length = length_dist(gen);
        config.name.reserve(length);
        
        // First character must be a letter
        std::uniform_int_distribution<> first_char_dist(0, 25);
        config.name += static_cast<char>('a' + first_char_dist(gen));
        
        // Remaining characters
        for (int i = 1; i < length; ++i) {
            int char_choice = char_dist(gen);
            if (char_choice < 26) {
                config.name += static_cast<char>('a' + char_choice);
            } else {
                config.name += static_cast<char>('0' + (char_choice - 26));
            }
        }
        
        // Generate version
        std::uniform_int_distribution<> version_dist(0, 9);
        config.version = std::to_string(version_dist(gen)) + "." + 
                        std::to_string(version_dist(gen)) + "." + 
                        std::to_string(version_dist(gen));
        
        // Generate project type
        std::uniform_int_distribution<> type_dist(0, 2);
        switch (type_dist(gen)) {
            case 0: config.type = ProjectType::Library; break;
            case 1: config.type = ProjectType::Executable; break;
            case 2: config.type = ProjectType::Mixed; break;
        }
        
        // Generate build system
        std::uniform_int_distribution<> build_dist(0, 1);
        config.build_system = (build_dist(gen) == 0) ? BuildSystem::Bazel : BuildSystem::Native;
        
        // Generate dependencies
        std::uniform_int_distribution<> dep_count_dist(0, 3);
        int dep_count = dep_count_dist(gen);
        for (int i = 0; i < dep_count; ++i) {
            config.dependencies.push_back("dep" + std::to_string(i));
        }
        
        return config;
    }
};

// Utility functions for validation
bool is_valid_bazel_syntax(const std::string& content) {
    // Basic syntax checks for Bazel BUILD files
    
    // Check for balanced parentheses
    int paren_count = 0;
    for (char c : content) {
        if (c == '(') paren_count++;
        else if (c == ')') paren_count--;
        if (paren_count < 0) return false;
    }
    if (paren_count != 0) return false;
    
    // Check for required load statement
    if (content.find("load(") == std::string::npos) {
        return false;
    }
    
    // Check for at least one rule definition
    std::regex rule_regex(R"((cc_library|cc_binary|cc_test)\s*\()");
    if (!std::regex_search(content, rule_regex)) {
        return false;
    }
    
    return true;
}

bool is_valid_yaml_syntax(const std::string& content) {
    // Basic YAML syntax checks
    
    // Check for required fields
    if (content.find("name:") == std::string::npos) return false;
    if (content.find("version:") == std::string::npos) return false;
    if (content.find("type:") == std::string::npos) return false;
    
    // Check for proper indentation (no tabs)
    if (content.find('\t') != std::string::npos) return false;
    
    return true;
}

TEST_F(BuildFileGenerationTest, BuildFileGeneration) {
    PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& gen) {
        // Generate test configuration
        ProjectConfig config = ProjectConfigGenerator::generate(gen);
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("test_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Generate BUILD.bazel file
            bool bazel_success = project_module_->generate_bazel_build_file(config, iteration_dir);
            EXPECT_TRUE(bazel_success) << "Failed to generate BUILD.bazel for project: " << config.name;
            
            if (bazel_success) {
                std::filesystem::path bazel_file = iteration_dir / "BUILD.bazel";
                
                // Property: BUILD.bazel file should exist
                EXPECT_TRUE(std::filesystem::exists(bazel_file))
                    << "BUILD.bazel file does not exist";
                
                // Property: BUILD.bazel should be readable and non-empty
                std::ifstream bazel_stream(bazel_file);
                std::string bazel_content((std::istreambuf_iterator<char>(bazel_stream)),
                                         std::istreambuf_iterator<char>());
                EXPECT_FALSE(bazel_content.empty())
                    << "BUILD.bazel file is empty";
                
                // Property: BUILD.bazel should have valid syntax
                EXPECT_TRUE(is_valid_bazel_syntax(bazel_content))
                    << "BUILD.bazel has invalid syntax";
                
                // Property: BUILD.bazel should contain project name
                EXPECT_TRUE(bazel_content.find(config.name) != std::string::npos)
                    << "BUILD.bazel does not contain project name: " << config.name;
                
                // Property: BUILD.bazel should contain appropriate rules for project type
                if (config.type == ProjectType::Library || config.type == ProjectType::Mixed) {
                    EXPECT_TRUE(bazel_content.find("cc_library") != std::string::npos)
                        << "BUILD.bazel missing cc_library rule for library project";
                }
                
                if (config.type == ProjectType::Executable || config.type == ProjectType::Mixed) {
                    EXPECT_TRUE(bazel_content.find("cc_binary") != std::string::npos)
                        << "BUILD.bazel missing cc_binary rule for executable project";
                }
                
                // Property: BUILD.bazel should always contain test rule
                EXPECT_TRUE(bazel_content.find("cc_test") != std::string::npos)
                    << "BUILD.bazel missing cc_test rule";
            }
            
            // Generate meld.yaml file
            bool yaml_success = project_module_->generate_meld_yaml(config, iteration_dir);
            EXPECT_TRUE(yaml_success) << "Failed to generate meld.yaml for project: " << config.name;
            
            if (yaml_success) {
                std::filesystem::path yaml_file = iteration_dir / "meld.yaml";
                
                // Property: meld.yaml file should exist
                EXPECT_TRUE(std::filesystem::exists(yaml_file))
                    << "meld.yaml file does not exist";
                
                // Property: meld.yaml should be readable and non-empty
                std::ifstream yaml_stream(yaml_file);
                std::string yaml_content((std::istreambuf_iterator<char>(yaml_stream)),
                                        std::istreambuf_iterator<char>());
                EXPECT_FALSE(yaml_content.empty())
                    << "meld.yaml file is empty";
                
                // Property: meld.yaml should have valid syntax
                EXPECT_TRUE(is_valid_yaml_syntax(yaml_content))
                    << "meld.yaml has invalid syntax";
                
                // Property: meld.yaml should contain project metadata
                EXPECT_TRUE(yaml_content.find("name: " + config.name) != std::string::npos)
                    << "meld.yaml does not contain correct project name";
                
                EXPECT_TRUE(yaml_content.find("version: " + config.version) != std::string::npos)
                    << "meld.yaml does not contain correct version";
                
                // Property: meld.yaml should contain correct project type
                std::string expected_type;
                switch (config.type) {
                    case ProjectType::Library: expected_type = "library"; break;
                    case ProjectType::Executable: expected_type = "executable"; break;
                    case ProjectType::Mixed: expected_type = "mixed"; break;
                }
                EXPECT_TRUE(yaml_content.find("type: " + expected_type) != std::string::npos)
                    << "meld.yaml does not contain correct project type";
                
                // Property: meld.yaml should contain build system information
                std::string expected_build_system;
                switch (config.build_system) {
                    case BuildSystem::Bazel: expected_build_system = "bazel"; break;
                    case BuildSystem::Native: expected_build_system = "native"; break;
                    case BuildSystem::Unknown: expected_build_system = "unknown"; break;
                }
                EXPECT_TRUE(yaml_content.find("build_system: " + expected_build_system) != std::string::npos)
                    << "meld.yaml does not contain correct build system";
                
                // Property: meld.yaml should contain dependencies if any
                if (!config.dependencies.empty()) {
                    EXPECT_TRUE(yaml_content.find("dependencies:") != std::string::npos)
                        << "meld.yaml missing dependencies section";
                    
                    for (const auto& dep : config.dependencies) {
                        EXPECT_TRUE(yaml_content.find("- " + dep) != std::string::npos)
                            << "meld.yaml missing dependency: " << dep;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during build file generation: " << e.what()
                   << " (project: " << config.name << ")";
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test build file generation with specific project types
TEST_F(BuildFileGenerationTest, ProjectTypeSpecificGeneration) {
    std::vector<ProjectType> project_types = {
        ProjectType::Library,
        ProjectType::Executable,
        ProjectType::Mixed
    };
    
    for (auto project_type : project_types) {
        ProjectConfig config;
        config.name = "test_project";
        config.version = "1.0.0";
        config.type = project_type;
        config.build_system = BuildSystem::Bazel;
        
        std::filesystem::path iteration_dir = test_dir_ / ("type_test_" + std::to_string(static_cast<int>(project_type)));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            bool success = project_module_->generate_bazel_build_file(config, iteration_dir);
            EXPECT_TRUE(success) << "Failed to generate BUILD.bazel for project type: " << static_cast<int>(project_type);
            
            if (success) {
                std::ifstream file(iteration_dir / "BUILD.bazel");
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                // Verify type-specific content
                switch (project_type) {
                    case ProjectType::Library:
                        EXPECT_TRUE(content.find("cc_library") != std::string::npos);
                        EXPECT_TRUE(content.find("cc_binary") == std::string::npos);
                        break;
                    case ProjectType::Executable:
                        EXPECT_TRUE(content.find("cc_binary") != std::string::npos);
                        EXPECT_TRUE(content.find("cc_library") == std::string::npos);
                        break;
                    case ProjectType::Mixed:
                        EXPECT_TRUE(content.find("cc_library") != std::string::npos);
                        EXPECT_TRUE(content.find("cc_binary") != std::string::npos);
                        break;
                }
                
                // All types should have tests
                EXPECT_TRUE(content.find("cc_test") != std::string::npos);
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception with project type " << static_cast<int>(project_type) << ": " << e.what();
        }
        
        // Clean up
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    }
}