/// @file test_trait_var_fnc_checker_pass.cpp
/// @brief Unit tests for the Trait Var Fnc Checker Pass
///
/// Tests that the pass correctly:
///   - Accepts matching `var fnc` in both trait and impl (OK)
///   - Accepts matching non-`var fnc` in both trait and impl (OK)
///   - Rejects impl adding `var fnc` when trait doesn't declare it (E5003)
///   - Rejects impl omitting `var fnc` when trait requires it (E5004)
///   - Handles multiple methods with mixed mutability
///   - Handles empty traits (no diagnostics)
///   - Skips methods not found in impl (handled by other passes)
///
/// Requirements: 57.8

#include <gtest/gtest.h>
#include "meld/compiler/trait_var_fnc_checker_pass.hpp"

using namespace meld::compiler;
namespace ast = meld::parser::ast;

// ===========================================================================
// Helpers: build AST nodes for testing
// ===========================================================================

/// Create an identifier node.
static ast::identifier make_id(const std::string& name) {
    ast::identifier id;
    id.name = name;
    return id;
}

/// Create a function_definition with the given name and is_mutating flag.
static ast::function_definition make_impl_method(
    const std::string& name,
    bool is_mutating = false
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.is_mutating = is_mutating;
    ast::block_expression body;
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return func;
}

/// Create a TraitMethodSignature.
static TraitMethodSignature make_trait_sig(
    const std::string& name,
    bool is_mutating = false
) {
    return TraitMethodSignature{name, is_mutating};
}

// ===========================================================================
// Test fixture
// ===========================================================================

class TraitVarFncCheckerPassTest : public ::testing::Test {
protected:
    TraitVarFncCheckerPass pass;
};

// --- Test 1: Both trait and impl use `var fnc` — OK ---

TEST_F(TraitVarFncCheckerPassTest, MatchingVarFncBothSidesOK) {
    TraitImplPair pair{
        .trait_name = "Resettable",
        .impl_type_name = "Timer",
        .trait_methods = {make_trait_sig("reset", true)},
        .impl_methods = {make_impl_method("reset", true)}
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 2: Both trait and impl are non-`var fnc` — OK ---

TEST_F(TraitVarFncCheckerPassTest, MatchingNonVarFncBothSidesOK) {
    TraitImplPair pair{
        .trait_name = "Displayable",
        .impl_type_name = "Point",
        .trait_methods = {make_trait_sig("display", false)},
        .impl_methods = {make_impl_method("display", false)}
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 3: Trait has `var fnc`, impl doesn't — E5004 ---

TEST_F(TraitVarFncCheckerPassTest, TraitVarFncImplMissingEmitsE5004) {
    TraitImplPair pair{
        .trait_name = "Updatable",
        .impl_type_name = "MyClass",
        .trait_methods = {make_trait_sig("update", true)},
        .impl_methods = {make_impl_method("update", false)}
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5004");
    EXPECT_EQ(result.diagnostics[0].level, TraitVarFncDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("update"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("MyClass"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Updatable"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("var"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
}

// --- Test 4: Trait doesn't have `var fnc`, impl does — E5003 ---

TEST_F(TraitVarFncCheckerPassTest, ImplAddsVarFncEmitsE5003) {
    TraitImplPair pair{
        .trait_name = "Updatable",
        .impl_type_name = "MyClass",
        .trait_methods = {make_trait_sig("update", false)},
        .impl_methods = {make_impl_method("update", true)}
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5003");
    EXPECT_EQ(result.diagnostics[0].level, TraitVarFncDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("update"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("MyClass"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Updatable"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
}

// --- Test 5: Multiple methods, mixed — correct diagnostics for each ---

TEST_F(TraitVarFncCheckerPassTest, MultipleMethodsMixedDiagnostics) {
    // Trait: var fnc reset(), fnc display(), var fnc clear()
    // Impl:  var fnc reset() [OK], var fnc display() [E5003], fnc clear() [E5004]
    TraitImplPair pair{
        .trait_name = "Widget",
        .impl_type_name = "Button",
        .trait_methods = {
            make_trait_sig("reset", true),
            make_trait_sig("display", false),
            make_trait_sig("clear", true)
        },
        .impl_methods = {
            make_impl_method("reset", true),
            make_impl_method("display", true),
            make_impl_method("clear", false)
        }
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 3u);
    EXPECT_EQ(result.errors_emitted, 2u);
    ASSERT_EQ(result.diagnostics.size(), 2u);

    bool found_e5003_display = false;
    bool found_e5004_clear = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "E5003" && diag.message.find("display") != std::string::npos) {
            found_e5003_display = true;
        }
        if (diag.code == "E5004" && diag.message.find("clear") != std::string::npos) {
            found_e5004_clear = true;
        }
    }
    EXPECT_TRUE(found_e5003_display);
    EXPECT_TRUE(found_e5004_clear);
}

// --- Test 6: Empty trait — no diagnostics ---

TEST_F(TraitVarFncCheckerPassTest, EmptyTraitNoDiagnostics) {
    TraitImplPair pair{
        .trait_name = "Marker",
        .impl_type_name = "MyClass",
        .trait_methods = {},
        .impl_methods = {}
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 7: Method not found in impl — skip (handled by other passes) ---

TEST_F(TraitVarFncCheckerPassTest, MissingImplMethodSkipped) {
    TraitImplPair pair{
        .trait_name = "Serializable",
        .impl_type_name = "Data",
        .trait_methods = {make_trait_sig("serialize", false)},
        .impl_methods = {}  // No implementation provided
    };
    auto result = pass.run({pair}, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 8: Multiple trait-impl pairs ---

TEST_F(TraitVarFncCheckerPassTest, MultiplePairsCheckedIndependently) {
    TraitImplPair ok_pair{
        .trait_name = "Readable",
        .impl_type_name = "File",
        .trait_methods = {make_trait_sig("read", false)},
        .impl_methods = {make_impl_method("read", false)}
    };
    TraitImplPair bad_pair{
        .trait_name = "Writable",
        .impl_type_name = "File",
        .trait_methods = {make_trait_sig("write", true)},
        .impl_methods = {make_impl_method("write", false)}
    };
    auto result = pass.run({ok_pair, bad_pair}, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5004");
    EXPECT_NE(result.diagnostics[0].message.find("write"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Writable"), std::string::npos);
}

// --- Test 9: Source file propagation ---

TEST_F(TraitVarFncCheckerPassTest, SourceFileInDiagnostics) {
    TraitImplPair pair{
        .trait_name = "Updatable",
        .impl_type_name = "MyClass",
        .trait_methods = {make_trait_sig("update", false)},
        .impl_methods = {make_impl_method("update", true)}
    };
    auto result = pass.run({pair}, "src/widgets.meld");

    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/widgets.meld");
}

// --- Test 10: No pairs — no diagnostics ---

TEST_F(TraitVarFncCheckerPassTest, NoPairsNoDiagnostics) {
    auto result = pass.run({}, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}
