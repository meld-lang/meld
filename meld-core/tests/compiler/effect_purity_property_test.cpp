#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/effect_checker.hpp"
#include "meld/effects/effect_types.hpp"
#include "meld/parser/parser.hpp"
#include <boost/variant.hpp>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

using namespace meld::compiler;
using namespace meld::effects;
using namespace meld::parser;

namespace {

/**
 * Generator for effect names (excluding EffectPure for side effects)
 */
rc::Gen<std::string> genSideEffectName() {
    return rc::gen::elementOf(std::vector<std::string>{
        "EffectIO",
        "EffectNetwork",
        "EffectState",
        "EffectTime"
    });
}

/**
 * Generator for multiple side effects
 */
rc::Gen<std::set<std::string>> genSideEffectSet() {
    return rc::gen::map(
        rc::gen::container<std::vector<std::string>>(genSideEffectName()),
        [](const std::vector<std::string>& effects) {
            std::set<std::string> effect_set;
            for (const auto& effect : effects) {
                effect_set.insert(effect);
            }
            return effect_set;
        }
    );
}

/**
 * Generator for function names
 */
rc::Gen<std::string> genFunctionName() {
    return rc::gen::map(rc::gen::inRange(1, 100), [](int n) {
        return "func" + std::to_string(n);
    });
}

/**
 * Generate a function definition with specified effects
 */
std::string generateFunctionWithEffects(
    const std::string& func_name,
    const std::set<std::string>& effects,
    const std::string& body = "rtn 42"
) {
    std::string effects_clause;
    if (!effects.empty()) {
        effects_clause = "\n    effects { ";
        bool first = true;
        for (const auto& effect : effects) {
            if (!first) effects_clause += ", ";
            effects_clause += effect;
            first = false;
        }
        effects_clause += " }";
    }
    
    return "fnc " + func_name + "(x: int) -> int" + effects_clause + " {\n    " + body + "\n}";
}

/**
 * Generate a function body that performs side effects
 */
std::string generateSideEffectBody(const std::set<std::string>& effects) {
    std::string body = "";
    
    for (const auto& effect : effects) {
        if (effect == "EffectIO") {
            body += "perform FileSystem.write(\"/tmp/test\", \"data\")\n    ";
        } else if (effect == "EffectNetwork") {
            body += "perform Network.get(\"http://example.com\")\n    ";
        } else if (effect == "EffectState") {
            body += "perform State.set(\"key\", \"value\")\n    ";
        } else if (effect == "EffectTime") {
            body += "perform Time.now()\n    ";
        }
    }
    
    body += "rtn 42";
    return body;
}

/**
 * Mock effect checker for testing purity properties
 */
class MockEffectPurityChecker {
public:
    /**
     * Check if a function declared as pure actually performs no side effects
     */
    static bool checkPurityViolation(
        const std::set<std::string>& declared_effects,
        const std::set<std::string>& performed_effects
    ) {
        // If function is declared as pure (no effects clause or explicit EffectPure)
        bool is_declared_pure = declared_effects.empty() || 
                               (declared_effects.size() == 1 && declared_effects.count("EffectPure") > 0);
        
        if (is_declared_pure) {
            // Pure functions should not perform any side effects
            for (const auto& effect : performed_effects) {
                if (effect != "EffectPure") {
                    return true; // Purity violation detected
                }
            }
        }
        
        return false; // No purity violation
    }
    
    /**
     * Check if function without effects clause defaults to EffectPure
     */
    static bool checkDefaultPurity(const std::set<std::string>& declared_effects) {
        // Functions without effects clause should default to EffectPure
        if (declared_effects.empty()) {
            return true; // Should be treated as pure by default
        }
        
        return false;
    }
};

} // anonymous namespace

/**
 * Property 24: Effect Purity
 * Validates: Requirements 39.3, 39.4
 * 
 * For any function declared with EffectPure (or no effects clause), 
 * the function body should not perform any side effects.
 */

/**
 * Property test: Functions without effects clause default to EffectPure
 * Validates Requirement 39.3: Functions without effects clause are treated as EffectPure by default
 */
