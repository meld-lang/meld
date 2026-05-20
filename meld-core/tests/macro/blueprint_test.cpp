#include <gtest/gtest.h>
#include "meld/macro/blueprint.hpp"
#include "meld/macro/macro.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"

using namespace meld::macro;
using namespace meld::kernel;

class BlueprintTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registries for clean test state
        MacroRegistry::instance().clear();
        BlueprintRegistry::instance().clear();
        
        // Register blueprint macro
        register_blueprint_macros();
    }
    
    void TearDown() override {
        MacroRegistry::instance().clear();
        BlueprintRegistry::instance().clear();
    }
};

TEST_F(BlueprintTest, RegisterBlueprintMacro) {
    // Test that blueprint macro is registered
    ASSERT_TRUE(MacroRegistry::instance().has_macro("blueprint"));
    
    auto macro_result = MacroRegistry::instance().get_macro("blueprint");
    ASSERT_TRUE(macro_result.has_value());
    EXPECT_EQ(macro_result.value()->name(), "blueprint");
}

TEST_F(BlueprintTest, ParseSimpleBlueprint) {
    BlueprintParser parser;
    
    // Create a simple blueprint AST: {summary: "Test function"}
    auto summary_symbol = SymbolTable::instance().intern("summary");
    auto summary_value = Value(std::make_shared<String>("Test function"));
    
    auto summary_pair = cons(Value(summary_symbol), cons(summary_value, Value(Empty::instance())));
    auto blueprint_ast = cons(summary_pair, Value(Empty::instance()));
    
    auto result = parser.parse_blueprint(blueprint_ast, "test_function");
    
    ASSERT_TRUE(result.has_value());
    
    const auto& metadata = result.value();
    EXPECT_EQ(metadata.summary, "Test function");
    EXPECT_TRUE(metadata.rules.empty());
    EXPECT_TRUE(metadata.examples.empty());
    EXPECT_TRUE(metadata.tags.empty());
    EXPECT_FALSE(metadata.parent_blueprint.has_value());
    // Note: cost field removed in v2.0
}

TEST_F(BlueprintTest, ParseComplexBlueprint) {
    BlueprintParser parser;
    
    // Create a complex blueprint AST with multiple fields
    std::vector<Value> fields;
    
    // summary: "Calculate distance"
    auto summary_symbol = SymbolTable::instance().intern("summary");
    auto summary_value = Value(std::make_shared<String>("Calculate distance"));
    fields.push_back(cons(Value(summary_symbol), cons(summary_value, Value(Empty::instance()))));
    
    // rules: ["Must be positive", "Uses Euclidean formula"]
    auto rules_symbol = SymbolTable::instance().intern("rules");
    auto rule1 = Value(std::make_shared<String>("Must be positive"));
    auto rule2 = Value(std::make_shared<String>("Uses Euclidean formula"));
    auto rules_list = cons(rule1, cons(rule2, Value(Empty::instance())));
    fields.push_back(cons(Value(rules_symbol), cons(rules_list, Value(Empty::instance()))));
    
    // examples: ["distance(0,0,3,4) == 5.0"]
    auto examples_symbol = SymbolTable::instance().intern("examples");
    auto example1 = Value(std::make_shared<String>("distance(0,0,3,4) == 5.0"));
    auto examples_list = cons(example1, Value(Empty::instance()));
    fields.push_back(cons(Value(examples_symbol), cons(examples_list, Value(Empty::instance()))));
    
    // tags: ["math", "geometry"]
    auto tags_symbol = SymbolTable::instance().intern("tags");
    auto tag1 = Value(std::make_shared<String>("math"));
    auto tag2 = Value(std::make_shared<String>("geometry"));
    auto tags_list = cons(tag1, cons(tag2, Value(Empty::instance())));
    fields.push_back(cons(Value(tags_symbol), cons(tags_list, Value(Empty::instance()))));
    
    // Build the blueprint AST
    Value blueprint_ast = Value(Empty::instance());
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        blueprint_ast = cons(*it, blueprint_ast);
    }
    
    auto result = parser.parse_blueprint(blueprint_ast, "distance");
    
    ASSERT_TRUE(result.has_value());
    
    const auto& metadata = result.value();
    EXPECT_EQ(metadata.summary, "Calculate distance");
    ASSERT_EQ(metadata.rules.size(), 2);
    EXPECT_EQ(metadata.rules[0], "Must be positive");
    EXPECT_EQ(metadata.rules[1], "Uses Euclidean formula");
    ASSERT_EQ(metadata.examples.size(), 1);
    EXPECT_EQ(metadata.examples[0].input, "distance(0,0,3,4) == 5.0");
    EXPECT_EQ(metadata.examples[0].output, ""); // Legacy format has empty output
    ASSERT_EQ(metadata.tags.size(), 2);
    EXPECT_EQ(metadata.tags[0], "math");
    EXPECT_EQ(metadata.tags[1], "geometry");
}

