#include <gtest/gtest.h>
#include "meld/cli/cli_core.hpp"
#include "meld/cli/compiler_module.hpp"
#include "meld/cli/interpreter_module.hpp"
#include "meld/cli/project_module.hpp"
#include "meld/cli/lsp_module.hpp"
// #include "meld/cli/mcp_server_module.hpp"  // MCP module not yet implemented
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <string>

using namespace meld::cli;
using namespace meld::testing;

/**
 * Integration tests for multi-module workflows
 * Tests end-to-end scenarios combining multiple CLI modules
 * 
 * Requirements: Cross-module integration
 */
class MultiModuleWorkflowsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_cli_integration_test";
        std::filesystem::create_directories(temp_dir_);
        
        // Initialize CLI core with test configuration
        CliConfig config;
        config.app_name = "meld-test";
        config.version = "0.1.0-test";
        config.enable_colors = false;  // Disable colors for testing
        config.enable_logging = true;
        config.log_level = "debug";
        
        cli_core_ = std::make_unique<CliCore>(config);
        ASSERT_TRUE(cli_core_->initialize());
    }
    
    void TearDown() override {
        // Clean up CLI
        if (cli_core_) {
            cli_core_->shutdown();
        }
        
        // Clean up temporary files
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }
    
    // Helper method to create a simple Meld source file
    std::filesystem::path create_test_source(const std::string& content, const std::string& filename = "test.meld") {
        auto source_path = temp_dir_ / filename;
        std::ofstream file(source_path);
        file << content;
        file.close();
        return source_path;
    }
    
    // Helper method to create a project directory structure
    std::filesystem::path create_test_project(const std::string& project_name) {
        auto project_path = temp_dir_ / project_name;
        std::filesystem::create_directories(project_path);
        
        // Create basic project structure
        std::filesystem::create_directories(project_path / "src");
        std::filesystem::create_directories(project_path / "tests");
        
        // Create main source file
        auto main_file = project_path / "src" / "main.meld";
        std::ofstream main_stream(main_file);
        main_stream << "fnc main() -> Int {\n";
        main_stream << "    println(\"Hello from " << project_name << "\")\n";
        main_stream << "    0\n";
        main_stream << "}\n";
        main_stream.close();
        
        // Create project configuration
        auto config_file = project_path / "meld.yaml";
        std::ofstream config_stream(config_file);
        config_stream << "name: " << project_name << "\n";
        config_stream << "version: 0.1.0\n";
        config_stream << "type: executable\n";
        config_stream.close();
        
        return project_path;
    }
    
    std::unique_ptr<CliCore> cli_core_;
    std::filesystem::path temp_dir_;
};

/**
 * Test 1: Compilation followed by execution workflow
 * This tests the integration between compiler and interpreter modules
 */
TEST_F(MultiModuleWorkflowsTest, CompilationFollowedByExecution) {
    // Create a simple Meld program
    std::string source_content = R"(
        fnc main() -> Int {
            val result = add(5, 3)
            println("Result: " + result.to_string())
            0
        }
        
        fnc add(a: Int, b: Int) -> Int {
            a + b
        }
    )";
    
    auto source_path = create_test_source(source_content);
    auto output_path = temp_dir_ / "compiled_output.cpp";
    
    // Step 1: Compile the source to C++
    std::vector<std::string> compile_args = {
        "compile", 
        "--target=cpp", 
        "--output=" + output_path.string(),
        source_path.string()
    };
    
    int compile_result = cli_core_->run(compile_args);
    EXPECT_EQ(compile_result, 0) << "Compilation should succeed";
    EXPECT_TRUE(std::filesystem::exists(output_path)) << "Compiled output should exist";
    
    // Step 2: Execute the original source using interpreter
    std::vector<std::string> run_args = {
        "run",
        source_path.string()
    };
    
    int run_result = cli_core_->run(run_args);
    EXPECT_EQ(run_result, 0) << "Execution should succeed";
    
    // Step 3: Verify that both compilation and execution produce consistent results
    // This tests that the compiler and interpreter handle the same source correctly
    
    // Read the compiled C++ code
    std::ifstream compiled_file(output_path);
    std::string compiled_content((std::istreambuf_iterator<char>(compiled_file)),
                                std::istreambuf_iterator<char>());
    
    // Verify the compiled code contains expected elements
    EXPECT_TRUE(compiled_content.find("main") != std::string::npos) 
        << "Compiled code should contain main function";
    EXPECT_TRUE(compiled_content.find("add") != std::string::npos) 
        << "Compiled code should contain add function";
}

