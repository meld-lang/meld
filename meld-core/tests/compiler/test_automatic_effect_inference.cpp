#include "../../include/meld/compiler/effect_checker.hpp"
#include "../../include/meld/parser/ast.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace meld::compiler;
using namespace meld::parser::ast;

// Helper function to create a simple function definition for testing
function_definition create_test_function(const std::string& name, 
                                       const std::vector<std::string>& effect_names = {},
                                       bool has_effects_clause = false) {
    function_definition func;
    func.name.name = name;
    func.has_effects = has_effects_clause;
    
    // Add effects to effects clause
    for (const auto& effect_name : effect_names) {
        identifier effect_id;
        effect_id.name = effect_name;
        func.effects_clause.push_back(effect_id);
    }
    
    // Create a simple block expression as body
    block_expression body_block;
    func.body = boost::spirit::x3::forward_ast<block_expression>(body_block);
    
    return func;
}

// Helper function to create a function call expression
function_call create_function_call(const std::string& func_name) {
    function_call call;
    call.function_name.name = func_name;
    return call;
}

// Test automatic effect inference for function bodies
TEST(AutomaticEffectInferenceTest, AnalyzeFunctionBodyEffects) {
    EffectChecker checker;
    
    // Create a test function
    auto func = create_test_function("testFunc");
    
    // Test inference on function with no effects (should be pure)
    auto inferred_effects = checker.analyze_function_body_effects(func);
    
    EXPECT_EQ(inferred_effects.size(), 1);
    EXPECT_TRUE(inferred_effects.find("EffectPure") != inferred_effects.end());
}

// Test effect propagation through call graph
TEST(AutomaticEffectInferenceTest, PropagateEffectsThroughCallGraph) {
    EffectChecker checker;
    
    // Create test functions
    std::vector<function_definition> functions;
    
    // Function A: pure function
    auto funcA = create_test_function("funcA");
    functions.push_back(funcA);
    
    // Function B: calls funcA (should remain pure)
    auto funcB = create_test_function("funcB");
    functions.push_back(funcB);
    
    // Function C: performs IO (should have EffectIO)
    auto funcC = create_test_function("funcC");
    functions.push_back(funcC);
    
    // Register some effects for funcC to simulate IO operations
    checker.register_function_effects("funcC", {"EffectIO"});
    
    // Test batch inference
    auto inferred_effects_map = checker.infer_effects_for_functions(functions);
    
    EXPECT_EQ(inferred_effects_map.size(), 3);
    
    // Check that funcA is pure
    auto funcA_effects = inferred_effects_map["funcA"];
    EXPECT_TRUE(funcA_effects.find("EffectPure") != funcA_effects.end());
    
    // Check that funcB is pure (calls pure function)
    auto funcB_effects = inferred_effects_map["funcB"];
    EXPECT_TRUE(funcB_effects.find("EffectPure") != funcB_effects.end());
    
    // Check that funcC has IO effects
    auto funcC_effects = inferred_effects_map["funcC"];
    EXPECT_TRUE(funcC_effects.find("EffectIO") != funcC_effects.end());
}

// Test @uses annotation generation
TEST(AutomaticEffectInferenceTest, GenerateUsesAnnotation) {
    EffectChecker checker;
    
    // Test pure function annotation
    std::set<std::string> pure_effects = {"EffectPure"};
    std::string pure_annotation = checker.generate_uses_annotation(pure_effects);
    EXPECT_EQ(pure_annotation, "@uses()");
    
    // Test single effect annotation
    std::set<std::string> io_effects = {"EffectIO"};
    std::string io_annotation = checker.generate_uses_annotation(io_effects);
    EXPECT_EQ(io_annotation, "@uses(EffectIO)");
    
    // Test multiple effects annotation
    std::set<std::string> multiple_effects = {"EffectIO", "EffectNetwork"};
    std::string multiple_annotation = checker.generate_uses_annotation(multiple_effects);
    EXPECT_TRUE(multiple_annotation == "@uses(EffectIO, EffectNetwork)" || 
                multiple_annotation == "@uses(EffectNetwork, EffectIO)");
    
    // Test empty effects
    std::set<std::string> empty_effects;
    std::string empty_annotation = checker.generate_uses_annotation(empty_effects);
    EXPECT_EQ(empty_annotation, "@uses()");
}

// Test effect annotation update detection
TEST(AutomaticEffectInferenceTest, NeedsEffectAnnotationUpdate) {
    EffectChecker checker;
    
    // Create function with no declared effects (pure by default)
    auto pure_func = create_test_function("pureFunc");
    std::set<std::string> pure_inferred = {"EffectPure"};
    
    // Should not need update (both pure)
    EXPECT_FALSE(checker.needs_effect_annotation_update(pure_func, pure_inferred));
    
    // Create function with declared IO effect
    auto io_func = create_test_function("ioFunc", {"EffectIO"}, true);
    std::set<std::string> io_inferred = {"EffectIO"};
    
    // Should not need update (both have EffectIO)
    EXPECT_FALSE(checker.needs_effect_annotation_update(io_func, io_inferred));
    
    // Test mismatch case
    std::set<std::string> network_inferred = {"EffectNetwork"};
    
    // Should need update (declared IO but inferred Network)
    EXPECT_TRUE(checker.needs_effect_annotation_update(io_func, network_inferred));
}