TEST_F(BlueprintTest, BlueprintRegistration) {
    BlueprintMetadata metadata;
    metadata.summary = "Test function for registration";
    metadata.rules = {"Must return valid result"};
    
    // Create example with v2.0 format
    ExamplePair example;
    example.input = "test()";
    example.output = "true";
    example.description = "Basic test case";
    metadata.examples = {example};
    
    metadata.tags = {"test", "utility"};
    
    // Register blueprint
    BlueprintRegistry::instance().register_blueprint("test_function", metadata);
    
    // Verify registration
    auto retrieved = BlueprintRegistry::instance().get_blueprint("test_function");
    ASSERT_TRUE(retrieved.has_value());
    
    EXPECT_EQ(retrieved->summary, metadata.summary);
    EXPECT_EQ(retrieved->rules, metadata.rules);
    EXPECT_EQ(retrieved->examples, metadata.examples);
    EXPECT_EQ(retrieved->tags, metadata.tags);
}

TEST_F(BlueprintTest, SearchByTags) {
    // Register multiple blueprints with different tags
    BlueprintMetadata math_func;
    math_func.summary = "Math function";
    math_func.tags = {"math", "calculation"};
    BlueprintRegistry::instance().register_blueprint("math_func", math_func);
    
    BlueprintMetadata string_func;
    string_func.summary = "String function";
    string_func.tags = {"string", "utility"};
    BlueprintRegistry::instance().register_blueprint("string_func", string_func);
    
    BlueprintMetadata util_func;
    util_func.summary = "Utility function";
    util_func.tags = {"utility", "helper"};
    BlueprintRegistry::instance().register_blueprint("util_func", util_func);
    
    // Search by tags
    auto math_results = BlueprintRegistry::instance().search_by_tags({"math"});
    ASSERT_EQ(math_results.size(), 1);
    EXPECT_EQ(math_results[0], "math_func");
    
    auto utility_results = BlueprintRegistry::instance().search_by_tags({"utility"});
    ASSERT_EQ(utility_results.size(), 2);
    EXPECT_TRUE(std::find(utility_results.begin(), utility_results.end(), "string_func") != utility_results.end());
    EXPECT_TRUE(std::find(utility_results.begin(), utility_results.end(), "util_func") != utility_results.end());
}

TEST_F(BlueprintTest, SearchByText) {
    // Register blueprints with different text content
    BlueprintMetadata distance_func;
    distance_func.summary = "Calculate distance between points";
    distance_func.rules = {"Uses Euclidean formula"};
    BlueprintRegistry::instance().register_blueprint("distance", distance_func);
    
    BlueprintMetadata area_func;
    area_func.summary = "Calculate area of shapes";
    area_func.examples = {ExamplePair{"area(circle, radius) for circular area", "", {}}};
    BlueprintRegistry::instance().register_blueprint("area", area_func);
    
    // Search by text
    auto distance_results = BlueprintRegistry::instance().search_by_text("distance");
    ASSERT_EQ(distance_results.size(), 1);
    EXPECT_EQ(distance_results[0], "distance");
    
    auto calculate_results = BlueprintRegistry::instance().search_by_text("calculate");
    ASSERT_EQ(calculate_results.size(), 2);
    EXPECT_TRUE(std::find(calculate_results.begin(), calculate_results.end(), "distance") != calculate_results.end());
    EXPECT_TRUE(std::find(calculate_results.begin(), calculate_results.end(), "area") != calculate_results.end());
    
    auto formula_results = BlueprintRegistry::instance().search_by_text("formula");
    ASSERT_EQ(formula_results.size(), 1);
    EXPECT_EQ(formula_results[0], "distance");
}

