#include <gtest/gtest.h>
#include "../../include/meld/compiler/compiler.hpp"
#include "../../include/meld/macro/blueprint.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace meld::compiler;
using namespace meld::macro;

class MCPIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler = std::make_unique<Compiler>();
        
        // Clear blueprint registry
        BlueprintRegistry::instance().clear();
        
        // Create test output directory
        test_output_dir = "./test_mcp_integration";
        std::filesystem::create_directories(test_output_dir);
    }
    
    void TearDown() override {
        // Clean up test output directory
        if (std::filesystem::exists(test_output_dir)) {
            std::filesystem::remove_all(test_output_dir);
        }
        
        BlueprintRegistry::instance().clear();
    }
    
    std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file) return "";
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::unique_ptr<Compiler> compiler;
    std::string test_output_dir;
};

TEST_F(MCPIntegrationTest, EndToEndMCPGeneration) {
    // Create a test Meld source file with @blueprint annotations
    std::string source = R"(
        @blueprint {
            summary: "Calculate the area of a rectangle",
            rules: ["Width and height must be positive numbers"],
            examples: ["area(5, 3) -> 15"]
        }
        fnc calculate_area(width: float, height: float) -> float {
            rtn width * height
        }
        
        @blueprint {
            summary: "Format a greeting message",
            rules: ["Name should not be empty"],
            examples: ["greet('Alice') -> 'Hello, Alice!'"]
        }
        fnc greet(name: string, formal: bool = false) -> string {
            val prefix = if (formal) "Good day" else "Hello"
            rtn prefix + ", " + name + "!"
        }
        
        fnc _internal_helper(data: string) -> string {
            rtn "processed: " + data
        }
        
        fnc test_calculation() {
            test "area calculation" {
                assert(calculate_area(5.0, 3.0) == 15.0)
            }
        }
    )";
    
    // Register blueprints manually for testing
    BlueprintMetadata area_blueprint;
    area_blueprint.summary = "Calculate the area of a rectangle";
    area_blueprint.rules = {"Width and height must be positive numbers"};
    
    // Create example with v2.0 format
    ExamplePair area_example;
    area_example.input = "area(5, 3)";
    area_example.output = "15";
    area_blueprint.examples = {area_example};
    
    area_blueprint.id = "calculate_area_test";
    BlueprintRegistry::instance().register_blueprint("calculate_area", area_blueprint);
    
    BlueprintMetadata greet_blueprint;
    greet_blueprint.summary = "Format a greeting message";
    greet_blueprint.rules = {"Name should not be empty"};
    
    // Create example with v2.0 format
    ExamplePair greet_example;
    greet_example.input = "greet('Alice')";
    greet_example.output = "'Hello, Alice!'";
    greet_blueprint.examples = {greet_example};
    
    greet_blueprint.id = "greet_test";
    BlueprintRegistry::instance().register_blueprint("greet", greet_blueprint);
    
    // Set up compilation options for MCP generation
    CompilationOptions options;
    options.generate_mcp = true;
    options.mcp_options.server_name = "test-mcp-server";
    options.mcp_options.server_version = "1.0.0";
    options.mcp_options.output_directory = test_output_dir;
    options.mcp_options.include_private_functions = false;
    options.mcp_options.include_test_functions = false;
    options.mcp_options.include_blueprint_metadata = true;
    options.mcp_options.generate_server_config = true;
    
    // Compile source with MCP generation
    auto result = compiler->compile_source(source, "test.meld", options);
    
    // Verify compilation succeeded
    EXPECT_TRUE(result.success) << "Compilation failed";
    EXPECT_TRUE(result.mcp_generated) << "MCP generation failed";
    EXPECT_EQ(result.mcp_tools_generated, 2) << "Expected 2 tools (calculate_area, greet)";
    EXPECT_EQ(result.mcp_output_path, test_output_dir);
    
    // Verify MCP files were created
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/mcp-config.json"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/server.js"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/package.json"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/README.md"));
    
    // Verify MCP configuration content
    std::string config_content = read_file(test_output_dir + "/mcp-config.json");
    EXPECT_FALSE(config_content.empty());
    
    // Check that the config contains expected tools
    EXPECT_TRUE(config_content.find("calculate_area") != std::string::npos);
    EXPECT_TRUE(config_content.find("greet") != std::string::npos);
    EXPECT_TRUE(config_content.find("Calculate the area of a rectangle") != std::string::npos);
    EXPECT_TRUE(config_content.find("Format a greeting message") != std::string::npos);
    
    // Check that private and test functions are excluded
    EXPECT_TRUE(config_content.find("_internal_helper") == std::string::npos);
    EXPECT_TRUE(config_content.find("test_calculation") == std::string::npos);
    
    // Verify server.js content
    std::string server_content = read_file(test_output_dir + "/server.js");
    EXPECT_FALSE(server_content.empty());
    EXPECT_TRUE(server_content.find("test-mcp-server") != std::string::npos);
    EXPECT_TRUE(server_content.find("calculate_area") != std::string::npos);
    EXPECT_TRUE(server_content.find("greet") != std::string::npos);
    
    // Verify package.json content
    std::string package_content = read_file(test_output_dir + "/package.json");
    EXPECT_FALSE(package_content.empty());
    EXPECT_TRUE(package_content.find("test-mcp-server") != std::string::npos);
    EXPECT_TRUE(package_content.find("1.0.0") != std::string::npos);
    EXPECT_TRUE(package_content.find("@modelcontextprotocol/sdk") != std::string::npos);
    
    // Verify README.md content
    std::string readme_content = read_file(test_output_dir + "/README.md");
    EXPECT_FALSE(readme_content.empty());
    EXPECT_TRUE(readme_content.find("test-mcp-server") != std::string::npos);
    EXPECT_TRUE(readme_content.find("calculate_area") != std::string::npos);
    EXPECT_TRUE(readme_content.find("greet") != std::string::npos);
}