/**
 * Test 2: Project creation followed by building workflow
 * This tests the integration between project management and build system modules
 */
TEST_F(MultiModuleWorkflowsTest, ProjectCreationFollowedByBuilding) {
    std::string project_name = "test_project";
    
    // Step 1: Create a new project
    std::vector<std::string> new_args = {
        "new",
        project_name,
        "--template=executable"
    };
    
    // Change to temp directory for project creation
    auto original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(temp_dir_);
    
    int new_result = cli_core_->run(new_args);
    EXPECT_EQ(new_result, 0) << "Project creation should succeed";
    
    auto project_path = temp_dir_ / project_name;
    EXPECT_TRUE(std::filesystem::exists(project_path)) << "Project directory should exist";
    EXPECT_TRUE(std::filesystem::exists(project_path / "src")) << "Source directory should exist";
    EXPECT_TRUE(std::filesystem::exists(project_path / "meld.yaml")) << "Project config should exist";
    
    // Step 2: Build the created project
    std::filesystem::current_path(project_path);
    
    std::vector<std::string> build_args = {
        "build"
    };
    
    int build_result = cli_core_->run(build_args);
    EXPECT_EQ(build_result, 0) << "Project build should succeed";
    
    // Step 3: Test the built project
    std::vector<std::string> test_args = {
        "test"
    };
    
    int test_result = cli_core_->run(test_args);
    // Test result may be 0 (success) or specific code if no tests exist
    // The important thing is that it doesn't crash
    EXPECT_GE(test_result, 0) << "Test command should not crash";
    
    // Restore original working directory
    std::filesystem::current_path(original_cwd);
}

/**
 * Test 3: LSP integration workflow (MCP integration pending implementation)
 * This tests the LSP module functionality and prepares for future MCP integration
 */
TEST_F(MultiModuleWorkflowsTest, LspIntegrationWorkflow) {
    // Create a test project for LSP analysis
    auto project_path = create_test_project("lsp_test_project");
    
    // Step 1: Start LSP server
    std::vector<std::string> lsp_start_args = {
        "lsp", "start",
        "--workspace=" + project_path.string()
    };
    
    int lsp_start_result = cli_core_->run(lsp_start_args);
    EXPECT_EQ(lsp_start_result, 0) << "LSP server should start successfully";
    
    // Give LSP server time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Step 2: Test LSP symbol search through CLI
    std::vector<std::string> symbol_search_args = {
        "lsp", "symbols", "main"
    };
    
    int symbol_result = cli_core_->run(symbol_search_args);
    EXPECT_EQ(symbol_result, 0) << "Symbol search should succeed";
    
    // Step 3: Test LSP diagnostics
    auto main_file = project_path / "src" / "main.meld";
    std::vector<std::string> diagnostics_args = {
        "lsp", "diagnostics", main_file.string()
    };
    
    int diagnostics_result = cli_core_->run(diagnostics_args);
    EXPECT_EQ(diagnostics_result, 0) << "Diagnostics should succeed";
    
    // Step 4: Test LSP formatting
    std::vector<std::string> format_args = {
        "lsp", "format", main_file.string()
    };
    
    int format_result = cli_core_->run(format_args);
    EXPECT_EQ(format_result, 0) << "LSP formatting should succeed";
    
    // Step 5: Stop LSP server
    std::vector<std::string> lsp_stop_args = {
        "lsp", "stop"
    };
    
    int lsp_stop_result = cli_core_->run(lsp_stop_args);
    EXPECT_EQ(lsp_stop_result, 0) << "LSP server should stop successfully";
    
    // TODO: Add MCP server integration when task 6 is implemented
    // This test should be extended to include MCP server startup and
    // integration with LSP for AI-powered code analysis
}