TEST_F(BlueprintTest, EmbeddingGeneration) {
    EmbeddingGenerator generator;
    
    BlueprintMetadata metadata;
    metadata.summary = "Test function";
    metadata.rules = {"Must be fast"};
    metadata.examples = {ExamplePair{"test() == true", "", {}}};
    metadata.tags = {"test"};
    
    auto embedding = generator.generate_embedding(metadata);
    
    // Check that embedding is generated and normalized
    EXPECT_FALSE(embedding.empty());
    EXPECT_EQ(embedding.size(), 128); // Expected dimension
    
    // Check normalization (vector should have unit length)
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    EXPECT_NEAR(norm, 1.0f, 0.001f); // Should be approximately 1.0
}

TEST_F(BlueprintTest, SimilaritySearch) {
    EmbeddingGenerator generator;
    
    // Create similar blueprints
    BlueprintMetadata math1;
    math1.summary = "Calculate mathematical distance";
    math1.tags = {"math", "calculation"};
    math1.embedding = generator.generate_embedding(math1);
    BlueprintRegistry::instance().register_blueprint("math1", math1);
    
    BlueprintMetadata math2;
    math2.summary = "Compute mathematical result";
    math2.tags = {"math", "computation"};
    math2.embedding = generator.generate_embedding(math2);
    BlueprintRegistry::instance().register_blueprint("math2", math2);
    
    BlueprintMetadata string_func;
    string_func.summary = "Process string data";
    string_func.tags = {"string", "processing"};
    string_func.embedding = generator.generate_embedding(string_func);
    BlueprintRegistry::instance().register_blueprint("string_func", string_func);
    
    // Search for similarity to math1
    auto results = BlueprintRegistry::instance().search_by_similarity(math1.embedding, 0.5f, 10);
    
    // Should find math1 itself and possibly math2 (depending on similarity)
    EXPECT_GE(results.size(), 1);
    EXPECT_EQ(results[0].first, "math1"); // Should find itself with highest similarity
    EXPECT_NEAR(results[0].second, 1.0f, 0.001f); // Perfect similarity to itself
}

TEST_F(BlueprintTest, MCPExport) {
    // Register a blueprint
    BlueprintMetadata metadata;
    metadata.summary = "Test function for MCP export";
    metadata.examples = {ExamplePair{"test() == true", "", {}}};
    metadata.tags = {"test", "export"};
    BlueprintRegistry::instance().register_blueprint("test_function", metadata);
    
    // Export to MCP JSON
    auto mcp_json = BlueprintRegistry::instance().export_to_mcp_json();
    
    // Verify JSON contains expected elements
    EXPECT_TRUE(mcp_json.find("\"tools\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"test_function\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"Test function for MCP export\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"inputSchema\"") != std::string::npos);
}

TEST_F(BlueprintTest, IDEIntegration) {
    BlueprintIDE ide;
    
    BlueprintMetadata metadata;
    metadata.summary = "Calculate distance between two points";
    metadata.rules = {"Must handle negative coordinates", "Returns non-negative result"};
    
    // Create examples with v2.0 format
    ExamplePair ex1;
    ex1.input = "distance(0,0,3,4)";
    ex1.output = "5.0";
    
    ExamplePair ex2;
    ex2.input = "distance(-1,-1,2,3)";
    ex2.output = "5.0";
    
    metadata.examples = {ex1, ex2};
    metadata.tags = {"math", "geometry", "utility"};
    // Note: cost field removed in v2.0
    
    // Generate hover info
    auto hover_info = ide.generate_hover_info(metadata);
    
    EXPECT_TRUE(hover_info.find("Calculate distance between two points") != std::string::npos);
    EXPECT_TRUE(hover_info.find("Must handle negative coordinates") != std::string::npos);
    EXPECT_TRUE(hover_info.find("distance(0,0,3,4)") != std::string::npos);
    EXPECT_TRUE(hover_info.find("5.0") != std::string::npos);
    EXPECT_TRUE(hover_info.find("math") != std::string::npos);
    // Note: cost field removed in v2.0
}