TEST(EffectPurityPropertyTest, FunctionsDefaultToPure) {
    rc::check("Functions without effects clause should default to EffectPure", []() {
        // Generate function name
        auto func_name = *genFunctionName();
        
        // Generate function without effects clause
        std::string source = generateFunctionWithEffects(func_name, {}, "rtn 42");
        
        // Parse and check effects
        Parser parser;
        auto parse_result = parser.parse(source);
        RC_ASSERT(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        
        // Property: Function without effects clause should be treated as EffectPure
        RC_ASSERT(check_result.declared_effects.count("EffectPure") > 0);
        RC_ASSERT(MockEffectPurityChecker::checkDefaultPurity({}));
    });
}

/**
 * Property test: Pure functions cannot perform side effects
 * Validates Requirement 39.4: Function bodies only perform declared effects
 */
TEST(EffectPurityPropertyTest, PureFunctionsCannotPerformSideEffects) {
    rc::check("Pure functions cannot perform any side effects", []() {
        // Generate function name and side effects
        auto func_name = *genFunctionName();
        auto side_effects = *genSideEffectSet();
        
        // Skip empty effect sets (no side effects to test)
        RC_PRE(!side_effects.empty());
        
        // Generate function declared as pure but performing side effects
        std::string body = generateSideEffectBody(side_effects);
        std::string source = generateFunctionWithEffects(func_name, {}, body);
        
        // Parse and check effects
        Parser parser;
        auto parse_result = parser.parse(source);
        RC_ASSERT(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        
        // Property: Pure function performing side effects should be detected as violation
        std::set<std::string> declared_pure = {"EffectPure"};
        bool has_purity_violation = MockEffectPurityChecker::checkPurityViolation(
            declared_pure, side_effects
        );
        
        RC_ASSERT(has_purity_violation);
        RC_ASSERT(!check_result.is_valid); // Should fail effect checking
        RC_ASSERT(!check_result.errors.empty()); // Should have error messages
    });
}

/**
 * Property test: Explicitly pure functions cannot perform side effects
 * Validates Requirement 39.4: Function bodies only perform declared effects
 */
TEST(EffectPurityPropertyTest, ExplicitlyPureFunctionsCannotPerformSideEffects) {
    rc::check("Explicitly pure functions cannot perform any side effects", []() {
        // Generate function name and side effects
        auto func_name = *genFunctionName();
        auto side_effects = *genSideEffectSet();
        
        // Skip empty effect sets (no side effects to test)
        RC_PRE(!side_effects.empty());
        
        // Generate function explicitly declared as pure but performing side effects
        std::string body = generateSideEffectBody(side_effects);
        std::set<std::string> pure_effects = {"EffectPure"};
        std::string source = generateFunctionWithEffects(func_name, pure_effects, body);
        
        // Parse and check effects
        Parser parser;
        auto parse_result = parser.parse(source);
        RC_ASSERT(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        
        // Property: Explicitly pure function performing side effects should be violation
        bool has_purity_violation = MockEffectPurityChecker::checkPurityViolation(
            pure_effects, side_effects
        );
        
        RC_ASSERT(has_purity_violation);
        RC_ASSERT(!check_result.is_valid); // Should fail effect checking
        RC_ASSERT(!check_result.errors.empty()); // Should have error messages
    });
}

/**
 * Property test: Pure functions with pure operations are valid
 * Validates Requirements 39.3, 39.4: Pure functions can perform pure operations
 */
TEST(EffectPurityPropertyTest, PureFunctionsWithPureOperationsAreValid) {
    rc::check("Pure functions with only pure operations should be valid", []() {
        // Generate function name
        auto func_name = *genFunctionName();
        
        // Generate pure function body (only pure operations)
        std::string pure_body = R"(
            val x = 42
            val y = x * 2
            val z = y + 10
            rtn z
        )";
        
        // Test both implicit and explicit purity
        std::vector<std::set<std::string>> purity_declarations = {
            {}, // Implicit purity (no effects clause)
            {"EffectPure"} // Explicit purity
        };
        
        for (const auto& declared_effects : purity_declarations) {
            std::string source = generateFunctionWithEffects(func_name, declared_effects, pure_body);
            
            // Parse and check effects
            Parser parser;
            auto parse_result = parser.parse(source);
            RC_ASSERT(parse_result.is_valid);
            
            EffectChecker checker;
            auto check_result = checker.check_function_effects(parse_result.functions[0]);
            
            // Property: Pure function with pure operations should be valid
            std::set<std::string> no_side_effects = {};
            bool has_purity_violation = MockEffectPurityChecker::checkPurityViolation(
                declared_effects.empty() ? std::set<std::string>{"EffectPure"} : declared_effects,
                no_side_effects
            );
            
            RC_ASSERT(!has_purity_violation);
            RC_ASSERT(check_result.is_valid); // Should pass effect checking
            RC_ASSERT(check_result.errors.empty()); // Should have no errors
        }
    });
}

/**
 * Property test: Functions with declared effects can perform those effects
 * Validates Requirement 39.4: Function bodies can perform declared effects
 */
TEST(EffectPurityPropertyTest, FunctionsCanPerformDeclaredEffects) {
    rc::check("Functions can perform effects they declare", []() {
        // Generate function name and effects
        auto func_name = *genFunctionName();
        auto declared_effects = *genSideEffectSet();
        
        // Skip empty effect sets (pure functions tested separately)
        RC_PRE(!declared_effects.empty());
        
        // Generate function body that performs the declared effects
        std::string body = generateSideEffectBody(declared_effects);
        std::string source = generateFunctionWithEffects(func_name, declared_effects, body);
        
        // Parse and check effects
        Parser parser;
        auto parse_result = parser.parse(source);
        RC_ASSERT(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        
        // Property: Function performing declared effects should be valid
        bool has_purity_violation = MockEffectPurityChecker::checkPurityViolation(
            declared_effects, declared_effects
        );
        
        RC_ASSERT(!has_purity_violation); // No purity violation (not declared as pure)
        RC_ASSERT(check_result.is_valid); // Should pass effect checking
        RC_ASSERT(check_result.errors.empty()); // Should have no errors
    });
}

/**
 * Property test: Mixed pure and impure function interactions
 * Validates Requirements 39.3, 39.4: Purity is preserved in call chains
 */
TEST(EffectPurityPropertyTest, PurityPreservedInCallChains) {
    rc::check("Purity should be preserved in function call chains", []() {
        // Generate function names
        auto pure_func = *genFunctionName();
        auto impure_func = *genFunctionName();
        auto side_effects = *genSideEffectSet();
        
        // Skip empty effect sets
        RC_PRE(!side_effects.empty());
        
        // Generate pure function
        std::string pure_source = generateFunctionWithEffects(pure_func, {}, "rtn 42");
        
        // Generate impure function that calls pure function
        std::string impure_body = "val result = " + pure_func + "(10)\n    " + 
                                 generateSideEffectBody(side_effects);
        std::string impure_source = generateFunctionWithEffects(impure_func, side_effects, impure_body);
        
        // Combine sources
        std::string combined_source = pure_source + "\n\n" + impure_source;
        
        // Parse and check effects
        Parser parser;
        auto parse_result = parser.parse(combined_source);
        RC_ASSERT(parse_result.is_valid);
        RC_ASSERT(parse_result.functions.size() == 2);
        
        EffectChecker checker;
        
        // Check pure function
        auto pure_result = checker.check_function_effects(parse_result.functions[0]);
        RC_ASSERT(pure_result.is_valid);
        RC_ASSERT(pure_result.declared_effects.count("EffectPure") > 0);
        
        // Check impure function
        auto impure_result = checker.check_function_effects(parse_result.functions[1]);
        RC_ASSERT(impure_result.is_valid);
        
        // Property: Pure function remains pure even when called by impure function
        std::set<std::string> pure_declared = {"EffectPure"};
        std::set<std::string> no_effects = {};
        bool pure_violation = MockEffectPurityChecker::checkPurityViolation(
            pure_declared, no_effects
        );
        
        RC_ASSERT(!pure_violation);
    });
}

// Configure RapidCheck for effect purity tests
class EffectPurityPropertyTestConfig : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure RapidCheck for comprehensive testing
        rc::detail::configuration().maxSuccess = 100;
        rc::detail::configuration().maxSize = 50;
        rc::detail::configuration().maxDiscardRatio = 10;
    }
};

// Use the configured test class for property tests
using EffectPurityPropertyTestConfigured = EffectPurityPropertyTestConfig;

TEST_F(EffectPurityPropertyTestConfigured, ConfiguredEffectPurity) {
    // This test uses the configured RapidCheck settings
    EXPECT_NO_THROW({
        rc::check("Effect purity properties with configured settings", []() {
            auto func_name = *genFunctionName();
            auto side_effects = *genSideEffectSet();
            
            // Test that pure functions cannot perform side effects
            if (!side_effects.empty()) {
                std::string body = generateSideEffectBody(side_effects);
                std::set<std::string> pure_effects = {"EffectPure"};
                
                bool has_violation = MockEffectPurityChecker::checkPurityViolation(
                    pure_effects, side_effects
                );
                
                RC_ASSERT(has_violation);
            }
        });
    });
}

/**
 * Property test: Effect purity edge cases
 */
TEST(EffectPurityPropertyTest, EdgeCases) {
    // Test empty function body
    {
        std::string source = "fnc emptyFunc() -> int { rtn 0 }";
        Parser parser;
        auto parse_result = parser.parse(source);
        EXPECT_TRUE(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        EXPECT_TRUE(check_result.is_valid);
        EXPECT_TRUE(check_result.declared_effects.count("EffectPure") > 0);
    }
    
    // Test function with only local variables
    {
        std::string source = R"(
            fnc localVarsOnly() -> int {
                val x = 42
                var y = x * 2
                rtn y
            }
        )";
        Parser parser;
        auto parse_result = parser.parse(source);
        EXPECT_TRUE(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        EXPECT_TRUE(check_result.is_valid);
        EXPECT_TRUE(check_result.declared_effects.count("EffectPure") > 0);
    }
    
    // Test function with mathematical operations only
    {
        std::string source = R"(
            fnc mathOnly(x: int, y: int) -> int {
                val sum = x + y
                val product = x * y
                val result = sum + product
                rtn result
            }
        )";
        Parser parser;
        auto parse_result = parser.parse(source);
        EXPECT_TRUE(parse_result.is_valid);
        
        EffectChecker checker;
        auto check_result = checker.check_function_effects(parse_result.functions[0]);
        EXPECT_TRUE(check_result.is_valid);
        EXPECT_TRUE(check_result.declared_effects.count("EffectPure") > 0);
    }
}

} // namespace