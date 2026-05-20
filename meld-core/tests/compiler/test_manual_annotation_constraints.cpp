#include "../../include/meld/compiler/effect_checker.hpp"
#include "../../include/meld/parser/ast.hpp"
#include <gtest/gtest.h>
#include <string>
#include <set>
#include <vector>

namespace meld::compiler::test {

class ManualAnnotationConstraintsTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<EffectChecker>();
    }

    std::unique_ptr<EffectChecker> checker;

    // Helper function to create a function definition with manual @uses annotation
    parser::ast::function_definition create_function_with_manual_annotation(
        const std::string& name,
        const std::vector<std::string>& manual_effects,
        const std::vector<std::string>& implementation_effects) {
        
        parser::ast::function_definition func_def;
        func_def.name.name = name;
        func_def.has_effects = true;
        
        // Set manual effects clause
        for (const auto& effect : manual_effects) {
            parser::ast::identifier effect_id;
            effect_id.name = effect;
            func_def.effects_clause.push_back(effect_id);
        }
        
        // Create a mock function body that would infer the implementation effects
        // For testing purposes, we'll register the implementation effects directly
        std::set<std::string> impl_effects_set(implementation_effects.begin(), implementation_effects.end());
        checker->register_function_effects(name, impl_effects_set);
        
        return func_def;
    }

    // Helper function to create a pure function with manual annotation
    parser::ast::function_definition create_pure_function_with_manual_annotation(
        const std::string& name,
        bool has_manual_pure_annotation = true) {
        
        parser::ast::function_definition func_def;
        func_def.name.name = name;
        func_def.has_effects = has_manual_pure_annotation;
        
        if (has_manual_pure_annotation) {
            // Empty effects clause represents @uses() (pure)
            func_def.effects_clause.clear();
        }
        
        // Register as pure function
        checker->register_function_effects(name, {"EffectPure"});
        
        return func_def;
    }
};