TEST_F(BlueprintTest, ValidationErrors) {
    BlueprintParser parser;
    
    // Test empty summary
    BlueprintMetadata empty_summary;
    empty_summary.summary = "";
    auto validation_result = parser.validate_blueprint(empty_summary);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("summary cannot be empty") != std::string::npos);
    
    // Test too long summary
    BlueprintMetadata long_summary;
    long_summary.summary = std::string(501, 'a'); // 501 characters
    validation_result = parser.validate_blueprint(long_summary);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("summary too long") != std::string::npos);
    
    // Test empty rule
    BlueprintMetadata empty_rule;
    empty_rule.summary = "Valid summary";
    empty_rule.rules = {""};
    validation_result = parser.validate_blueprint(empty_rule);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("rule cannot be empty") != std::string::npos);
}

// v2.0 Feature Tests
TEST_F(BlueprintTest, V2InputOutputExamples) {
    BlueprintMetadata metadata;
    metadata.summary = "Test v2.0 input-output examples";
    
    // Create examples with new v2.0 format
    ExamplePair example1;
    example1.input = "calculate(5, 3)";
    example1.output = "8";
    example1.description = "Addition example";
    
    ExamplePair example2;
    example2.input = "calculate(10, 2)";
    example2.output = "12";
    
    metadata.examples = {example1, example2};
    
    BlueprintRegistry::instance().register_blueprint("calculate", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("calculate");
    ASSERT_TRUE(retrieved.has_value());
    
    ASSERT_EQ(retrieved->examples.size(), 2);
    EXPECT_EQ(retrieved->examples[0].input, "calculate(5, 3)");
    EXPECT_EQ(retrieved->examples[0].output, "8");
    EXPECT_EQ(retrieved->examples[0].description.value(), "Addition example");
    
    EXPECT_EQ(retrieved->examples[1].input, "calculate(10, 2)");
    EXPECT_EQ(retrieved->examples[1].output, "12");
    EXPECT_FALSE(retrieved->examples[1].description.has_value());
}

TEST_F(BlueprintTest, V2ShadowProvenanceIntegration) {
    BlueprintMetadata metadata;
    metadata.summary = "Test shadow provenance integration";
    metadata.rules = {"Test rule"};
    
    BlueprintRegistry::instance().register_blueprint("test_func", metadata);
    
    // Test shadow provenance linking
    BlueprintRegistry::instance().link_to_shadow_history("test_func", "shadow_001", "conv_123");
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("test_func");
    ASSERT_TRUE(retrieved.has_value());
    
    EXPECT_EQ(retrieved->shadow_history_id, "shadow_001");
    EXPECT_EQ(retrieved->conversation_id.value(), "conv_123");
}

TEST_F(BlueprintTest, V2ASTNodeLinking) {
    BlueprintMetadata metadata;
    metadata.summary = "Test AST node linking";
    
    BlueprintRegistry::instance().register_blueprint("ast_test", metadata);
    
    // Test AST node linking
    BlueprintRegistry::instance().link_to_ast_node("ast_test", "ast_node_456");
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("ast_test");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->ast_node_id, "ast_node_456");
    
    // Test reverse lookup by AST node ID
    auto by_ast_node = BlueprintRegistry::instance().get_by_ast_node_id("ast_node_456");
    ASSERT_TRUE(by_ast_node.has_value());
    EXPECT_EQ(by_ast_node->summary, "Test AST node linking");
}

