/// @file mcp_named_tools_test.cpp
/// Unit tests for named MCP tools (Req 29).
/// Task 49.2 — Validates: Requirements 29.1–29.8

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct NamedToolsFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "mymod";
        root->kind = "module";

        auto fn = std::make_shared<ASTNode>();
        fn->name = "process";
        fn->kind = "fnc";
        fn->type_info = "Hold[Buffer] -> Int";
        fn->effects = {"IO", "Memory"};
        fn->location = {std::filesystem::path("mymod.meld"), 5, 2};
        root->children.push_back(fn);

        Diagnostic d;
        d.rule_id = "safety.dangling_ref";
        d.message = "Dangling reference to buffer";
        d.severity = DiagnosticSeverity::Error;
        d.location = {std::filesystem::path("mymod.meld"), 5, 2};

        FileSemantics sem;
        sem.path = "mymod.meld";
        sem.ast = root;
        sem.diagnostics = {d};
        model.update_file("mymod.meld", std::move(sem));

        dep_graph.upsert(DependencyNode{"dep_a", "1.0.0", "registry", "", {"dep_b"}});
        dep_graph.upsert(DependencyNode{"dep_b", "2.0.0", "registry", "", {}});
    }
};

TEST_F(NamedToolsFixture, AnalyzeSafetyReturnsViolations) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("mymod.meld");
    ASSERT_FALSE(violations.empty());
    EXPECT_FALSE(violations[0].sdf.rule_id.empty());
    EXPECT_EQ(violations[0].sdf.severity, "error");
}

TEST_F(NamedToolsFixture, QueryLifecycleReturnsOwnership) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto info = provider.query_lifecycle("process");
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.ownership_state, "Hold[T]");
    EXPECT_FALSE(info.scope.empty());
}

TEST_F(NamedToolsFixture, TraceEffectReturnsPermissions) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto trace = provider.trace_effect("process");
    EXPECT_EQ(trace.expression, "process");
    EXPECT_FALSE(trace.required_permissions.empty());
    // Should include IO and Memory from the node's effects
    EXPECT_EQ(trace.required_permissions.size(), 2u);
}

TEST_F(NamedToolsFixture, ResolveVersionConflictDetectsCollisions) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto conflict = provider.resolve_version_conflict("dep_a");
    EXPECT_EQ(conflict.dependency, "dep_a");
    // dep_b@2.0.0 vs dep_a@1.0.0
    EXPECT_FALSE(conflict.collisions.empty());
    EXPECT_FALSE(conflict.resolution_strategies.empty());
}

TEST_F(NamedToolsFixture, DryRunPatchVerifiesWithoutDiskWrites) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    ASTPatch patch;
    patch.selector = "$.fnc.process";
    patch.action = "replace";
    patch.content = "new_body()";
    auto result = provider.dry_run_patch({patch});
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(NamedToolsFixture, DryRunPatchRejectsInvalidAction) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    ASTPatch patch;
    patch.selector = "$.fnc.process";
    patch.action = "invalid_action";
    patch.content = "x";
    auto result = provider.dry_run_patch({patch});
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST_F(NamedToolsFixture, ApplyPatchCommitsAfterVfsVerification) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    ASTPatch patch;
    patch.selector = "$.fnc.process";
    patch.action = "replace";
    patch.content = "fixed()";
    auto result = provider.apply_patch({patch});
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.files_modified, 1u);
}

TEST_F(NamedToolsFixture, DiagnoseBuildReturnsStructuredErrors) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto diag = provider.diagnose_build("mymod.meld");
    EXPECT_EQ(diag.target, "mymod.meld");
    EXPECT_FALSE(diag.errors.empty());
    EXPECT_FALSE(diag.fix_suggestions.empty());
}

TEST_F(NamedToolsFixture, VerifyClosureReturnsVersionTree) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto closure = provider.verify_closure();
    EXPECT_TRUE(closure.complete);
    EXPECT_GE(closure.version_tree.size(), 2u);
}

}  // namespace