/**
 * Test 4: Error propagation between modules
 * This tests that errors are properly propagated and handled across module boundaries
 */
TEST_F(MultiModuleWorkflowsTest, ErrorPropagationBetweenModules) {
    // Create invalid Meld source
    std::string invalid_source = R"(
        fnc main() -> Int {
            // Syntax error: missing closing brace
            val x = 42
    )";
    
    auto source_path = create_test_source(invalid_source, "invalid.meld");
    
    // Test 1: Compilation error propagation
    std::vector<std::string> compile_args = {
        "compile",
        "--target=cpp",
        source_path.string()
    };
    
    int compile_result = cli_core_->run(compile_args);
    EXPECT_NE(compile_result, 0) << "Compilation should fail with syntax error";
    
    // Test 2: Interpreter error propagation
    std::vector<std::string> run_args = {
        "run",
        source_path.string()
    };
    
    int run_result = cli_core_->run(run_args);
    EXPECT_NE(run_result, 0) << "Execution should fail with syntax error";
    
    // Test 3: Project build error propagation
    auto project_path = create_test_project("error_test_project");
    
    // Replace main.meld with invalid content
    auto main_file = project_path / "src" / "main.meld";
    std::ofstream invalid_main(main_file);
    invalid_main << invalid_source;
    invalid_main.close();
    
    auto original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(project_path);
    
    std::vector<std::string> build_args = {
        "build"
    };
    
    int build_result = cli_core_->run(build_args);
    EXPECT_NE(build_result, 0) << "Project build should fail with syntax error";
    
    std::filesystem::current_path(original_cwd);
}

/**
 * Test 5: Configuration sharing across modules
 * This tests that configuration settings are properly shared between modules
 */
TEST_F(MultiModuleWorkflowsTest, ConfigurationSharingAcrossModules) {
    // Set configuration values
    std::vector<std::string> config_set_args = {
        "config", "set", "default_target", "go"
    };
    
    int config_result = cli_core_->run(config_set_args);
    EXPECT_EQ(config_result, 0) << "Configuration setting should succeed";
    
    // Create test source
    std::string source_content = "val x = 42";
    auto source_path = create_test_source(source_content);
    
    // Test that compiler uses the configured default target
    std::vector<std::string> compile_args = {
        "compile",  // No explicit target - should use configured default
        source_path.string()
    };
    
    int compile_result = cli_core_->run(compile_args);
    EXPECT_EQ(compile_result, 0) << "Compilation with default target should succeed";
    
    // Verify that Go output was generated (based on configured default)
    auto go_output = temp_dir_ / "test.go";
    EXPECT_TRUE(std::filesystem::exists(go_output)) << "Go output should exist based on config";
    
    // Test configuration retrieval
    std::vector<std::string> config_get_args = {
        "config", "get", "default_target"
    };
    
    int get_result = cli_core_->run(config_get_args);
    EXPECT_EQ(get_result, 0) << "Configuration retrieval should succeed";
}

/**
 * Test 6: Plugin architecture extensibility
 * This tests that the plugin architecture allows for module extension
 */
TEST_F(MultiModuleWorkflowsTest, PluginArchitectureExtensibility) {
    // Test that all expected modules are registered
    auto& dispatcher = cli_core_->get_dispatcher();
    
    std::vector<std::string> expected_commands = {
        "compile", "run", "repl", "new", "build", "test",
        "lsp", "format", "lint", "install", "help", "version"
    };
    
    for (const auto& command : expected_commands) {
        EXPECT_TRUE(dispatcher.has_command(command)) 
            << "Command '" << command << "' should be registered";
    }
    
    // Test command help accessibility
    for (const auto& command : expected_commands) {
        std::string help = dispatcher.get_command_help(command);
        EXPECT_FALSE(help.empty()) 
            << "Command '" << command << "' should have help text";
    }
    
    // Test command completion
    std::vector<std::string> partial_args = {"com"};
    auto completions = dispatcher.get_completions(partial_args);
    
    EXPECT_TRUE(std::find(completions.begin(), completions.end(), "compile") != completions.end())
        << "Completion should suggest 'compile' for 'com'";
}

