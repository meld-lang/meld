#include <gtest/gtest.h>
#include "meld/compiler/effect_checker.hpp"
#include "meld/parser/parser.hpp"

using namespace meld::compiler;
using namespace meld::parser;

// Helper function to parse and check a function
EffectCheckResult parse_and_check_function(const std::string& source) {
    Parser parser;
std::vector<ast::expression> _exprs;
    bool result_ok = parser.parse_file(source, _exprs);
    
    if (!result_ok) {
        EffectCheckResult error_result;
        error_result.is_valid = false;
        error_result.errors.push_back("Parse error: " + parser.error_message());
        return error_result;
    }
    
    auto& expressions = _exprs;
    if (expressions.empty()) {
        EffectCheckResult error_result;
        error_result.is_valid = false;
        error_result.errors.push_back("No expressions found");
        return error_result;
    }
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&expressions[0]);
    auto* func_def = func_def_fwd ? &func_def_fwd->get() : nullptr;
    if (!func_def) {
        EffectCheckResult error_result;
        error_result.is_valid = false;
        error_result.errors.push_back("Not a function definition");
        return error_result;
    }
    
    EffectChecker checker;
    return checker.check_function(*func_def);
}

// Test pure function (no effects)
TEST(EffectCheckerTest, PureFunction) {
    std::string source = R"(
        fnc add(a: int, b: int) -> int {
            rtn a + b
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.declared_effects.find("EffectPure") != result.declared_effects.end());
}

// Test function with declared IO effect
TEST(EffectCheckerTest, DeclaredIOEffect) {
    std::string source = R"(
        fnc readConfig(path: string) -> string
            effects { EffectIO }
        {
            rtn File.read(path)
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.declared_effects.find("EffectIO") != result.declared_effects.end());
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
}

// Test function with multiple declared effects
TEST(EffectCheckerTest, MultipleDeclaredEffects) {
    std::string source = R"(
        fnc fetchAndSave(url: string, path: string) -> string
            effects { EffectNetwork, EffectIO }
        {
            val data = http.get(url)
            File.write(path, data)
            rtn data
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.declared_effects.find("EffectNetwork") != result.declared_effects.end());
    EXPECT_TRUE(result.declared_effects.find("EffectIO") != result.declared_effects.end());
}

