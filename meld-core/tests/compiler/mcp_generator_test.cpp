#include <gtest/gtest.h>
#include "../../include/meld/compiler/mcp_generator.hpp"
#include "../../include/meld/parser/parser.hpp"
#include "../../include/meld/macro/blueprint.hpp"
#include <filesystem>
#include <fstream>

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::macro;

class MCPGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        generator = std::make_unique<MCPGenerator>();
        parser = std::make_unique<Parser>();
        
        // Clear blueprint registry
        BlueprintRegistry::instance().clear();
        
        // Create test output directory
        test_output_dir = "./test_mcp_output";
        std::filesystem::create_directories(test_output_dir);
    }
    
    void TearDown() override {
        // Clean up test output directory
        if (std::filesystem::exists(test_output_dir)) {
            std::filesystem::remove_all(test_output_dir);
        }
        
        BlueprintRegistry::instance().clear();
    }
    
    std::vector<ast::expression> parse_source(const std::string& source) {
        std::vector<ast::expression> expressions;
        if (!parser->parse_file(source, expressions)) {
            throw std::runtime_error("Parse error: " + parser->error_message());
        }
        return expressions;
    }
    
    std::unique_ptr<MCPGenerator> generator;
    std::unique_ptr<Parser> parser;
    std::string test_output_dir;
};

TEST_F(MCPGeneratorTest, GenerateBasicMCPTool) {
    std::string source = R"(
        fnc calculate_distance(x1: int, y1: int, x2: int, y2: int) -> float {
            val dx = x2 - x1
            val dy = y2 - y1
            rtn sqrt(dx * dx + dy * dy)
        }
    )";
    
    auto expressions = parse_source(source);
    ASSERT_EQ(expressions.size(), 1);
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&expressions[0]);
    auto func_def = func_def_fwd ? &func_def_fwd->get() : nullptr;
    ASSERT_NE(func_def, nullptr);
    
    MCPGenerationOptions options;
    auto tool = generator->generate_tool_from_function(*func_def, options);
    
    ASSERT_TRUE(tool.has_value());
    EXPECT_EQ(tool->name, "calculate_distance");
    EXPECT_FALSE(tool->description.empty());
    EXPECT_EQ(tool->input_schema.size(), 4);
    EXPECT_EQ(tool->required_parameters.size(), 4);
    
    // Check parameter types
    EXPECT_EQ(tool->input_schema["x1"], "integer");
    EXPECT_EQ(tool->input_schema["y1"], "integer");
    EXPECT_EQ(tool->input_schema["x2"], "integer");
    EXPECT_EQ(tool->input_schema["y2"], "integer");
    
    // Check required parameters
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "x1") != tool->required_parameters.end());
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "y1") != tool->required_parameters.end());
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "x2") != tool->required_parameters.end());
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "y2") != tool->required_parameters.end());
}

TEST_F(MCPGeneratorTest, GenerateMCPToolWithBlueprint) {
    std::string source = R"(
        @blueprint {
            summary: "Calculate the Euclidean distance between two points",
            rules: ["Input coordinates must be valid integers", "Returns positive distance"],
            examples: ["distance(0, 0, 3, 4) -> 5.0"]
        }
        fnc calculate_distance(x1: int, y1: int, x2: int, y2: int) -> float {
            val dx = x2 - x1
            val dy = y2 - y1
            rtn sqrt(dx * dx + dy * dy)
        }
    )";
    
    // Register blueprint manually for testing
    BlueprintMetadata blueprint;
    blueprint.summary = "Calculate the Euclidean distance between two points";
    blueprint.rules = {"Input coordinates must be valid integers", "Returns positive distance"};
    
    // Create example with v2.0 format
    ExamplePair example;
    example.input = "distance(0, 0, 3, 4)";
    example.output = "5.0";
    blueprint.examples = {example};
    
    blueprint.id = "calculate_distance_test";
    BlueprintRegistry::instance().register_blueprint("calculate_distance", blueprint);
    
    auto expressions = parse_source(source);
    ASSERT_EQ(expressions.size(), 1);
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&expressions[0]);
    auto func_def = func_def_fwd ? &func_def_fwd->get() : nullptr;
    ASSERT_NE(func_def, nullptr);
    
    MCPGenerationOptions options;
    auto tool = generator->generate_tool_from_function(*func_def, options);
    
    ASSERT_TRUE(tool.has_value());
    EXPECT_EQ(tool->name, "calculate_distance");
    EXPECT_EQ(tool->description, "Calculate the Euclidean distance between two points\n\nRules:\n- Input coordinates must be valid integers\n- Returns positive distance\n\nExamples:\n- distance(0, 0, 3, 4) -> 5.0\n");
    EXPECT_TRUE(tool->blueprint_id.has_value());
    EXPECT_EQ(*tool->blueprint_id, "calculate_distance_test");
}

