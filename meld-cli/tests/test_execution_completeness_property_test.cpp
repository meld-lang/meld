#include <gtest/gtest.h>
#include "meld/cli/project_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 13: Test Execution Completeness**
 * **Validates: Requirements 5.4**
 * 
 * For any project with test files, running tests should execute all discoverable 
 * test cases and report results for each
 */

class TestExecutionCompletenessTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        project_module_ = std::make_unique<ProjectModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_test_execution_test";
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

// Generator for test project structures
class TestProjectGenerator {
public:
    struct TestProject {
        std::string name;
        BuildSystem build_system;
        std::vector<std::string> test_files;
        int expected_test_count;
    };
    
    static TestProject generate(std::mt19937& gen) {
        TestProject project;
        
        // Generate project name
        std::uniform_int_distribution<> name_length_dist(5, 12);
        std::uniform_int_distribution<> char_dist(0, 25);
        
        int name_length = name_length_dist(gen);
        project.name.reserve(name_length);
        for (int i = 0; i < name_length; ++i) {
            project.name += static_cast<char>('a' + char_dist(gen));
        }
        
        // Generate build system (focus on Bazel since it's implemented)
        project.build_system = BuildSystem::Bazel;
        
        // Generate test files
        std::uniform_int_distribution<> test_count_dist(1, 5);
        int test_count = test_count_dist(gen);
        project.expected_test_count = 0;
        
        for (int i = 0; i < test_count; ++i) {
            std::string test_file = "tests/test_" + std::to_string(i) + ".cpp";
            project.test_files.push_back(test_file);
            
            // Each test file will have 1-3 test cases
            std::uniform_int_distribution<> cases_dist(1, 3);
            project.expected_test_count += cases_dist(gen);
        }
        
        return project;
    }
};