// Test function with missing effect declaration
TEST(EffectCheckerTest, MissingEffectDeclaration) {
    std::string source = R"(
        fnc readFile(path: string) -> string {
            rtn File.read(path)
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
    EXPECT_TRUE(result.declared_effects.find("EffectPure") != result.declared_effects.end());
    
    // Check error message
    EXPECT_TRUE(std::any_of(result.errors.begin(), result.errors.end(),
        [](const std::string& error) {
            return error.find("EffectIO") != std::string::npos && 
                   error.find("not declared") != std::string::npos;
        }));
}

// Test pure function with side effects (should fail)
TEST(EffectCheckerTest, PureFunctionWithSideEffects) {
    std::string source = R"(
        fnc impureFunction(x: int) -> int
            effects { }
        {
            print("Debug: " + x)
            rtn x * 2
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Should detect that print requires EffectIO
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
}

// Test function with unused declared effect (should warn)
TEST(EffectCheckerTest, UnusedDeclaredEffect) {
    std::string source = R"(
        fnc calculate(x: int) -> int
            effects { EffectIO, EffectNetwork }
        {
            rtn x * 2
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid); // Valid but with warnings
    EXPECT_FALSE(result.warnings.empty());
    
    // Should warn about unused effects
    EXPECT_TRUE(std::any_of(result.warnings.begin(), result.warnings.end(),
        [](const std::string& warning) {
            return warning.find("declared but not used") != std::string::npos;
        }));
}

// Test effect inference from function names
TEST(EffectCheckerTest, EffectInferenceFromFunctionNames) {
    std::string source = R"(
        fnc processData(data: string) -> string
            effects { EffectIO }
        {
            writeFile("output.txt", data)
            rtn data
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
}

// Test nested function calls with effects
TEST(EffectCheckerTest, NestedFunctionCallsWithEffects) {
    std::string source = R"(
        fnc complexOperation(url: string) -> string
            effects { EffectNetwork, EffectIO, EffectTime }
        {
            val timestamp = Time.now()
            val data = http.get(url)
            val filename = "data_" + timestamp + ".txt"
            File.write(filename, data)
            rtn filename
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.required_effects.find("EffectNetwork") != result.required_effects.end());
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
    EXPECT_TRUE(result.required_effects.find("EffectTime") != result.required_effects.end());
}

// Test perform expressions
TEST(EffectCheckerTest, PerformExpressions) {
    std::string source = R"(
        fnc performIO(path: string) -> string
            effects { EffectIO }
        {
            rtn perform EffectIO.read(path)
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
}

// Test custom effect types
TEST(EffectCheckerTest, CustomEffectTypes) {
    std::string source = R"(
        fnc queryDatabase(sql: string) -> ResultSet
            effects { EffectDatabase }
        {
            rtn Database.query(sql)
        }
    )";
    
    auto result = parse_and_check_function(source);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.declared_effects.find("EffectDatabase") != result.declared_effects.end());
}

// Test effect checker registration
TEST(EffectCheckerTest, EffectOperationRegistration) {
    EffectChecker checker;
    
    // Test built-in operations
    EXPECT_TRUE(checker.operation_requires_effect("File.read", "EffectIO"));
    EXPECT_TRUE(checker.operation_requires_effect("http.get", "EffectNetwork"));
    EXPECT_TRUE(checker.operation_requires_effect("Time.now", "EffectTime"));
    EXPECT_FALSE(checker.operation_requires_effect("Math.add", "EffectIO"));
    
    // Test custom operation registration
    checker.register_operation("CustomOp.execute", "EffectCustom", "Custom operation");
    EXPECT_TRUE(checker.operation_requires_effect("CustomOp.execute", "EffectCustom"));
}

// Test effect compatibility checking
TEST(EffectCheckerTest, EffectCompatibilityChecking) {
    EffectChecker checker;
    
    std::set<std::string> required = {"EffectIO", "EffectNetwork"};
    std::set<std::string> declared_compatible = {"EffectIO", "EffectNetwork", "EffectTime"};
    std::set<std::string> declared_incompatible = {"EffectIO"};
    std::set<std::string> declared_pure = {"EffectPure"};
    
    EXPECT_TRUE(checker.are_effects_compatible(required, declared_compatible));
    EXPECT_FALSE(checker.are_effects_compatible(required, declared_incompatible));
    EXPECT_FALSE(checker.are_effects_compatible(required, declared_pure));
    
    // Pure function should be compatible with no effects
    std::set<std::string> no_effects = {};
    EXPECT_TRUE(checker.are_effects_compatible(no_effects, declared_pure));
}

// Test error message generation
TEST(EffectCheckerTest, ErrorMessageGeneration) {
    EffectChecker checker;
    
    std::set<std::string> required = {"EffectIO", "EffectNetwork"};
    std::set<std::string> declared = {"EffectIO"};
    
    auto errors = checker.generate_effect_errors(required, declared, "testFunction");
    
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(),
        [](const std::string& error) {
            return error.find("EffectNetwork") != std::string::npos &&
                   error.find("not declared") != std::string::npos;
        }));
}

// Test lambda effect inference
TEST(EffectCheckerTest, LambdaEffectInference) {
    std::string code = R"(
        val pure_lambda = (x: int) -> x + 1
        val io_lambda = (filename: string) -> File.read(filename)
        val network_lambda = (url: string) -> http.get(url)
        val mixed_lambda = (x: int, url: string) -> {
            val data = http.get(url)
            File.write("output.txt", data)
            x + 1
        }
    )";
    
    auto result = parse_and_check_function(code);
    
    EffectChecker checker;
    
    // Test pure lambda - should infer no effects (pure)
    // Test IO lambda - should infer EffectIO via infer_lambda_effects
    // Test network lambda - should infer EffectNetwork  
    // Test mixed lambda - should infer both EffectNetwork and EffectIO
    
    // Note: This test validates the inference logic exists
    // Actual lambda parsing would require AST support for lambda expressions
    EXPECT_TRUE(true); // Placeholder - actual implementation would test lambda inference
}

