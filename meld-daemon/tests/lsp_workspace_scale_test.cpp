/**
 * LSP Workspace-Scale Test (Task 40.3)
 *
 * Open workspace with 100+ files → verify incremental diagnostics,
 * cross-file navigation, and workspace symbols.
 *
 * Validates: Requirements 20.1, 20.3, 20.4
 */

#include "meld/daemon/dependency_graph.hpp"
#include "meld/daemon/navigation_provider.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/workspace_provider.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper: populate the SemanticModel with N synthetic Meld files
// ---------------------------------------------------------------------------

static void populate_workspace(SemanticModel& model, size_t file_count) {
    for (size_t i = 0; i < file_count; ++i) {
        std::string filename = "src/mod_" + std::to_string(i) + ".meld";

        FileSemantics sem;
        sem.path = filename;

        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = "mod_" + std::to_string(i);
        root->location = {filename, 0, 0};

        // Each file exports a function "func_<i>"
        auto fn = std::make_shared<ASTNode>();
        fn->kind = "function_definition";
        fn->name = "func_" + std::to_string(i);
        fn->type_info = "(Int) -> Int";
        fn->location = {filename, 1, 0};

        // Each file also exports a type "Type_<i>"
        auto ty = std::make_shared<ASTNode>();
        ty->kind = "type_definition";
        ty->name = "Type_" + std::to_string(i);
        ty->type_info = "struct";
        ty->location = {filename, 5, 0};

        root->children.push_back(fn);
        root->children.push_back(ty);

        sem.ast = root;
        sem.exports = {fn->name, ty->name};

        // Cross-file imports: each file imports from the previous one
        if (i > 0) {
            sem.imports = {"mod_" + std::to_string(i - 1)};

            // Add a reference to the previous file's function
            auto ref = std::make_shared<ASTNode>();
            ref->kind = "function_call";
            ref->name = "func_" + std::to_string(i - 1);
            ref->type_info = "(Int) -> Int";
            ref->location = {filename, 3, 4};
            fn->children.push_back(ref);
        }

        model.update_file(filename, std::move(sem));
    }
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class LspWorkspaceScaleTest : public ::testing::Test {
protected:
    void SetUp() override {
        populate_workspace(model_, kFileCount);
    }

    static constexpr size_t kFileCount = 120;  // > 100 files
    SemanticModel model_;
    DependencyGraph dep_graph_;
};

// ============================================================================
// 40.3a: Workspace file discovery (Req 20.1)
// ============================================================================

TEST_F(LspWorkspaceScaleTest, AllFilesIndexed) {
    EXPECT_EQ(model_.file_count(), kFileCount);

    auto files = model_.get_indexed_files();
    EXPECT_EQ(files.size(), kFileCount);
}

// ============================================================================
// 40.3b: Workspace symbols across 100+ files (Req 20.4)
// ============================================================================

TEST_F(LspWorkspaceScaleTest, WorkspaceSymbolSearchAcrossAllFiles) {
    NavigationProvider nav(model_);

    // Search for "func_" — should find symbols from all files
    auto results = nav.get_workspace_symbols("func_");
    EXPECT_GE(results.size(), kFileCount);

    // Search for a specific function
    auto specific = nav.get_workspace_symbols("func_50");
    EXPECT_FALSE(specific.empty());
    EXPECT_EQ(specific[0].name, "func_50");
}

TEST_F(LspWorkspaceScaleTest, WorkspaceSymbolSearchForTypes) {
    NavigationProvider nav(model_);

    // Search for type symbols
    auto types = nav.get_workspace_symbols("Type_");
    EXPECT_GE(types.size(), kFileCount);
}

// ============================================================================
// 40.3c: Cross-file navigation at scale (Req 20.3)
// ============================================================================

TEST_F(LspWorkspaceScaleTest, GoToDefinitionCrossFile) {
    NavigationProvider nav(model_);

    // func_49 is defined in mod_49.meld, referenced from mod_50.meld
    auto def = nav.go_to_definition("src/mod_50.meld", "func_49");
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->name, "func_49");
    EXPECT_EQ(def->file, fs::path("src/mod_49.meld"));
}

TEST_F(LspWorkspaceScaleTest, FindReferencesCrossFile) {
    NavigationProvider nav(model_);

    // func_0 is defined in mod_0 and referenced from mod_1
    auto refs = nav.find_references("func_0");
    EXPECT_GE(refs.size(), 1u);

    bool found_in_mod1 = false;
    for (const auto& ref : refs) {
        if (ref.file == fs::path("src/mod_1.meld")) found_in_mod1 = true;
    }
    EXPECT_TRUE(found_in_mod1);
}