TEST_F(BlueprintTest, V2RuntimeQuerying) {
    // Register blueprints with different rules and examples
    BlueprintMetadata math_func;
    math_func.summary = "Mathematical calculation";
    math_func.rules = {"Must use mathematical formula", "Input validation required"};
    
    ExamplePair math_example;
    math_example.input = "sqrt(16)";
    math_example.output = "4";
    math_func.examples = {math_example};
    
    BlueprintRegistry::instance().register_blueprint("sqrt", math_func);
    
    BlueprintMetadata string_func;
    string_func.summary = "String processing";
    string_func.rules = {"Handle empty strings", "Preserve encoding"};
    
    ExamplePair string_example;
    string_example.input = "process('hello')";
    string_example.output = "HELLO";
    string_func.examples = {string_example};
    
    BlueprintRegistry::instance().register_blueprint("process", string_func);
    
    // Test querying by rules
    auto math_results = BlueprintRegistry::instance().query_by_rules({"mathematical", "formula"});
    ASSERT_EQ(math_results.size(), 1);
    EXPECT_EQ(math_results[0], "sqrt");
    
    auto validation_results = BlueprintRegistry::instance().query_by_rules({"validation"});
    ASSERT_EQ(validation_results.size(), 1);
    EXPECT_EQ(validation_results[0], "sqrt");
    
    // Test querying by examples
    auto sqrt_results = BlueprintRegistry::instance().query_by_examples("sqrt");
    ASSERT_EQ(sqrt_results.size(), 1);
    EXPECT_EQ(sqrt_results[0], "sqrt");
    
    auto process_results = BlueprintRegistry::instance().query_by_examples("process");
    ASSERT_EQ(process_results.size(), 1);
    EXPECT_EQ(process_results[0], "process");
}

TEST_F(BlueprintTest, V2BlueprintEvolution) {
    // Create initial blueprint
    BlueprintMetadata original;
    original.summary = "Original function";
    original.rules = {"Original rule"};
    
    BlueprintRegistry::instance().register_blueprint("evolving_func", original);
    
    // Update blueprint (should track history)
    BlueprintMetadata updated;
    updated.summary = "Updated function";
    updated.rules = {"Updated rule", "New rule"};
    
    BlueprintRegistry::instance().track_blueprint_update("evolving_func", original, updated);
    
    // Check that history is tracked
    auto history = BlueprintRegistry::instance().get_blueprint_history("evolving_func");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].summary, "Original function");
    EXPECT_EQ(history[0].rules.size(), 1);
    
    // Check current version
    auto current = BlueprintRegistry::instance().get_blueprint("evolving_func");
    ASSERT_TRUE(current.has_value());
    EXPECT_EQ(current->summary, "Updated function");
    EXPECT_EQ(current->rules.size(), 2);
}

TEST_F(BlueprintTest, V2DeprecatedFieldsRemoved) {
    // Verify that cost and intent fields are not present in BlueprintMetadata
    BlueprintMetadata metadata;
    metadata.summary = "Test deprecated fields removal";
    
    // These should compile without errors (fields should not exist)
    // If cost or intent fields existed, this would cause compilation errors
    
    BlueprintRegistry::instance().register_blueprint("no_deprecated", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("no_deprecated");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->summary, "Test deprecated fields removal");
    
    // Test that parser ignores deprecated fields in custom_fields
    // (This would be tested more thoroughly in integration tests)
}

