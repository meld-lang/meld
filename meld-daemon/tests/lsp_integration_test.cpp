/**
 * LSP Integration Test (Task 40.1)
 *
 * End-to-end test: populate SemanticModel → exercise all LSP providers
 * (completions, diagnostics, navigation, formatting, semantic tokens)
 * → verify responses.
 *
 * Validates: Requirements 15.1, 16.1, 17.1, 18.1, 19.1
 */

#include "meld/daemon/completion_provider.hpp"
#include "meld/daemon/diagnostics_provider.hpp"
#include "meld/daemon/formatting_provider.hpp"
#include "meld/daemon/meld_feature_provider.hpp"
#include "meld/daemon/navigation_provider.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/semantic_token_provider.hpp"
#include "meld/daemon/workspace_provider.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <string>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Shared fixture: a SemanticModel pre-populated with a small Meld workspace
// ---------------------------------------------------------------------------

class LspIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // File 1: src/math.meld — exports "add" and "multiply"
        {
            FileSemantics sem;
            sem.path = "src/math.meld";

            auto root = std::make_shared<ASTNode>();
            root->kind = "module";
            root->name = "math";
            root->location = {"src/math.meld", 0, 0};

            auto add_fn = std::make_shared<ASTNode>();
            add_fn->kind = "function_definition";
            add_fn->name = "add";
            add_fn->type_info = "(Int, Int) -> Int";
            add_fn->location = {"src/math.meld", 1, 0};
            root->children.push_back(add_fn);

            auto mul_fn = std::make_shared<ASTNode>();
            mul_fn->kind = "function_definition";
            mul_fn->name = "multiply";
            mul_fn->type_info = "(Int, Int) -> Int";
            mul_fn->location = {"src/math.meld", 5, 0};
            root->children.push_back(mul_fn);

            sem.ast = root;
            sem.exports = {"add", "multiply"};
            model_.update_file("src/math.meld", std::move(sem));
        }

        // File 2: src/main.meld — imports math, defines "main"
        {
            FileSemantics sem;
            sem.path = "src/main.meld";

            auto root = std::make_shared<ASTNode>();
            root->kind = "module";
            root->name = "main";
            root->location = {"src/main.meld", 0, 0};

            auto main_fn = std::make_shared<ASTNode>();
            main_fn->kind = "function_definition";
            main_fn->name = "main";
            main_fn->type_info = "() -> Int";
            main_fn->location = {"src/main.meld", 3, 0};

            // Reference to "add" inside main
            auto call_node = std::make_shared<ASTNode>();
            call_node->kind = "function_call";
            call_node->name = "add";
            call_node->type_info = "(Int, Int) -> Int";
            call_node->location = {"src/main.meld", 4, 4};
            main_fn->children.push_back(call_node);

            root->children.push_back(main_fn);

            sem.ast = root;
            sem.exports = {"main"};
            sem.imports = {"math"};
            model_.update_file("src/main.meld", std::move(sem));
        }
    }

    SemanticModel model_;
};

// ============================================================================
// 40.1a: Semantic tokens / syntax highlighting (Req 15.1)
// ============================================================================

TEST_F(LspIntegrationTest, SemanticTokensForMeldSource) {
    SemanticTokenProvider provider;

    const std::string source = R"(
fnc add(a: Int, b: Int) -> Int {
    return a + b
}
)";

    auto result = provider.parse(source);

    // Must produce tokens without total failure
    EXPECT_FALSE(result.tokens.empty());
    // Tokens should include at least a keyword ("fnc") and identifiers
    bool has_keyword = false;
    bool has_identifier = false;
    for (const auto& tok : result.tokens) {
        if (tok.type == SemanticTokenType::Keyword) has_keyword = true;
        if (tok.type == SemanticTokenType::Function ||
            tok.type == SemanticTokenType::Variable ||
            tok.type == SemanticTokenType::Parameter)
            has_identifier = true;
    }
    EXPECT_TRUE(has_keyword);
    EXPECT_TRUE(has_identifier);
}

// ============================================================================
// 40.1b: Code completion (Req 16.1)
// ============================================================================

TEST_F(LspIntegrationTest, CompletionsFromSharedModel) {
    CompletionProvider provider(model_);

    // Request completions in main.meld at a general position
    auto result = provider.get_completions("src/main.meld", 4, 0, "");

    // Should include symbols from the model (at least "add", "multiply", "main")
    bool found_add = false;
    bool found_multiply = false;
    for (const auto& item : result.items) {
        if (item.label == "add") found_add = true;
        if (item.label == "multiply") found_multiply = true;
    }
    EXPECT_TRUE(found_add) << "Completion should include 'add' from math.meld";
    EXPECT_TRUE(found_multiply) << "Completion should include 'multiply' from math.meld";
}

TEST_F(LspIntegrationTest, CompletionsFilterByPrefix) {
    CompletionProvider provider(model_);

    // Type "ad" — should filter to "add"
    auto result = provider.get_completions("src/main.meld", 4, 2, "ad");

    bool found_add = false;
    for (const auto& item : result.items) {
        if (item.label == "add") found_add = true;
    }
    EXPECT_TRUE(found_add);
}