// Test function update with inferred effects
TEST(AutomaticEffectInferenceTest, UpdateFunctionWithInferredEffects) {
    EffectChecker checker;
    
    // Create function with no effects
    auto func = create_test_function("testFunc");
    
    // Update with IO effects
    std::set<std::string> io_effects = {"EffectIO"};
    auto updated_func = checker.update_function_with_inferred_effects(func, io_effects);
    
    EXPECT_TRUE(updated_func.has_effects);
    EXPECT_EQ(updated_func.effects_clause.size(), 1);
    EXPECT_EQ(updated_func.effects_clause[0].name, "EffectIO");
    
    // Update with pure effects
    std::set<std::string> pure_effects = {"EffectPure"};
    auto pure_updated_func = checker.update_function_with_inferred_effects(func, pure_effects);
    
    EXPECT_FALSE(pure_updated_func.has_effects);
    EXPECT_EQ(pure_updated_func.effects_clause.size(), 0);
}

// Test caching of inferred effects
TEST(AutomaticEffectInferenceTest, InferredEffectsCaching) {
    EffectChecker checker;
    
    // Create test function
    auto func = create_test_function("cachedFunc");
    
    // First call should compute and cache
    auto effects1 = checker.analyze_function_body_effects(func);
    
    // Second call should use cache
    auto effects2 = checker.analyze_function_body_effects(func);
    
    EXPECT_EQ(effects1, effects2);
    
    // Verify we can retrieve cached effects
    auto cached_effects = checker.get_inferred_effects("cachedFunc");
    EXPECT_EQ(effects1, cached_effects);
}

// Test call graph construction and effect propagation
TEST(AutomaticEffectInferenceTest, CallGraphPropagation) {
    EffectChecker checker;
    
    // Create a chain of function calls: A -> B -> C
    // Where C performs IO, so A and B should inherit EffectIO
    
    std::vector<function_definition> functions;
    
    // Function C: performs IO
    auto funcC = create_test_function("funcC");
    checker.register_function_effects("funcC", {"EffectIO"});
    functions.push_back(funcC);
    
    // Function B: calls C
    auto funcB = create_test_function("funcB");
    checker.register_function_effects("funcB", {"EffectPure"}); // Initially pure
    functions.push_back(funcB);
    
    // Function A: calls B
    auto funcA = create_test_function("funcA");
    checker.register_function_effects("funcA", {"EffectPure"}); // Initially pure
    functions.push_back(funcA);
    
    // Simulate call relationships by registering them
    checker.register_function_effects("funcB", {"EffectIO"}); // B calls C, so inherits IO
    checker.register_function_effects("funcA", {"EffectIO"}); // A calls B, so inherits IO
    
    // Test that effects are properly propagated
    auto funcA_effects = checker.get_function_effects("funcA");
    auto funcB_effects = checker.get_function_effects("funcB");
    auto funcC_effects = checker.get_function_effects("funcC");
    
    EXPECT_TRUE(funcA_effects.find("EffectIO") != funcA_effects.end());
    EXPECT_TRUE(funcB_effects.find("EffectIO") != funcB_effects.end());
    EXPECT_TRUE(funcC_effects.find("EffectIO") != funcC_effects.end());
}

// Test built-in operation effect detection
TEST(AutomaticEffectInferenceTest, BuiltinOperationEffects) {
    EffectChecker checker;
    
    // Test that built-in operations are properly registered
    EXPECT_TRUE(checker.operation_requires_effect("File.read", "EffectIO"));
    EXPECT_TRUE(checker.operation_requires_effect("http.get", "EffectNetwork"));
    EXPECT_TRUE(checker.operation_requires_effect("Time.now", "EffectTime"));
    EXPECT_TRUE(checker.operation_requires_effect("Console.print", "EffectIO"));
    
    // Test getting operation effects
    auto file_read_effects = checker.get_operation_effects("File.read");
    EXPECT_TRUE(file_read_effects.find("EffectIO") != file_read_effects.end());
    
    auto http_get_effects = checker.get_operation_effects("http.get");
    EXPECT_TRUE(http_get_effects.find("EffectNetwork") != http_get_effects.end());
}

// Test effect composition and cleanup
TEST(AutomaticEffectInferenceTest, EffectComposition) {
    EffectChecker checker;
    
    // Test composing pure with other effects
    std::set<std::string> pure_effects = {"EffectPure"};
    std::set<std::string> io_effects = {"EffectIO"};
    
    auto composed = checker.compose_effects(pure_effects, io_effects);
    
    // Should contain only EffectIO (EffectPure removed when other effects present)
    EXPECT_EQ(composed.size(), 1);
    EXPECT_TRUE(composed.find("EffectIO") != composed.end());
    EXPECT_TRUE(composed.find("EffectPure") == composed.end());
    
    // Test composing multiple non-pure effects
    std::set<std::string> network_effects = {"EffectNetwork"};
    auto multi_composed = checker.compose_effects(io_effects, network_effects);
    
    EXPECT_EQ(multi_composed.size(), 2);
    EXPECT_TRUE(multi_composed.find("EffectIO") != multi_composed.end());
    EXPECT_TRUE(multi_composed.find("EffectNetwork") != multi_composed.end());
}