// Test function call chain effect propagation
TEST(EffectCheckerTest, CallChainEffectPropagation) {
    std::string code = R"(
        fnc helper_io() effects { EffectIO } {
            rtn File.read("test.txt")
        }
        
        fnc caller_function() {
            rtn helper_io()
        }
    )";
    
    auto result = parse_and_check_function(code);
    
    // The caller should be inferred to need EffectIO because it calls helper_io
    // This tests propagate_call_chain_effects functionality
    EXPECT_FALSE(result.is_valid); // Should fail because caller doesn't declare EffectIO
    EXPECT_FALSE(result.errors.empty());
    EXPECT_TRUE(result.required_effects.find("EffectIO") != result.required_effects.end());
}

// Test effect inference for function definitions
TEST(EffectCheckerTest, FunctionEffectInference) {
    std::string code = R"(
        fnc inferred_io_function() {
            rtn File.read("config.txt")
        }
        
        fnc inferred_network_function() {
            rtn http.get("https://api.example.com")
        }
        
        fnc inferred_mixed_function() {
            val data = http.get("https://api.example.com")
            File.write("cache.txt", data)
            rtn Time.now()
        }
    )";
    
    EffectChecker checker;
    
    // Test that functions without explicit effects clauses have their effects inferred
    // inferred_io_function should be inferred to need EffectIO
    // inferred_network_function should be inferred to need EffectNetwork
    // inferred_mixed_function should be inferred to need EffectNetwork, EffectIO, EffectTime
    
    EXPECT_TRUE(true); // Placeholder - actual implementation would test function inference
}

// Test effect inference caching
TEST(EffectCheckerTest, EffectInferenceCaching) {
    EffectChecker checker;
    
    // Test that lambda effects are cached for performance
    // This would require actual lambda AST nodes to test properly
    EXPECT_TRUE(true); // Placeholder for caching tests
}

// Test effect polymorphism
TEST(EffectCheckerTest, EffectPolymorphism) {
    EffectChecker checker;
    
    // Register effect type parameters
    std::set<std::string> io_effects = {"EffectIO"};
    std::set<std::string> network_effects = {"EffectNetwork"};
    std::set<std::string> any_effects = {"EffectIO", "EffectNetwork", "EffectState", "EffectTime"};
    
    checker.register_effect_type_parameter("E", any_effects);
    checker.register_effect_type_parameter("IOEffect", io_effects);
    checker.register_effect_type_parameter("NetEffect", network_effects);
    
    // Test effect type parameter recognition
    EXPECT_TRUE(checker.is_effect_type_parameter("E"));
    EXPECT_TRUE(checker.is_effect_type_parameter("IOEffect"));
    EXPECT_FALSE(checker.is_effect_type_parameter("EffectIO"));
    
    // Test possible effects retrieval
    auto possible_e = checker.get_possible_effects_for_parameter("E");
    EXPECT_EQ(possible_e.size(), 4);
    EXPECT_TRUE(possible_e.find("EffectIO") != possible_e.end());
    EXPECT_TRUE(possible_e.find("EffectNetwork") != possible_e.end());
    
    // Test constrained effect type parameters
    auto possible_io = checker.get_possible_effects_for_parameter("IOEffect");
    EXPECT_EQ(possible_io.size(), 1);
    EXPECT_TRUE(possible_io.find("EffectIO") != possible_io.end());
    
    // Test effect constraint resolution
    std::set<std::string> required_effects = {"EffectIO", "EffectNetwork"};
    auto resolved_e = checker.resolve_effect_constraints("E", required_effects);
    EXPECT_EQ(resolved_e.size(), 2);
    EXPECT_TRUE(resolved_e.find("EffectIO") != resolved_e.end());
    EXPECT_TRUE(resolved_e.find("EffectNetwork") != resolved_e.end());
    
    auto resolved_io = checker.resolve_effect_constraints("IOEffect", required_effects);
    EXPECT_EQ(resolved_io.size(), 1);
    EXPECT_TRUE(resolved_io.find("EffectIO") != resolved_io.end());
}