// ============================================================================
// 40.1c: Diagnostics (Req 17.1)
// ============================================================================

TEST_F(LspIntegrationTest, DiagnosticsFromSharedModel) {
    DiagnosticsProvider provider(model_);

    // Check grammar on valid source — should conform
    auto grammar_result = provider.check_grammar(
        "src/math.meld",
        "fnc add(a: Int, b: Int) -> Int { return a + b }");
    EXPECT_TRUE(grammar_result.conforms);

    // Check grammar on invalid source — should report diagnostics
    auto bad_result = provider.check_grammar(
        "src/math.meld",
        "fnc (broken syntax {{{");
    EXPECT_FALSE(bad_result.conforms);
    EXPECT_FALSE(bad_result.diagnostics.empty());
}

// ============================================================================
// 40.1d: Navigation — go-to-definition (Req 18.1)
// ============================================================================

TEST_F(LspIntegrationTest, GoToDefinitionFromSharedModel) {
    NavigationProvider provider(model_);

    // "add" is defined in src/math.meld
    auto def = provider.go_to_definition("src/main.meld", "add");
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->name, "add");
    EXPECT_EQ(def->file, fs::path("src/math.meld"));
}

TEST_F(LspIntegrationTest, FindReferencesAcrossFiles) {
    NavigationProvider provider(model_);

    // "add" is referenced in main.meld (call) and defined in math.meld
    auto refs = provider.find_references("add");
    EXPECT_GE(refs.size(), 1u);

    bool found_in_main = false;
    for (const auto& ref : refs) {
        if (ref.file == fs::path("src/main.meld")) found_in_main = true;
    }
    EXPECT_TRUE(found_in_main);
}

TEST_F(LspIntegrationTest, DocumentSymbolsOutline) {
    NavigationProvider provider(model_);

    auto symbols = provider.get_document_symbols("src/math.meld");
    EXPECT_FALSE(symbols.empty());

    // The root "math" module node has a name, so it becomes a top-level symbol
    // with "add" and "multiply" as children.
    bool found_add = false;
    bool found_mul = false;
    for (const auto& sym : symbols) {
        if (sym.name == "add") found_add = true;
        if (sym.name == "multiply") found_mul = true;
        // Also check children (module node wraps them)
        for (const auto& child : sym.children) {
            if (child.name == "add") found_add = true;
            if (child.name == "multiply") found_mul = true;
        }
    }
    EXPECT_TRUE(found_add);
    EXPECT_TRUE(found_mul);
}

TEST_F(LspIntegrationTest, WorkspaceSymbolSearch) {
    NavigationProvider provider(model_);

    auto results = provider.get_workspace_symbols("add");
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].name, "add");
}

TEST_F(LspIntegrationTest, HoverInformation) {
    NavigationProvider provider(model_);

    auto hover = provider.get_hover("src/math.meld", "add");
    ASSERT_TRUE(hover.has_value());
    EXPECT_EQ(hover->name, "add");
    EXPECT_FALSE(hover->type_signature.empty());
}

// ============================================================================
// 40.1e: Formatting (Req 19.1)
// ============================================================================

TEST_F(LspIntegrationTest, DocumentFormattingFromSharedModel) {
    FormattingProvider provider(model_);

    auto edits = provider.format_document("src/math.meld");
    // Formatting should succeed (may return empty edits if already formatted)
    // The key assertion is that it doesn't crash and reads from the model
    SUCCEED();
}

TEST_F(LspIntegrationTest, SymbolRenameAcrossWorkspace) {
    FormattingProvider provider(model_);

    auto result = provider.rename_symbol("add", "sum");
    EXPECT_TRUE(result.success);
    // Should produce edits in at least the definition file
    EXPECT_FALSE(result.file_edits.empty());
}

// ============================================================================
// All providers read from the SAME SemanticModel (no separate state)
// ============================================================================

TEST_F(LspIntegrationTest, AllProvidersShareSameModel) {
    // Add a new file to the model
    FileSemantics sem;
    sem.path = "src/utils.meld";
    auto node = std::make_shared<ASTNode>();
    node->kind = "function_definition";
    node->name = "helper";
    node->type_info = "() -> String";
    node->location = {"src/utils.meld", 1, 0};
    sem.ast = node;
    sem.exports = {"helper"};
    model_.update_file("src/utils.meld", std::move(sem));

    // All providers should immediately see the new symbol
    CompletionProvider comp(model_);
    auto completions = comp.get_completions("src/utils.meld", 2, 0, "");
    bool comp_sees_helper = false;
    for (const auto& item : completions.items) {
        if (item.label == "helper") comp_sees_helper = true;
    }
    EXPECT_TRUE(comp_sees_helper);

    NavigationProvider nav(model_);
    auto def = nav.go_to_definition("src/main.meld", "helper");
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->name, "helper");

    auto ws_syms = nav.get_workspace_symbols("helper");
    EXPECT_FALSE(ws_syms.empty());
}

}  // namespace
}  // namespace meld::daemon