TEST_F(MCPGeneratorTest, GenerateMCPToolWithOptionalParameters) {
    std::string source = R"(
        fnc format_message(message: string, prefix: string = "INFO", timestamp: bool = false) -> string {
            val result = prefix + ": " + message
            rtn if (timestamp) get_timestamp() + " " + result else result
        }
    )";
    
    auto expressions = parse_source(source);
    ASSERT_EQ(expressions.size(), 1);
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&expressions[0]);
    auto func_def = func_def_fwd ? &func_def_fwd->get() : nullptr;
    ASSERT_NE(func_def, nullptr);
    
    MCPGenerationOptions options;
    auto tool = generator->generate_tool_from_function(*func_def, options);
    
    ASSERT_TRUE(tool.has_value());
    EXPECT_EQ(tool->name, "format_message");
    EXPECT_EQ(tool->input_schema.size(), 3);
    EXPECT_EQ(tool->required_parameters.size(), 1);  // Only 'message' is required
    
    // Check parameter types
    EXPECT_EQ(tool->input_schema["message"], "string");
    EXPECT_EQ(tool->input_schema["prefix"], "string");
    EXPECT_EQ(tool->input_schema["timestamp"], "boolean");
    
    // Check only required parameter
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "message") != tool->required_parameters.end());
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "prefix") == tool->required_parameters.end());
    EXPECT_TRUE(std::find(tool->required_parameters.begin(), tool->required_parameters.end(), "timestamp") == tool->required_parameters.end());
}

TEST_F(MCPGeneratorTest, GenerateFullMCPConfig) {
    std::string source = R"(
        fnc add(a: int, b: int) -> int {
            rtn a + b
        }
        
        fnc multiply(x: float, y: float) -> float {
            rtn x * y
        }
        
        fnc _private_function(data: string) -> string {
            rtn "processed: " + data
        }
        
        fnc test_something() {
            test "basic test" {
                assert(true)
            }
        }
    )";
    
    auto expressions = parse_source(source);
    ASSERT_EQ(expressions.size(), 4);
    
    MCPGenerationOptions options;
    options.server_name = "test-mcp-server";
    options.server_version = "1.0.0";
    options.include_private_functions = false;
    options.include_test_functions = false;
    
    auto config = generator->generate_mcp_config(expressions, options);
    
    EXPECT_EQ(config.name, "test-mcp-server");
    EXPECT_EQ(config.version, "1.0.0");
    EXPECT_FALSE(config.description.empty());
    
    // Should include add and multiply, but not _private_function or test_something
    EXPECT_EQ(config.tools.size(), 2);
    
    bool found_add = false, found_multiply = false;
    for (const auto& tool : config.tools) {
        if (tool.name == "add") {
            found_add = true;
            EXPECT_EQ(tool.input_schema.size(), 2);
            EXPECT_EQ(tool.required_parameters.size(), 2);
        } else if (tool.name == "multiply") {
            found_multiply = true;
            EXPECT_EQ(tool.input_schema.size(), 2);
            EXPECT_EQ(tool.required_parameters.size(), 2);
        }
    }
    
    EXPECT_TRUE(found_add);
    EXPECT_TRUE(found_multiply);
}

TEST_F(MCPGeneratorTest, GenerateMCPConfigWithPrivateFunctions) {
    std::string source = R"(
        fnc public_function(x: int) -> int {
            rtn x * 2
        }
        
        fnc _private_function(y: int) -> int {
            rtn y + 1
        }
    )";
    
    auto expressions = parse_source(source);
    ASSERT_EQ(expressions.size(), 2);
    
    MCPGenerationOptions options;
    options.include_private_functions = true;
    
    auto config = generator->generate_mcp_config(expressions, options);
    
    // Should include both functions
    EXPECT_EQ(config.tools.size(), 2);
    
    bool found_public = false, found_private = false;
    for (const auto& tool : config.tools) {
        if (tool.name == "public_function") {
            found_public = true;
        } else if (tool.name == "_private_function") {
            found_private = true;
        }
    }
    
    EXPECT_TRUE(found_public);
    EXPECT_TRUE(found_private);
}

