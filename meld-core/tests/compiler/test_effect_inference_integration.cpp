#include "../../include/meld/compiler/effect_checker.hpp"
#include "../../include/meld/parser/ast.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace meld::compiler;
using namespace meld::parser::ast;

// Integration test for automatic effect inference functionality
// This test validates the core requirements for Task 35.8

class EffectInferenceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<EffectChecker>();
    }
    
    std::unique_ptr<EffectChecker> checker;
};

// Test Requirement 41.6: Analyze function bodies to detect performed effects
TEST_F(EffectInferenceIntegrationTest, AnalyzeFunctionBodyEffects) {
    // Create a simple function definition
    function_definition func;
    func.name.name = "testFunction";
    func.has_effects = false; // No explicit effects declared
    
    // Create a simple block expression as body
    block_expression body_block;
    func.body = boost::spirit::x3::forward_ast<block_expression>(body_block);
    
    // Test the analyze_function_body_effects method
    auto inferred_effects = checker->analyze_function_body_effects(func);
    
    // Should infer EffectPure for a function with no operations
    EXPECT_FALSE(inferred_effects.empty());
    EXPECT_TRUE(inferred_effects.find("EffectPure") != inferred_effects.end());
}

// Test Requirement 41.6: Generate @uses(...) annotations based on inference
TEST_F(EffectInferenceIntegrationTest, GenerateUsesAnnotations) {
    // Test pure function annotation
    std::set<std::string> pure_effects = {"EffectPure"};
    std::string pure_annotation = checker->generate_uses_annotation(pure_effects);
    EXPECT_EQ(pure_annotation, "@uses()");
    
    // Test single effect annotation
    std::set<std::string> io_effects = {"EffectIO"};
    std::string io_annotation = checker->generate_uses_annotation(io_effects);
    EXPECT_EQ(io_annotation, "@uses(EffectIO)");
    
    // Test multiple effects annotation
    std::set<std::string> multiple_effects = {"EffectIO", "EffectNetwork"};
    std::string multiple_annotation = checker->generate_uses_annotation(multiple_effects);
    
    // Should contain both effects (order may vary)
    EXPECT_TRUE(multiple_annotation.find("EffectIO") != std::string::npos);
    EXPECT_TRUE(multiple_annotation.find("EffectNetwork") != std::string::npos);
    EXPECT_TRUE(multiple_annotation.find("@uses(") != std::string::npos);
}

// Test Requirement 41.20, 41.21: Propagate effects through call graph automatically
TEST_F(EffectInferenceIntegrationTest, CallGraphPropagation) {
    // Create multiple functions to test call graph propagation
    std::vector<function_definition> functions;
    
    // Function A: pure function
    function_definition funcA;
    funcA.name.name = "funcA";
    funcA.has_effects = false;
    block_expression bodyA;
    funcA.body = boost::spirit::x3::forward_ast<block_expression>(bodyA);
    functions.push_back(funcA);
    
    // Function B: calls funcA (should remain pure)
    function_definition funcB;
    funcB.name.name = "funcB";
    funcB.has_effects = false;
    block_expression bodyB;
    funcB.body = boost::spirit::x3::forward_ast<block_expression>(bodyB);
    functions.push_back(funcB);
    
    // Function C: performs IO
    function_definition funcC;
    funcC.name.name = "funcC";
    funcC.has_effects = false;
    block_expression bodyC;
    funcC.body = boost::spirit::x3::forward_ast<block_expression>(bodyC);
    functions.push_back(funcC);
    
    // Manually register effects for funcC to simulate IO operations
    checker->register_function_effects("funcC", {"EffectIO"});
    
    // Test batch inference with call graph propagation
    auto inferred_effects_map = checker->infer_effects_for_functions(functions);
    
    EXPECT_EQ(inferred_effects_map.size(), 3);
    
    // Verify that each function has inferred effects
    EXPECT_TRUE(inferred_effects_map.find("funcA") != inferred_effects_map.end());
    EXPECT_TRUE(inferred_effects_map.find("funcB") != inferred_effects_map.end());
    EXPECT_TRUE(inferred_effects_map.find("funcC") != inferred_effects_map.end());
    
    // funcA should be pure
    auto funcA_effects = inferred_effects_map["funcA"];
    EXPECT_TRUE(funcA_effects.find("EffectPure") != funcA_effects.end());
    
    // funcC should have IO effects
    auto funcC_effects = inferred_effects_map["funcC"];
    EXPECT_TRUE(funcC_effects.find("EffectIO") != funcC_effects.end());
}

