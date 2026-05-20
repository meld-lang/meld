#include <gtest/gtest.h>
#include "meld/lsp/effect_diagnostic_provider.hpp"
#include "../../include/meld/compiler/effect_checker.hpp"
#include "../../include/meld/parser/ast.hpp"

using namespace meld::lsp;
using namespace meld::compiler;
using namespace meld::parser::ast;

class EffectDiagnosticProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker_ = std::make_shared<EffectChecker>();
        provider_ = std::make_unique<EffectDiagnosticProvider>(checker_);
    }

    std::shared_ptr<EffectChecker> checker_;
    std::unique_ptr<EffectDiagnosticProvider> provider_;

    // Helper: create a function definition with optional @uses effects
    function_definition make_function(
        const std::string& name,
        const std::vector<std::string>& effects = {}) {

        function_definition func;
        func.name.name = name;
        func.has_effects = !effects.empty();
        func.has_tests = false;
        func.has_contracts = false;
        for (const auto& eff : effects) {
            identifier id;
            id.name = eff;
            func.effects_clause.push_back(id);
        }
        return func;
    }

    // Helper: create an implicit_effect_call node
    implicit_effect_call make_implicit_call(
        const std::string& effect,
        const std::string& operation) {

        implicit_effect_call call;
        call.effect_name.name = effect;
        call.operation_name.name = operation;
        return call;
    }
};

// ---------------------------------------------------------------------------
// provide_function_diagnostics — valid function, no errors
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, ValidFunctionProducesNoDiagnostics) {
    auto func = make_function("pureAdd");
    // Pure function with no effect calls — should be clean.
    auto diags = provider_->provide_function_diagnostics(func);
    EXPECT_TRUE(diags.empty());
}

// ---------------------------------------------------------------------------
// validate_implicit_effect_call — top-level usage (Requirement 7.3)
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, TopLevelImplicitCallReportsError) {
    auto call = make_implicit_call("Console", "println");

    // No enclosing function → top-level
    auto diags = provider_->validate_implicit_effect_call(call, nullptr);

    ASSERT_FALSE(diags.empty());
    EXPECT_EQ(diags[0].severity, DiagnosticSeverity::Error);
    EXPECT_NE(diags[0].message.find("top level"), std::string::npos);
    EXPECT_EQ(diags[0].code.value_or(""), "effect-top-level");
}

// ---------------------------------------------------------------------------
// validate_implicit_effect_call — unknown effect (Requirement 7.1)
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, UnknownEffectReportsError) {
    auto call = make_implicit_call("UnknownEffect", "doStuff");
    auto func = make_function("myFunc", {"UnknownEffect"});

    auto diags = provider_->validate_implicit_effect_call(call, &func);

    // The validate_effect_operation should flag the unknown effect.
    bool has_unknown_error = false;
    for (const auto& d : diags) {
        if (d.severity == DiagnosticSeverity::Error &&
            d.message.find("Unknown effect") != std::string::npos) {
            has_unknown_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_unknown_error);
}

// ---------------------------------------------------------------------------
// validate_implicit_effect_call — missing @uses annotation (Requirement 2.3)
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, MissingUsesAnnotationReportsError) {
    auto call = make_implicit_call("Console", "println");
    // Function with NO @uses annotation
    auto func = make_function("myFunc");

    auto diags = provider_->validate_implicit_effect_call(call, &func);

    bool has_missing_uses = false;
    for (const auto& d : diags) {
        if (d.severity == DiagnosticSeverity::Error &&
            d.message.find("no @uses annotation") != std::string::npos) {
            has_missing_uses = true;
            break;
        }
    }
    EXPECT_TRUE(has_missing_uses);
}

// ---------------------------------------------------------------------------
// validate_implicit_effect_call — effect not in @uses (Requirement 2.2)
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, EffectNotInUsesAnnotationReportsError) {
    auto call = make_implicit_call("Console", "println");
    // Function declares EffectNetwork but NOT the Console/EffectIO effect
    auto func = make_function("myFunc", {"EffectNetwork"});

    auto diags = provider_->validate_implicit_effect_call(call, &func);

    bool has_undeclared = false;
    for (const auto& d : diags) {
        if (d.severity == DiagnosticSeverity::Error &&
            d.message.find("not listed in @uses") != std::string::npos) {
            has_undeclared = true;
            break;
        }
    }
    EXPECT_TRUE(has_undeclared);
}

// ---------------------------------------------------------------------------
// validate_implicit_effect_call — valid call with matching @uses
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, ValidImplicitCallWithMatchingUsesProducesNoUsesDiagnostic) {
    auto call = make_implicit_call("Console", "println");
    // Function declares Console — the effect checker may still flag the operation
    // as unknown (Console.println is registered under EffectIO), but the @uses
    // check itself should not fire for the Console name.
    auto func = make_function("myFunc", {"Console"});

    auto diags = provider_->validate_implicit_effect_call(call, &func);

    // Should NOT have a "not listed in @uses" error for Console
    for (const auto& d : diags) {
        EXPECT_EQ(d.message.find("not listed in @uses"), std::string::npos)
            << "Unexpected @uses error: " << d.message;
    }
}

// ---------------------------------------------------------------------------
// collect_perform_deprecation_diagnostics — Requirement 4.3
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, PerformExpressionProducesDeprecationWarning) {
    // Build a perform_expression wrapped in an expression variant.
    perform_expression perform;
    perform.effect_name.name = "Console";
    perform.operation_name.name = "println";
    perform.is_deprecated = true;

    expression expr(perform);

    auto diags = provider_->collect_perform_deprecation_diagnostics(expr);

    ASSERT_FALSE(diags.empty());
    EXPECT_EQ(diags[0].severity, DiagnosticSeverity::Warning);
    EXPECT_NE(diags[0].message.find("Deprecated"), std::string::npos);
    EXPECT_EQ(diags[0].code.value_or(""), "effect-deprecated");
}

// ---------------------------------------------------------------------------
// provide_diagnostics — batch over multiple functions
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, ProvideDiagnosticsOverMultipleFunctions) {
    std::vector<function_definition> functions;
    functions.push_back(make_function("pureFunc"));
    functions.push_back(make_function("anotherPure"));

    auto diags = provider_->provide_diagnostics("test.meld", functions);

    // Two pure functions with no effect calls should produce no diagnostics.
    EXPECT_TRUE(diags.empty());
}

// ---------------------------------------------------------------------------
// Diagnostic source field is always "meld-effect-checker"
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, DiagnosticSourceIsEffectChecker) {
    auto call = make_implicit_call("Console", "println");
    auto diags = provider_->validate_implicit_effect_call(call, nullptr);

    for (const auto& d : diags) {
        EXPECT_EQ(d.source, "meld-effect-checker");
    }
}

// ---------------------------------------------------------------------------
// Default constructor creates its own EffectChecker
// ---------------------------------------------------------------------------
TEST_F(EffectDiagnosticProviderTest, DefaultConstructorWorks) {
    EffectDiagnosticProvider default_provider;
    auto func = make_function("pureFunc");
    auto diags = default_provider.provide_function_diagnostics(func);
    EXPECT_TRUE(diags.empty());
}