TEST_F(MCPIntegrationTest, MCPGenerationWithIncludeOptions) {
    std::string source = R"(
        fnc public_function(x: int) -> int {
            rtn x * 2
        }
        
        fnc _private_function(y: int) -> int {
            rtn y + 1
        }
        
        fnc test_something() {
            test "basic test" {
                assert(true)
            }
        }
    )";
    
    // Test with include_private_functions = true
    CompilationOptions options;
    options.generate_mcp = true;
    options.mcp_options.server_name = "inclusive-server";
    options.mcp_options.output_directory = test_output_dir;
    options.mcp_options.include_private_functions = true;
    options.mcp_options.include_test_functions = true;
    
    auto result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.mcp_generated);
    EXPECT_EQ(result.mcp_tools_generated, 3);  // All functions included
    
    // Verify config includes all functions
    std::string config_content = read_file(test_output_dir + "/mcp-config.json");
    EXPECT_TRUE(config_content.find("public_function") != std::string::npos);
    EXPECT_TRUE(config_content.find("_private_function") != std::string::npos);
    EXPECT_TRUE(config_content.find("test_something") != std::string::npos);
}

TEST_F(MCPIntegrationTest, MCPGenerationWithExclusions) {
    std::string source = R"(
        fnc function_a(x: int) -> int {
            rtn x + 1
        }
        
        fnc function_b(y: int) -> int {
            rtn y * 2
        }
        
        fnc function_c(z: int) -> int {
            rtn z - 1
        }
    )";
    
    CompilationOptions options;
    options.generate_mcp = true;
    options.mcp_options.server_name = "selective-server";
    options.mcp_options.output_directory = test_output_dir;
    options.mcp_options.exclude_functions = {"function_b"};
    
    auto result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.mcp_generated);
    EXPECT_EQ(result.mcp_tools_generated, 2);  // function_a and function_c
    
    // Verify config excludes function_b
    std::string config_content = read_file(test_output_dir + "/mcp-config.json");
    EXPECT_TRUE(config_content.find("function_a") != std::string::npos);
    EXPECT_TRUE(config_content.find("function_b") == std::string::npos);
    EXPECT_TRUE(config_content.find("function_c") != std::string::npos);
}

TEST_F(MCPIntegrationTest, MCPGenerationValidation) {
    // Test with invalid configuration (empty server name)
    std::string source = R"(
        fnc test_function(x: int) -> int {
            rtn x
        }
    )";
    
    CompilationOptions options;
    options.generate_mcp = true;
    options.mcp_options.server_name = "";  // Invalid empty name
    options.mcp_options.output_directory = test_output_dir;
    
    auto result = compiler->compile_source(source, "test.meld", options);
    
    // Compilation should succeed but MCP generation should fail validation
    EXPECT_TRUE(result.success);  // Source compilation succeeds
    EXPECT_FALSE(result.mcp_generated);  // But MCP generation fails
    
    // Should have validation error in diagnostics
    bool found_validation_error = false;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.message.find("validation error") != std::string::npos) {
            found_validation_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_validation_error);
}

TEST_F(MCPIntegrationTest, IncrementalMCPGeneration) {
    // First compilation
    std::string initial_source = R"(
        fnc initial_function(x: int) -> int {
            rtn x * 2
        }
    )";
    
    CompilationOptions options;
    options.generate_mcp = true;
    options.mcp_options.server_name = "incremental-server";
    options.mcp_options.output_directory = test_output_dir;
    
    auto initial_result = compiler->compile_source(initial_source, "initial.meld", options);
    EXPECT_TRUE(initial_result.success);
    EXPECT_TRUE(initial_result.mcp_generated);
    EXPECT_EQ(initial_result.mcp_tools_generated, 1);
    
    // Verify initial config
    std::string initial_config = read_file(test_output_dir + "/mcp-config.json");
    EXPECT_TRUE(initial_config.find("initial_function") != std::string::npos);
    
    // Second compilation with additional functions
    std::string updated_source = R"(
        fnc initial_function(x: int) -> int {
            rtn x * 3  // Updated implementation
        }
        
        fnc new_function(y: string) -> string {
            rtn "processed: " + y
        }
    )";
    
    auto updated_result = compiler->compile_source(updated_source, "updated.meld", options);
    EXPECT_TRUE(updated_result.success);
    EXPECT_TRUE(updated_result.mcp_generated);
    EXPECT_EQ(updated_result.mcp_tools_generated, 2);
    
    // Verify updated config contains both functions
    std::string updated_config = read_file(test_output_dir + "/mcp-config.json");
    EXPECT_TRUE(updated_config.find("initial_function") != std::string::npos);
    EXPECT_TRUE(updated_config.find("new_function") != std::string::npos);
}