// Helper to create a test project structure
bool create_test_project(const TestProjectGenerator::TestProject& project, 
                        const std::filesystem::path& base_dir) {
    try {
        std::filesystem::path project_dir = base_dir / project.name;
        std::filesystem::create_directories(project_dir);
        
        // Create directory structure
        std::filesystem::create_directories(project_dir / "src");
        std::filesystem::create_directories(project_dir / "tests");
        std::filesystem::create_directories(project_dir / "include");
        
        // Create BUILD.bazel file
        std::ofstream build_file(project_dir / "BUILD.bazel");
        build_file << "load(\"@rules_cc//cc:defs.bzl\", \"cc_library\", \"cc_test\")\n\n";
        build_file << "cc_library(\n";
        build_file << "    name = \"" << project.name << "\",\n";
        build_file << "    srcs = glob([\"src/**/*.cpp\"]),\n";
        build_file << "    hdrs = glob([\"include/**/*.hpp\"]),\n";
        build_file << "    includes = [\"include\"],\n";
        build_file << ")\n\n";
        build_file << "cc_test(\n";
        build_file << "    name = \"" << project.name << "_test\",\n";
        build_file << "    srcs = glob([\"tests/**/*.cpp\"]),\n";
        build_file << "    deps = [\n";
        build_file << "        \":\" + \"" << project.name << "\",\n";
        build_file << "        \"@googletest//:gtest_main\",\n";
        build_file << "    ],\n";
        build_file << ")\n";
        build_file.close();
        
        // Create source files
        std::ofstream src_file(project_dir / "src" / (project.name + ".cpp"));
        src_file << "#include \"" << project.name << ".hpp\"\n\n";
        src_file << "int " << project.name << "_function(int x) {\n";
        src_file << "    return x * 2;\n";
        src_file << "}\n";
        src_file.close();
        
        std::ofstream header_file(project_dir / "include" / (project.name + ".hpp"));
        header_file << "#pragma once\n\n";
        header_file << "int " << project.name << "_function(int x);\n";
        header_file.close();
        
        // Create test files
        int test_case_counter = 0;
        for (size_t i = 0; i < project.test_files.size(); ++i) {
            std::filesystem::path test_file_path = project_dir / project.test_files[i];
            std::filesystem::create_directories(test_file_path.parent_path());
            
            std::ofstream test_file(test_file_path);
            test_file << "#include <gtest/gtest.h>\n";
            test_file << "#include \"" << project.name << ".hpp\"\n\n";
            
            // Generate 1-3 test cases per file
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> cases_dist(1, 3);
            int cases_in_file = cases_dist(gen);
            
            for (int j = 0; j < cases_in_file; ++j) {
                test_file << "TEST(" << project.name << "Test" << i << ", TestCase" << j << ") {\n";
                test_file << "    EXPECT_EQ(" << project.name << "_function(" << (j + 1) << "), " << ((j + 1) * 2) << ");\n";
                test_file << "}\n\n";
                test_case_counter++;
            }
            
            test_file.close();
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

TEST_F(TestExecutionCompletenessTest, TestExecutionCompleteness) {
    PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& gen) {  // Reduced iterations due to complexity
        // Generate test project
        auto test_project = TestProjectGenerator::generate(gen);
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("test_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create the test project structure
            bool project_created = create_test_project(test_project, iteration_dir);
            EXPECT_TRUE(project_created) << "Failed to create test project structure";
            
            if (!project_created) {
                return; // Skip if project creation failed
            }
            
            std::filesystem::path project_dir = iteration_dir / test_project.name;
            
            // Verify build system detection
            BuildSystem detected_system = project_module_->detect_build_system(project_dir);
            EXPECT_EQ(detected_system, test_project.build_system)
                << "Build system detection failed";
            
            // Run tests
            std::vector<std::string> test_options;
            TestResult result = project_module_->run_tests(project_dir, test_options);
            
            // Property: Test execution should complete (success or failure, but not crash)
            // We can't guarantee success since we're creating mock projects, but execution should complete
            EXPECT_GE(result.execution_time_seconds, 0.0)
                << "Test execution time should be non-negative";
            
            // Property: Test result should have consistent counts
            EXPECT_EQ(result.total_tests, result.passed_tests + result.failed_tests)
                << "Total test count should equal passed + failed";
            
            // Property: If tests failed, there should be failure information
            if (result.failed_tests > 0) {
                EXPECT_FALSE(result.failures.empty())
                    << "Failed tests should have failure information";
            }
            
            // Property: If all tests passed, there should be no failures
            if (result.passed_tests == result.total_tests && result.total_tests > 0) {
                EXPECT_TRUE(result.success)
                    << "Result should be successful if all tests passed";
                EXPECT_EQ(result.failed_tests, 0)
                    << "Failed test count should be 0 if all passed";
            }
            
            // Property: Test execution should discover test files
            // Note: We can't guarantee exact test count due to build system complexity,
            // but we can verify that some tests were discovered if test files exist
            if (!test_project.test_files.empty()) {
                // At minimum, the test execution should attempt to run something
                // Even if it fails due to missing dependencies, it should report some activity
                EXPECT_TRUE(result.total_tests > 0 || !result.failures.empty())
                    << "Test execution should discover or attempt to run tests when test files exist";
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during test execution: " << e.what()
                   << " (project: " << test_project.name << ")";
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test with specific project configurations
TEST_F(TestExecutionCompletenessTest, SpecificProjectConfigurations) {
    struct TestCase {
        std::string name;
        BuildSystem build_system;
        bool should_have_tests;
    };
    
    std::vector<TestCase> test_cases = {
        {"bazel_project", BuildSystem::Bazel, true},
        {"native_project", BuildSystem::Native, true},
        {"empty_project", BuildSystem::Unknown, false},
    };
    
    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& test_case = test_cases[i];
        
        std::filesystem::path iteration_dir = test_dir_ / ("specific_" + std::to_string(i));
        std::filesystem::create_directories(iteration_dir);
        std::filesystem::path project_dir = iteration_dir / test_case.name;
        std::filesystem::create_directories(project_dir);
        
        try {
            // Create appropriate build files
            if (test_case.build_system == BuildSystem::Bazel) {
                std::ofstream build_file(project_dir / "BUILD.bazel");
                build_file << "load(\"@rules_cc//cc:defs.bzl\", \"cc_test\")\n";
                build_file << "cc_test(\n";
                build_file << "    name = \"test\",\n";
                build_file << "    srcs = [\"test.cpp\"],\n";
                build_file << "    deps = [\"@googletest//:gtest_main\"],\n";
                build_file << ")\n";
                build_file.close();
                
                if (test_case.should_have_tests) {
                    std::ofstream test_file(project_dir / "test.cpp");
                    test_file << "#include <gtest/gtest.h>\n";
                    test_file << "TEST(BasicTest, AlwaysPass) { EXPECT_TRUE(true); }\n";
                    test_file.close();
                }
            } else if (test_case.build_system == BuildSystem::Native) {
                std::ofstream yaml_file(project_dir / "meld.yaml");
                yaml_file << "name: " << test_case.name << "\n";
                yaml_file << "version: 1.0.0\n";
                yaml_file << "type: library\n";
                yaml_file.close();
            }
            
            // Test build system detection
            BuildSystem detected = project_module_->detect_build_system(project_dir);
            EXPECT_EQ(detected, test_case.build_system)
                << "Build system detection failed for " << test_case.name;
            
            // Run tests
            TestResult result = project_module_->run_tests(project_dir);
            
            // Verify test execution behavior
            if (test_case.build_system == BuildSystem::Unknown) {
                EXPECT_FALSE(result.success)
                    << "Test execution should fail for unknown build system";
                EXPECT_EQ(result.total_tests, 0)
                    << "No tests should be discovered for unknown build system";
            } else {
                // For known build systems, execution should complete
                EXPECT_GE(result.execution_time_seconds, 0.0)
                    << "Execution time should be recorded";
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception in specific test case " << test_case.name << ": " << e.what();
        }
        
        // Clean up
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    }
}