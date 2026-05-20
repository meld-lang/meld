#include <gtest/gtest.h>
#include "meld/cli/project_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <fstream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 10: Project Scaffolding Consistency**
 * **Validates: Requirements 4.1, 4.2**
 * 
 * For any valid project name and template, project creation should generate 
 * a directory structure that matches the template specification
 */

class ProjectScaffoldingConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        project_module_ = std::make_unique<ProjectModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_test_projects";
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

// Generator for valid project names
class ProjectNameGenerator {
public:
    static std::string generate(std::mt19937& gen) {
        std::uniform_int_distribution<> length_dist(3, 20);
        std::uniform_int_distribution<> char_dist(0, 61); // a-z, A-Z, 0-9
        
        int length = length_dist(gen);
        std::string name;
        name.reserve(length);
        
        // First character must be a letter
        std::uniform_int_distribution<> first_char_dist(0, 51); // a-z, A-Z
        int first_char = first_char_dist(gen);
        if (first_char < 26) {
            name += static_cast<char>('a' + first_char);
        } else {
            name += static_cast<char>('A' + (first_char - 26));
        }
        
        // Remaining characters can be letters, numbers, or underscores
        for (int i = 1; i < length; ++i) {
            int char_choice = char_dist(gen);
            if (char_choice < 26) {
                name += static_cast<char>('a' + char_choice);
            } else if (char_choice < 52) {
                name += static_cast<char>('A' + (char_choice - 26));
            } else if (char_choice < 62) {
                name += static_cast<char>('0' + (char_choice - 52));
            } else {
                name += '_';
            }
        }
        
        return name;
    }
};

// Generator for project templates
class ProjectTemplateGenerator {
public:
    static std::vector<std::string> getTemplateNames() {
        return {"lib", "bin"};
    }
    
    static std::string generate(std::mt19937& gen) {
        auto templates = getTemplateNames();
        std::uniform_int_distribution<> dist(0, templates.size() - 1);
        return templates[dist(gen)];
    }
};

TEST_F(ProjectScaffoldingConsistencyTest, ProjectScaffoldingConsistency) {
    PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& gen) {
        // Generate test inputs
        std::string project_name = ProjectNameGenerator::generate(gen);
        std::string template_name = ProjectTemplateGenerator::generate(gen);
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("test_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Get the template
            ProjectTemplate tmpl = project_module_->get_template(template_name);
            
            // Create the project
            bool creation_success = project_module_->create_project(project_name, tmpl, iteration_dir);
            
            // Property: Project creation should succeed for valid inputs
            EXPECT_TRUE(creation_success) << "Project creation failed for name: " << project_name 
                                         << ", template: " << template_name;
            
            if (!creation_success) {
                return; // Skip further checks if creation failed
            }
            
            std::filesystem::path project_dir = iteration_dir / project_name;
            
            // Property: Project directory should exist
            EXPECT_TRUE(std::filesystem::exists(project_dir)) 
                << "Project directory does not exist: " << project_dir;
            
            // Property: All template directories should be created
            for (const auto& dir : tmpl.directories) {
                std::filesystem::path expected_dir = project_dir / dir;
                EXPECT_TRUE(std::filesystem::exists(expected_dir) && std::filesystem::is_directory(expected_dir))
                    << "Template directory missing: " << expected_dir;
            }
            
            // Property: Essential build files should exist
            EXPECT_TRUE(std::filesystem::exists(project_dir / "BUILD.bazel"))
                << "BUILD.bazel file missing";
            EXPECT_TRUE(std::filesystem::exists(project_dir / "meld.yaml"))
                << "meld.yaml file missing";
            EXPECT_TRUE(std::filesystem::exists(project_dir / "README.md"))
                << "README.md file missing";
            
            // Property: Template-specific files should exist
            if (tmpl.type == ProjectType::Library || tmpl.type == ProjectType::Mixed) {
                EXPECT_TRUE(std::filesystem::exists(project_dir / "include"))
                    << "Include directory missing for library project";
                
                // Check for header file
                std::filesystem::path header_dir = project_dir / "include" / project_name;
                EXPECT_TRUE(std::filesystem::exists(header_dir))
                    << "Project-specific include directory missing";
                
                std::filesystem::path header_file = header_dir / (project_name + ".hpp");
                EXPECT_TRUE(std::filesystem::exists(header_file))
                    << "Header file missing: " << header_file;
            }
            
            if (tmpl.type == ProjectType::Executable || tmpl.type == ProjectType::Mixed) {
                std::filesystem::path main_file = project_dir / "src" / "main.cpp";
                EXPECT_TRUE(std::filesystem::exists(main_file))
                    << "Main source file missing: " << main_file;
            }
            
            // Property: Test files should exist
            std::filesystem::path test_file = project_dir / "tests" / (project_name + "_test.cpp");
            EXPECT_TRUE(std::filesystem::exists(test_file))
                << "Test file missing: " << test_file;
            
            // Property: Generated files should contain project name
            std::ifstream build_file(project_dir / "BUILD.bazel");
            std::string build_content((std::istreambuf_iterator<char>(build_file)),
                                     std::istreambuf_iterator<char>());
            EXPECT_TRUE(build_content.find(project_name) != std::string::npos)
                << "BUILD.bazel does not contain project name";
            
            std::ifstream yaml_file(project_dir / "meld.yaml");
            std::string yaml_content((std::istreambuf_iterator<char>(yaml_file)),
                                    std::istreambuf_iterator<char>());
            EXPECT_TRUE(yaml_content.find(project_name) != std::string::npos)
                << "meld.yaml does not contain project name";
            
            std::ifstream readme_file(project_dir / "README.md");
            std::string readme_content((std::istreambuf_iterator<char>(readme_file)),
                                      std::istreambuf_iterator<char>());
            EXPECT_TRUE(readme_content.find(project_name) != std::string::npos)
                << "README.md does not contain project name";
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during project creation: " << e.what()
                   << " (project: " << project_name << ", template: " << template_name << ")";
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test with edge cases for project names
TEST_F(ProjectScaffoldingConsistencyTest, ProjectScaffoldingEdgeCases) {
    std::vector<std::string> edge_case_names = {
        "a",           // minimum length
        "A",           // single uppercase
        "a1",          // with number
        "my_project",  // with underscore
        "MyProject",   // mixed case
        "project123",  // ending with numbers
        std::string(20, 'x')  // maximum reasonable length
    };
    
    for (const auto& project_name : edge_case_names) {
        for (const auto& template_name : ProjectTemplateGenerator::getTemplateNames()) {
            std::filesystem::path iteration_dir = test_dir_ / ("edge_" + project_name + "_" + template_name);
            std::filesystem::create_directories(iteration_dir);
            
            try {
                ProjectTemplate tmpl = project_module_->get_template(template_name);
                bool success = project_module_->create_project(project_name, tmpl, iteration_dir);
                
                EXPECT_TRUE(success) << "Failed to create project with edge case name: " << project_name;
                
                if (success) {
                    std::filesystem::path project_dir = iteration_dir / project_name;
                    EXPECT_TRUE(std::filesystem::exists(project_dir))
                        << "Project directory missing for edge case: " << project_name;
                }
                
            } catch (const std::exception& e) {
                FAIL() << "Exception with edge case name '" << project_name << "': " << e.what();
            }
            
            // Clean up
            if (std::filesystem::exists(iteration_dir)) {
                std::filesystem::remove_all(iteration_dir);
            }
        }
    }
}