TEST_F(MCPGeneratorTest, WriteMCPFiles) {
    MCPServerConfig config;
    config.name = "test-server";
    config.version = "1.0.0";
    config.description = "Test MCP server";
    
    MCPTool tool;
    tool.name = "test_tool";
    tool.description = "A test tool";
    tool.input_schema["param1"] = "string";
    tool.required_parameters.push_back("param1");
    tool.function_signature = "fnc test_tool(param1: string) -> string";
    
    config.tools.push_back(tool);
    
    MCPGenerationOptions options;
    options.output_directory = test_output_dir;
    options.generate_server_config = true;
    
    bool success = generator->write_mcp_files(config, options);
    EXPECT_TRUE(success);
    
    // Check that files were created
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/mcp-config.json"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/server.js"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/package.json"));
    EXPECT_TRUE(std::filesystem::exists(test_output_dir + "/README.md"));
    
    // Check config file content
    std::ifstream config_file(test_output_dir + "/mcp-config.json");
    ASSERT_TRUE(config_file.is_open());
    
    std::string config_content((std::istreambuf_iterator<char>(config_file)),
                               std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(config_content.find("test-server") != std::string::npos);
    EXPECT_TRUE(config_content.find("test_tool") != std::string::npos);
    EXPECT_TRUE(config_content.find("A test tool") != std::string::npos);
}

TEST_F(MCPGeneratorTest, ValidateMCPConfig) {
    MCPServerConfig valid_config;
    valid_config.name = "valid-server";
    valid_config.version = "1.0.0";
    valid_config.description = "Valid server";
    
    MCPTool tool;
    tool.name = "valid_tool";
    tool.description = "Valid tool";
    valid_config.tools.push_back(tool);
    
    auto errors = generator->validate_mcp_config(valid_config);
    EXPECT_TRUE(errors.empty());
    
    // Test invalid config
    MCPServerConfig invalid_config;
    // Missing name, version, and tools
    
    errors = generator->validate_mcp_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_GE(errors.size(), 3);  // At least name, version, and tools errors
}

TEST_F(MCPGeneratorTest, IncrementalMCPUpdate) {
    // Create initial config
    MCPServerConfig initial_config;
    initial_config.name = "test-server";
    initial_config.version = "1.0.0";
    
    MCPTool existing_tool;
    existing_tool.name = "existing_tool";
    existing_tool.description = "Existing tool";
    initial_config.tools.push_back(existing_tool);
    
    // Create new expressions
    std::string new_source = R"(
        fnc new_function(x: int) -> int {
            rtn x * 3
        }
        
        fnc existing_tool(y: string) -> string {
            rtn "updated: " + y
        }
    )";
    
    auto new_expressions = parse_source(new_source);
    
    MCPGenerationOptions options;
    auto updated_config = generator->generate_incremental_update(initial_config, new_expressions, options);
    
    EXPECT_EQ(updated_config.name, "test-server");
    EXPECT_EQ(updated_config.version, "1.0.0");
    EXPECT_EQ(updated_config.tools.size(), 2);  // existing_tool updated, new_function added
    
    bool found_existing = false, found_new = false;
    for (const auto& tool : updated_config.tools) {
        if (tool.name == "existing_tool") {
            found_existing = true;
            // Should be updated version
        } else if (tool.name == "new_function") {
            found_new = true;
        }
    }
    
    EXPECT_TRUE(found_existing);
    EXPECT_TRUE(found_new);
}

TEST_F(MCPGeneratorTest, TypeMappingToJSONSchema) {
    EXPECT_EQ(meld_type_to_json_schema("int"), "integer");
    EXPECT_EQ(meld_type_to_json_schema("i32"), "integer");
    EXPECT_EQ(meld_type_to_json_schema("i64"), "integer");
    EXPECT_EQ(meld_type_to_json_schema("float"), "number");
    EXPECT_EQ(meld_type_to_json_schema("f32"), "number");
    EXPECT_EQ(meld_type_to_json_schema("f64"), "number");
    EXPECT_EQ(meld_type_to_json_schema("string"), "string");
    EXPECT_EQ(meld_type_to_json_schema("bool"), "boolean");
    EXPECT_EQ(meld_type_to_json_schema("list<int>"), "array");
    EXPECT_EQ(meld_type_to_json_schema("array<string>"), "array");
    EXPECT_EQ(meld_type_to_json_schema("map<string, int>"), "object");
    EXPECT_EQ(meld_type_to_json_schema("object"), "object");
    EXPECT_EQ(meld_type_to_json_schema("CustomType"), "string");  // Unknown types default to string
}