// Test 1: Function with manual annotation that matches implementation should pass
TEST_F(ManualAnnotationConstraintsTest, ManualAnnotationMatchesImplementation) {
    // Requirement 41.11: Manual annotation should be treated as constraint and verified
    auto func_def = create_function_with_manual_annotation(
        "readFile", 
        {"EffectIO"}, 
        {"EffectIO"}
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(result.declared_effects, std::set<std::string>{"EffectIO"});
    EXPECT_EQ(result.required_effects, std::set<std::string>{"EffectIO"});
}

// Test 2: Function with manual annotation that doesn't match implementation should fail
TEST_F(ManualAnnotationConstraintsTest, ManualAnnotationMismatchesImplementation) {
    // Requirement 41.12: Compilation error when implementation performs undeclared effects
    auto func_def = create_function_with_manual_annotation(
        "fetchAndSave", 
        {"EffectIO"},           // Manual annotation only declares IO
        {"EffectIO", "EffectNetwork"}  // Implementation performs both IO and Network
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Check that error message mentions the undeclared effect
    bool found_network_error = false;
    for (const auto& error : result.errors) {
        if (error.find("EffectNetwork") != std::string::npos && 
            error.find("not declared") != std::string::npos) {
            found_network_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_network_error);
}

// Test 3: Pure function with manual @uses() annotation should pass if implementation is pure
TEST_F(ManualAnnotationConstraintsTest, PureFunctionWithManualPureAnnotation) {
    auto func_def = create_pure_function_with_manual_annotation("calculate", true);
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// Test 4: Pure function with manual @uses() annotation should fail if implementation has effects
TEST_F(ManualAnnotationConstraintsTest, PureFunctionWithManualPureAnnotationButImpureImplementation) {
    // Requirement 41.12: Pure annotation with impure implementation should fail
    auto func_def = create_function_with_manual_annotation(
        "calculateWithLogging", 
        {},  // Empty effects clause = @uses() = pure
        {"EffectIO"}  // Implementation performs IO
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Check that error message mentions pure annotation violation
    bool found_pure_violation = false;
    for (const auto& error : result.errors) {
        if (error.find("pure") != std::string::npos && 
            error.find("EffectIO") != std::string::npos) {
            found_pure_violation = true;
            break;
        }
    }
    EXPECT_TRUE(found_pure_violation);
}

// Test 5: Function without manual annotation should not be validated as constraint
TEST_F(ManualAnnotationConstraintsTest, FunctionWithoutManualAnnotationNotValidated) {
    parser::ast::function_definition func_def;
    func_def.name.name = "autoInferredFunction";
    func_def.has_effects = false;  // No manual annotation
    
    EXPECT_FALSE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_TRUE(result.is_valid);  // Should pass because no constraint to validate
    EXPECT_TRUE(result.errors.empty());
}

// Test 6: Multiple effects in manual annotation should be validated correctly
TEST_F(ManualAnnotationConstraintsTest, MultipleEffectsInManualAnnotation) {
    auto func_def = create_function_with_manual_annotation(
        "complexOperation", 
        {"EffectIO", "EffectNetwork", "EffectState"},  // Manual annotation declares three effects
        {"EffectIO", "EffectNetwork", "EffectState"}   // Implementation matches
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// Test 7: Implementation with subset of manual annotation effects should pass
TEST_F(ManualAnnotationConstraintsTest, ImplementationUsesSubsetOfManualAnnotation) {
    // Manual annotation can declare more effects than implementation uses (over-specification is allowed)
    auto func_def = create_function_with_manual_annotation(
        "conditionalIO", 
        {"EffectIO", "EffectNetwork"},  // Manual annotation allows both
        {"EffectIO"}                   // Implementation only uses IO
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// Test 8: Implementation with superset of manual annotation effects should fail
TEST_F(ManualAnnotationConstraintsTest, ImplementationUsesMoreThanManualAnnotation) {
    // Requirement 41.12: Implementation cannot perform more effects than declared
    auto func_def = create_function_with_manual_annotation(
        "unexpectedNetwork", 
        {"EffectIO"},                          // Manual annotation only allows IO
        {"EffectIO", "EffectNetwork", "EffectTime"}  // Implementation performs more
    );
    
    EXPECT_TRUE(checker->has_manual_uses_annotation(func_def));
    
    auto result = checker->validate_manual_annotation_constraints(func_def);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Check that error mentions both undeclared effects
    std::string combined_errors;
    for (const auto& error : result.errors) {
        combined_errors += error + " ";
    }
    EXPECT_TRUE(combined_errors.find("EffectNetwork") != std::string::npos);
    EXPECT_TRUE(combined_errors.find("EffectTime") != std::string::npos);
}

// Test 9: Manual annotation compatibility check
TEST_F(ManualAnnotationConstraintsTest, ManualAnnotationCompatibilityCheck) {
    std::set<std::string> manual_effects = {"EffectIO", "EffectNetwork"};
    std::set<std::string> inferred_effects = {"EffectIO"};
    
    // Implementation uses subset - should be compatible
    EXPECT_TRUE(checker->is_manual_annotation_compatible(manual_effects, inferred_effects));
    
    // Implementation uses superset - should not be compatible
    inferred_effects.insert("EffectTime");
    EXPECT_FALSE(checker->is_manual_annotation_compatible(manual_effects, inferred_effects));
    
    // Pure manual annotation with pure implementation - should be compatible
    std::set<std::string> pure_manual = {};
    std::set<std::string> pure_inferred = {"EffectPure"};
    EXPECT_TRUE(checker->is_manual_annotation_compatible(pure_manual, pure_inferred));
    
    // Pure manual annotation with impure implementation - should not be compatible
    std::set<std::string> impure_inferred = {"EffectIO"};
    EXPECT_FALSE(checker->is_manual_annotation_compatible(pure_manual, impure_inferred));
}

// Test 10: Error message generation for manual annotation mismatches
TEST_F(ManualAnnotationConstraintsTest, ErrorMessageGeneration) {
    std::set<std::string> manual_effects = {"EffectIO"};
    std::set<std::string> inferred_effects = {"EffectIO", "EffectNetwork", "EffectTime"};
    
    auto errors = checker->generate_manual_annotation_errors(
        manual_effects, inferred_effects, "testFunction"
    );
    
    EXPECT_FALSE(errors.empty());
    
    // Check that error message contains function name and undeclared effects
    std::string error_text = errors[0];
    EXPECT_TRUE(error_text.find("testFunction") != std::string::npos);
    EXPECT_TRUE(error_text.find("EffectNetwork") != std::string::npos);
    EXPECT_TRUE(error_text.find("EffectTime") != std::string::npos);
    EXPECT_TRUE(error_text.find("not declared") != std::string::npos);
    
    // Check that error message suggests updated annotation
    EXPECT_TRUE(error_text.find("@uses(") != std::string::npos);
}

// Test 11: Integration with main effect checking
TEST_F(ManualAnnotationConstraintsTest, IntegrationWithMainEffectChecking) {
    // Test that manual annotation constraints are checked first in check_function
    auto func_def = create_function_with_manual_annotation(
        "violatesConstraint", 
        {"EffectIO"},           // Manual constraint
        {"EffectIO", "EffectNetwork"}  // Implementation violates constraint
    );
    
    // Mock the function body to return the implementation effects
    // In a real scenario, this would be analyzed from the AST
    
    auto result = checker->check_function(func_def);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Should return early due to constraint violation
    bool found_constraint_error = false;
    for (const auto& error : result.errors) {
        if (error.find("not declared in manual @uses annotation") != std::string::npos) {
            found_constraint_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_constraint_error);
}

} // namespace meld::compiler::test