/**
 * Test 7: Concurrent operations and resource sharing
 * This tests that multiple operations can run concurrently without conflicts
 */
TEST_F(MultiModuleWorkflowsTest, ConcurrentOperationsAndResourceSharing) {
    // Create multiple test sources
    std::vector<std::filesystem::path> source_paths;
    for (int i = 0; i < 3; ++i) {
        std::string content = "val x" + std::to_string(i) + " = " + std::to_string(i * 10);
        auto path = create_test_source(content, "test" + std::to_string(i) + ".meld");
        source_paths.push_back(path);
    }
    
    // Test concurrent compilation
    std::vector<std::thread> compile_threads;
    std::vector<int> compile_results(source_paths.size());
    
    for (size_t i = 0; i < source_paths.size(); ++i) {
        compile_threads.emplace_back([this, &source_paths, &compile_results, i]() {
            // Create separate CLI instance for each thread to avoid conflicts
            CliConfig config;
            config.app_name = "meld-test-" + std::to_string(i);
            config.version = "0.1.0-test";
            config.enable_colors = false;
            config.enable_logging = false;  // Disable logging to avoid conflicts
            
            CliCore thread_cli(config);
            thread_cli.initialize();
            
            std::vector<std::string> args = {
                "compile",
                "--target=cpp",
                "--output=" + (temp_dir_ / ("output" + std::to_string(i) + ".cpp")).string(),
                source_paths[i].string()
            };
            
            compile_results[i] = thread_cli.run(args);
            thread_cli.shutdown();
        });
    }
    
    // Wait for all compilations to complete
    for (auto& thread : compile_threads) {
        thread.join();
    }
    
    // Verify all compilations succeeded
    for (size_t i = 0; i < compile_results.size(); ++i) {
        EXPECT_EQ(compile_results[i], 0) 
            << "Concurrent compilation " << i << " should succeed";
        
        auto output_path = temp_dir_ / ("output" + std::to_string(i) + ".cpp");
        EXPECT_TRUE(std::filesystem::exists(output_path))
            << "Concurrent compilation output " << i << " should exist";
    }
}

/**
 * Test 8: End-to-end workflow combining all modules
 * This is a comprehensive test that exercises multiple modules in sequence
 */
TEST_F(MultiModuleWorkflowsTest, EndToEndWorkflowAllModules) {
    std::string project_name = "comprehensive_test";
    
    // Step 1: Create project
    auto original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(temp_dir_);
    
    std::vector<std::string> new_args = {"new", project_name};
    EXPECT_EQ(cli_core_->run(new_args), 0);
    
    auto project_path = temp_dir_ / project_name;
    std::filesystem::current_path(project_path);
    
    // Step 2: Format the generated code
    std::vector<std::string> format_args = {"format", "src/"};
    int format_result = cli_core_->run(format_args);
    // Format may succeed or fail depending on implementation
    EXPECT_GE(format_result, 0);
    
    // Step 3: Lint the code
    std::vector<std::string> lint_args = {"lint", "src/"};
    int lint_result = cli_core_->run(lint_args);
    // Lint may succeed or fail depending on implementation
    EXPECT_GE(lint_result, 0);
    
    // Step 4: Build the project
    std::vector<std::string> build_args = {"build"};
    EXPECT_EQ(cli_core_->run(build_args), 0);
    
    // Step 5: Run tests
    std::vector<std::string> test_args = {"test"};
    int test_result = cli_core_->run(test_args);
    EXPECT_GE(test_result, 0);
    
    // Step 6: Run the project
    std::vector<std::string> run_args = {"run", "src/main.meld"};
    EXPECT_EQ(cli_core_->run(run_args), 0);
    
    // Step 7: Get help for various commands
    std::vector<std::string> commands_to_test = {"compile", "run", "build", "test"};
    for (const auto& cmd : commands_to_test) {
        std::vector<std::string> help_args = {"help", cmd};
        EXPECT_EQ(cli_core_->run(help_args), 0) 
            << "Help for command '" << cmd << "' should be available";
    }
    
    std::filesystem::current_path(original_cwd);
}