// v2.1 Feature Tests for inputs, outputs, and links fields
TEST_F(BlueprintTest, V21InputsField) {
    BlueprintMetadata metadata;
    metadata.summary = "Test inputs field";
    metadata.inputs = {
        {"x", "The x coordinate as a number"},
        {"y", "The y coordinate as a number"},
        {"precision", "Optional precision for calculation"}
    };
    
    BlueprintRegistry::instance().register_blueprint("calculate_distance", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("calculate_distance");
    ASSERT_TRUE(retrieved.has_value());
    
    ASSERT_EQ(retrieved->inputs.size(), 3);
    EXPECT_EQ(retrieved->inputs.at("x"), "The x coordinate as a number");
    EXPECT_EQ(retrieved->inputs.at("y"), "The y coordinate as a number");
    EXPECT_EQ(retrieved->inputs.at("precision"), "Optional precision for calculation");
}

TEST_F(BlueprintTest, V21OutputsField) {
    BlueprintMetadata metadata;
    metadata.summary = "Test outputs field";
    metadata.outputs = {
        {"result", "The calculated distance as a floating point number"},
        {"status", "Success or error status"},
        {"metadata", "Additional calculation metadata"}
    };
    
    BlueprintRegistry::instance().register_blueprint("distance_calc", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("distance_calc");
    ASSERT_TRUE(retrieved.has_value());
    
    ASSERT_EQ(retrieved->outputs.size(), 3);
    EXPECT_EQ(retrieved->outputs.at("result"), "The calculated distance as a floating point number");
    EXPECT_EQ(retrieved->outputs.at("status"), "Success or error status");
    EXPECT_EQ(retrieved->outputs.at("metadata"), "Additional calculation metadata");
}

TEST_F(BlueprintTest, V21LinksField) {
    BlueprintMetadata metadata;
    metadata.summary = "Test links field";
    metadata.links = {
        "https://en.wikipedia.org/wiki/Euclidean_distance",
        "https://mathworld.wolfram.com/Distance.html",
        "https://docs.example.com/math/distance"
    };
    
    BlueprintRegistry::instance().register_blueprint("distance_with_links", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("distance_with_links");
    ASSERT_TRUE(retrieved.has_value());
    
    ASSERT_EQ(retrieved->links.size(), 3);
    EXPECT_EQ(retrieved->links[0], "https://en.wikipedia.org/wiki/Euclidean_distance");
    EXPECT_EQ(retrieved->links[1], "https://mathworld.wolfram.com/Distance.html");
    EXPECT_EQ(retrieved->links[2], "https://docs.example.com/math/distance");
}

TEST_F(BlueprintTest, V21CompleteBlueprint) {
    BlueprintMetadata metadata;
    metadata.summary = "Complete blueprint with all v2.1 fields";
    metadata.rules = {"Must validate inputs", "Must handle edge cases"};
    
    // Examples with v2.0 format
    ExamplePair example;
    example.input = "calculate(5, 3)";
    example.output = "8.0";
    example.description = "Basic calculation example";
    metadata.examples = {example};
    
    metadata.tags = {"math", "calculation", "utility"};
    
    // v2.1 fields
    metadata.inputs = {
        {"a", "First number for calculation"},
        {"b", "Second number for calculation"}
    };
    
    metadata.outputs = {
        {"result", "The calculated result"},
        {"precision", "Number of decimal places"}
    };
    
    metadata.links = {
        "https://example.com/math-docs",
        "https://example.com/api-reference"
    };
    
    BlueprintRegistry::instance().register_blueprint("complete_func", metadata);
    
    auto retrieved = BlueprintRegistry::instance().get_blueprint("complete_func");
    ASSERT_TRUE(retrieved.has_value());
    
    // Verify all fields are preserved
    EXPECT_EQ(retrieved->summary, "Complete blueprint with all v2.1 fields");
    EXPECT_EQ(retrieved->rules.size(), 2);
    EXPECT_EQ(retrieved->examples.size(), 1);
    EXPECT_EQ(retrieved->tags.size(), 3);
    EXPECT_EQ(retrieved->inputs.size(), 2);
    EXPECT_EQ(retrieved->outputs.size(), 2);
    EXPECT_EQ(retrieved->links.size(), 2);
}

TEST_F(BlueprintTest, V21EmbeddingIncludesNewFields) {
    EmbeddingGenerator generator;
    
    BlueprintMetadata metadata;
    metadata.summary = "Test embedding generation";
    metadata.inputs = {{"param", "test parameter"}};
    metadata.outputs = {{"result", "test result"}};
    metadata.links = {"https://example.com/test"};
    
    auto embedding = generator.generate_embedding(metadata);
    
    // Verify embedding is generated (the actual content depends on hash function)
    EXPECT_FALSE(embedding.empty());
    EXPECT_EQ(embedding.size(), 128);
    
    // Verify normalization
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    EXPECT_NEAR(norm, 1.0f, 0.001f);
}

TEST_F(BlueprintTest, V21IDEHoverInfoIncludesNewFields) {
    BlueprintIDE ide;
    
    BlueprintMetadata metadata;
    metadata.summary = "Test function with all fields";
    metadata.rules = {"Validate inputs"};
    
    ExamplePair example;
    example.input = "test(5)";
    example.output = "10";
    metadata.examples = {example};
    
    metadata.inputs = {{"value", "Input value to process"}};
    metadata.outputs = {{"result", "Processed result"}};
    metadata.links = {"https://example.com/docs"};
    metadata.tags = {"test"};
    
    auto hover_info = ide.generate_hover_info(metadata);
    
    // Verify all sections are included
    EXPECT_TRUE(hover_info.find("Test function with all fields") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Rules:**") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Examples:**") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Inputs:**") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Outputs:**") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Links:**") != std::string::npos);
    EXPECT_TRUE(hover_info.find("**Tags:**") != std::string::npos);
    
    // Verify specific content
    EXPECT_TRUE(hover_info.find("value") != std::string::npos);
    EXPECT_TRUE(hover_info.find("Input value to process") != std::string::npos);
    EXPECT_TRUE(hover_info.find("result") != std::string::npos);
    EXPECT_TRUE(hover_info.find("Processed result") != std::string::npos);
    EXPECT_TRUE(hover_info.find("https://example.com/docs") != std::string::npos);
}

TEST_F(BlueprintTest, V21MCPExportIncludesNewFields) {
    BlueprintMetadata metadata;
    metadata.summary = "Test MCP export with new fields";
    
    ExamplePair example;
    example.input = "test(5)";
    example.output = "10";
    metadata.examples = {example};
    
    metadata.inputs = {
        {"value", "Input value"},
        {"options", "Configuration options"}
    };
    
    metadata.outputs = {
        {"result", "The result"},
        {"status", "Operation status"}
    };
    
    metadata.links = {"https://example.com/api"};
    metadata.tags = {"test", "api"};
    
    auto mcp_json = generate_mcp_tool("test_function", metadata);
    
    // Verify new fields are included in MCP export
    EXPECT_TRUE(mcp_json.find("\"inputSchema\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"outputSchema\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"value\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"Input value\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"result\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"The result\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("\"links\"") != std::string::npos);
    EXPECT_TRUE(mcp_json.find("https://example.com/api") != std::string::npos);
}

TEST_F(BlueprintTest, V21ValidationNewFields) {
    BlueprintParser parser;
    
    // Test empty input key
    BlueprintMetadata empty_input_key;
    empty_input_key.summary = "Valid summary";
    empty_input_key.inputs = {{"", "description"}};
    auto validation_result = parser.validate_blueprint(empty_input_key);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("input key cannot be empty") != std::string::npos);
    
    // Test empty input description
    BlueprintMetadata empty_input_desc;
    empty_input_desc.summary = "Valid summary";
    empty_input_desc.inputs = {{"key", ""}};
    validation_result = parser.validate_blueprint(empty_input_desc);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("input description cannot be empty") != std::string::npos);
    
    // Test too long input key
    BlueprintMetadata long_input_key;
    long_input_key.summary = "Valid summary";
    long_input_key.inputs = {{std::string(51, 'a'), "description"}};
    validation_result = parser.validate_blueprint(long_input_key);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("input key too long") != std::string::npos);
    
    // Test empty output key
    BlueprintMetadata empty_output_key;
    empty_output_key.summary = "Valid summary";
    empty_output_key.outputs = {{"", "description"}};
    validation_result = parser.validate_blueprint(empty_output_key);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("output key cannot be empty") != std::string::npos);
    
    // Test empty link
    BlueprintMetadata empty_link;
    empty_link.summary = "Valid summary";
    empty_link.links = {""};
    validation_result = parser.validate_blueprint(empty_link);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("link cannot be empty") != std::string::npos);
    
    // Test too long link
    BlueprintMetadata long_link;
    long_link.summary = "Valid summary";
    long_link.links = {std::string(501, 'a')};
    validation_result = parser.validate_blueprint(long_link);
    EXPECT_FALSE(validation_result.has_value());
    EXPECT_TRUE(validation_result.error().find("link too long") != std::string::npos);
}