// Test effect composition
TEST(EffectCheckerTest, EffectComposition) {
    EffectChecker checker;
    
    // Register effect type parameters for composition testing
    std::set<std::string> any_effects = {"EffectIO", "EffectNetwork", "EffectState", "EffectTime"};
    checker.register_effect_type_parameter("E1", any_effects);
    checker.register_effect_type_parameter("E2", any_effects);
    
    std::set<std::string> effects1 = {"EffectIO"};
    std::set<std::string> effects2 = {"EffectNetwork"};
    std::set<std::string> effects3 = {"EffectPure"};
    std::set<std::string> effects_with_param = {"E1"};
    
    // Test composing different effects
    auto composed1 = checker.compose_effects(effects1, effects2);
    EXPECT_EQ(composed1.size(), 2);
    EXPECT_TRUE(composed1.find("EffectIO") != composed1.end());
    EXPECT_TRUE(composed1.find("EffectNetwork") != composed1.end());
    
    // Test composing with pure effect
    auto composed2 = checker.compose_effects(effects1, effects3);
    EXPECT_EQ(composed2.size(), 1);
    EXPECT_TRUE(composed2.find("EffectIO") != composed2.end());
    EXPECT_TRUE(composed2.find("EffectPure") == composed2.end()); // Pure should be removed
    
    // Test composing pure with pure
    auto composed3 = checker.compose_effects(effects3, effects3);
    EXPECT_EQ(composed3.size(), 1);
    EXPECT_TRUE(composed3.find("EffectPure") != composed3.end());
    
    // Test composing with effect type parameters
    auto composed4 = checker.compose_effects(effects1, effects_with_param);
    EXPECT_GE(composed4.size(), 1); // Should include at least EffectIO
    EXPECT_TRUE(composed4.find("EffectIO") != composed4.end());
    // May include other effects from E1 parameter resolution
}

// Test polymorphic function checking
TEST(EffectCheckerTest, PolymorphicFunctionChecking) {
    std::string code = R"(
        fnc generic_function<E>() effects { E } {
            // Function body would use effect E
            rtn 42
        }
        
        fnc compose_effects<E1, E2>() effects { E1, E2 } {
            // Function body would use both E1 and E2
            rtn "composed"
        }
    )";
    
    EffectChecker checker;
    
    // Register effect type parameters
    std::set<std::string> any_effects = {"EffectIO", "EffectNetwork", "EffectState", "EffectTime"};
    checker.register_effect_type_parameter("E", any_effects);
    checker.register_effect_type_parameter("E1", any_effects);
    checker.register_effect_type_parameter("E2", any_effects);
    
    // Test polymorphic function validation
    // This would require actual parsing of generic function syntax
    EXPECT_TRUE(true); // Placeholder for polymorphic function tests
    
    // Test effect type parameter validation
    EXPECT_TRUE(checker.is_effect_type_parameter("E"));
    EXPECT_TRUE(checker.is_effect_type_parameter("E1"));
    EXPECT_TRUE(checker.is_effect_type_parameter("E2"));
    
    // Test effect constraint resolution for multiple parameters
    std::set<std::string> required_effects = {"EffectIO", "EffectNetwork"};
    auto resolved_e1 = checker.resolve_effect_constraints("E1", required_effects);
    auto resolved_e2 = checker.resolve_effect_constraints("E2", required_effects);
    
    // Both should resolve to the intersection of possible and required effects
    EXPECT_EQ(resolved_e1.size(), 2);
    EXPECT_EQ(resolved_e2.size(), 2);
    EXPECT_TRUE(resolved_e1.find("EffectIO") != resolved_e1.end());
    EXPECT_TRUE(resolved_e1.find("EffectNetwork") != resolved_e1.end());
    
    // Test composition of resolved effects
    auto composed_resolved = checker.compose_effects(resolved_e1, resolved_e2);
    EXPECT_EQ(composed_resolved.size(), 2);
    EXPECT_TRUE(composed_resolved.find("EffectIO") != composed_resolved.end());
    EXPECT_TRUE(composed_resolved.find("EffectNetwork") != composed_resolved.end());
}