// ============================================================================
// 40.3d: Incremental diagnostics at scale (Req 20.1, 20.4)
// ============================================================================

TEST_F(LspWorkspaceScaleTest, IncrementalUpdateSingleFile) {
    // Modify one file and verify the model updates correctly
    auto before_count = model_.file_count();

    // Update mod_50 with a diagnostic
    FileSemantics updated;
    updated.path = "src/mod_50.meld";
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "mod_50";
    root->location = {"src/mod_50.meld", 0, 0};

    auto fn = std::make_shared<ASTNode>();
    fn->kind = "function_definition";
    fn->name = "func_50";
    fn->type_info = "(Int) -> Int";
    fn->location = {"src/mod_50.meld", 1, 0};
    root->children.push_back(fn);
    updated.ast = root;
    updated.exports = {"func_50"};

    Diagnostic d;
    d.location = {"src/mod_50.meld", 3, 0};
    d.severity = DiagnosticSeverity::Error;
    d.message = "Type error in func_50";
    d.rule_id = "E1001";
    updated.diagnostics = {d};

    model_.update_file("src/mod_50.meld", std::move(updated));

    // File count should remain the same (update, not add)
    EXPECT_EQ(model_.file_count(), before_count);

    // Diagnostics for that file should reflect the update
    auto diags = model_.get_diagnostics("src/mod_50.meld");
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].rule_id, "E1001");

    // Other files should be unaffected
    auto other_diags = model_.get_diagnostics("src/mod_49.meld");
    EXPECT_TRUE(other_diags.empty());
}

TEST_F(LspWorkspaceScaleTest, AddNewFileIncrementally) {
    auto before = model_.file_count();

    FileSemantics sem;
    sem.path = "src/new_module.meld";
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "new_module";
    root->location = {"src/new_module.meld", 0, 0};

    auto fn = std::make_shared<ASTNode>();
    fn->kind = "function_definition";
    fn->name = "new_func";
    fn->type_info = "() -> String";
    fn->location = {"src/new_module.meld", 1, 0};
    root->children.push_back(fn);
    sem.ast = root;
    sem.exports = {"new_func"};
    model_.update_file("src/new_module.meld", std::move(sem));

    EXPECT_EQ(model_.file_count(), before + 1);

    // New symbol should be discoverable via workspace symbols
    NavigationProvider nav(model_);
    auto results = nav.get_workspace_symbols("new_func");
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].name, "new_func");
}

TEST_F(LspWorkspaceScaleTest, RemoveFileIncrementally) {
    auto before = model_.file_count();

    model_.remove_file("src/mod_99.meld");
    EXPECT_EQ(model_.file_count(), before - 1);

    // Removed file's symbols should no longer appear
    NavigationProvider nav(model_);
    auto def = nav.go_to_definition("src/mod_100.meld", "func_99");
    // Definition should not be found in the removed file
    if (def.has_value()) {
        EXPECT_NE(def->file, fs::path("src/mod_99.meld"));
    }
}

// ============================================================================
// 40.3e: Performance — operations on 100+ file workspace (Req 20.4)
// ============================================================================

TEST_F(LspWorkspaceScaleTest, WorkspaceSymbolSearchPerformance) {
    NavigationProvider nav(model_);

    auto start = std::chrono::steady_clock::now();
    auto results = nav.get_workspace_symbols("func_");
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should complete in reasonable time (< 1 second for 120 files)
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_LT(ms, 1000) << "Workspace symbol search took " << ms << "ms";
    EXPECT_GE(results.size(), kFileCount);
}

TEST_F(LspWorkspaceScaleTest, DocumentSymbolsPerformance) {
    NavigationProvider nav(model_);

    auto start = std::chrono::steady_clock::now();
    // Get document symbols for every file
    for (size_t i = 0; i < kFileCount; ++i) {
        std::string file = "src/mod_" + std::to_string(i) + ".meld";
        auto symbols = nav.get_document_symbols(file);
        EXPECT_FALSE(symbols.empty());
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_LT(ms, 2000) << "Document symbols for " << kFileCount
                         << " files took " << ms << "ms";
}

TEST_F(LspWorkspaceScaleTest, CrossFileResolutionViaWorkspaceProvider) {
    WorkspaceProvider wp(model_, dep_graph_);

    // Resolve a cross-file reference
    auto ref = wp.resolve_cross_file_reference("src/mod_50.meld", "func_49");
    EXPECT_TRUE(ref.resolved);
    EXPECT_EQ(ref.symbol_name, "func_49");
    EXPECT_EQ(ref.target_file, fs::path("src/mod_49.meld"));
}

}  // namespace
}  // namespace meld::daemon