// Test effect annotation update detection
TEST_F(EffectInferenceIntegrationTest, EffectAnnotationUpdateDetection) {
    // Create function with no declared effects (pure by default)
    function_definition pure_func;
    pure_func.name.name = "pureFunc";
    pure_func.has_effects = false;
    
    std::set<std::string> pure_inferred = {"EffectPure"};
    
    // Should not need update (both pure)
    EXPECT_FALSE(checker->needs_effect_annotation_update(pure_func, pure_inferred));
    
    // Create function with declared IO effect
    function_definition io_func;
    io_func.name.name = "ioFunc";
    io_func.has_effects = true;
    identifier io_effect;
    io_effect.name = "EffectIO";
    io_func.effects_clause.push_back(io_effect);
    
    std::set<std::string> io_inferred = {"EffectIO"};
    
    // Should not need update (both have EffectIO)
    EXPECT_FALSE(checker->needs_effect_annotation_update(io_func, io_inferred));
    
    // Test mismatch case
    std::set<std::string> network_inferred = {"EffectNetwork"};
    
    // Should need update (declared IO but inferred Network)
    EXPECT_TRUE(checker->needs_effect_annotation_update(io_func, network_inferred));
}

// Test function update with inferred effects
TEST_F(EffectInferenceIntegrationTest, UpdateFunctionWithInferredEffects) {
    // Create function with no effects
    function_definition func;
    func.name.name = "testFunc";
    func.has_effects = false;
    
    // Update with IO effects
    std::set<std::string> io_effects = {"EffectIO"};
    auto updated_func = checker->update_function_with_inferred_effects(func, io_effects);
    
    EXPECT_TRUE(updated_func.has_effects);
    EXPECT_EQ(updated_func.effects_clause.size(), 1);
    EXPECT_EQ(updated_func.effects_clause[0].name, "EffectIO");
    
    // Update with pure effects
    std::set<std::string> pure_effects = {"EffectPure"};
    auto pure_updated_func = checker->update_function_with_inferred_effects(func, pure_effects);
    
    EXPECT_FALSE(pure_updated_func.has_effects);
    EXPECT_EQ(pure_updated_func.effects_clause.size(), 0);
}

// Test caching of inferred effects
TEST_F(EffectInferenceIntegrationTest, InferredEffectsCaching) {
    // Create test function
    function_definition func;
    func.name.name = "cachedFunc";
    func.has_effects = false;
    block_expression body;
    func.body = boost::spirit::x3::forward_ast<block_expression>(body);
    
    // First call should compute and cache
    auto effects1 = checker->analyze_function_body_effects(func);
    
    // Second call should use cache
    auto effects2 = checker->analyze_function_body_effects(func);
    
    EXPECT_EQ(effects1, effects2);
    
    // Verify we can retrieve cached effects
    auto cached_effects = checker->get_inferred_effects("cachedFunc");
    EXPECT_EQ(effects1, cached_effects);
}

// Test built-in operation effect detection
TEST_F(EffectInferenceIntegrationTest, BuiltinOperationEffects) {
    // Test that built-in operations are properly registered
    EXPECT_TRUE(checker->operation_requires_effect("File.read", "EffectIO"));
    EXPECT_TRUE(checker->operation_requires_effect("http.get", "EffectNetwork"));
    EXPECT_TRUE(checker->operation_requires_effect("Time.now", "EffectTime"));
    EXPECT_TRUE(checker->operation_requires_effect("Console.print", "EffectIO"));
    
    // Test getting operation effects
    auto file_read_effects = checker->get_operation_effects("File.read");
    EXPECT_TRUE(file_read_effects.find("EffectIO") != file_read_effects.end());
    
    auto http_get_effects = checker->get_operation_effects("http.get");
    EXPECT_TRUE(http_get_effects.find("EffectNetwork") != http_get_effects.end());
}

// Test effect composition and cleanup
TEST_F(EffectInferenceIntegrationTest, EffectComposition) {
    // Test composing pure with other effects
    std::set<std::string> pure_effects = {"EffectPure"};
    std::set<std::string> io_effects = {"EffectIO"};
    
    auto composed = checker->compose_effects(pure_effects, io_effects);
    
    // Should contain only EffectIO (EffectPure removed when other effects present)
    EXPECT_EQ(composed.size(), 1);
    EXPECT_TRUE(composed.find("EffectIO") != composed.end());
    EXPECT_TRUE(composed.find("EffectPure") == composed.end());
    
    // Test composing multiple non-pure effects
    std::set<std::string> network_effects = {"EffectNetwork"};
    auto multi_composed = checker->compose_effects(io_effects, network_effects);
    
    EXPECT_EQ(multi_composed.size(), 2);
    EXPECT_TRUE(multi_composed.find("EffectIO") != multi_composed.end());
    EXPECT_TRUE(multi_composed.find("EffectNetwork") != multi_composed.end());
}

// Test integration with existing effect checking
TEST_F(EffectInferenceIntegrationTest, IntegrationWithExistingEffectChecking) {
    // Create a function with declared effects
    function_definition func;
    func.name.name = "testFunc";
    func.has_effects = true;
    
    identifier io_effect;
    io_effect.name = "EffectIO";
    func.effects_clause.push_back(io_effect);
    
    // Create a simple block expression as body
    block_expression body_block;
    func.body = boost::spirit::x3::forward_ast<block_expression>(body_block);
    
    // Test the existing check_function method with automatic inference
    auto result = checker->check_function(func);
    
    // Should be valid since we're not performing any operations that require effects
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.declared_effects.find("EffectIO") != result.declared_effects.end());
    
    // Should have warnings about unused declared effects
    EXPECT_FALSE(result.warnings.empty());
}

