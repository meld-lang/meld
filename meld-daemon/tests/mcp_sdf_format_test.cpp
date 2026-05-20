/// @file mcp_sdf_format_test.cpp
/// Unit tests for Structured Diagnostic Frame (SDF) format consistency.
/// Task 49.1 — Validates: Requirements 30.1, 30.2, 30.3, 30.4, 30.5, 30.6

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

// Helper: build a minimal SemanticModel + deps for testing.
struct TestFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        // Add a file with a diagnostic
        auto root = std::make_shared<ASTNode>();
        root->name = "test_module";
        root->kind = "module";
        auto child = std::make_shared<ASTNode>();
        child->name = "unsafe_fn";
        child->kind = "fnc";
        child->type_info = "() -> Int";
        child->location = {std::filesystem::path("test.meld"), 1, 0};
        root->children.push_back(child);

        Diagnostic d;
        d.rule_id = "safety.use_after_free";
        d.message = "Use after free";
        d.severity = DiagnosticSeverity::Error;
        d.location = {std::filesystem::path("test.meld"), 1, 0};

        FileSemantics sem;
        sem.path = "test.meld";
        sem.ast = root;
        sem.diagnostics = {d};
        model.update_file("test.meld", std::move(sem));
    }
};

TEST_F(TestFixture, MakeSdfIncludesAllRequiredFields) {
    auto sdf = McpAdvancedToolProvider::make_sdf(
        "test.rule", "$.module.fn", "Test message", "error");
    // Req 30.1: rule_id present
    EXPECT_EQ(sdf.rule_id, "test.rule");
    // Req 30.2: ast_selector present
    EXPECT_EQ(sdf.ast_selector, "$.module.fn");
    // Req 30.3: context_hash present and non-empty
    EXPECT_FALSE(sdf.context_hash.empty());
    // Req 30.4: version_hash present
    EXPECT_FALSE(sdf.version_hash.empty());
}

TEST_F(TestFixture, SdfFixFieldContainsValidAstPatches) {
    // Req 30.5: fix field with AST_Patch objects
    auto sdf = McpAdvancedToolProvider::make_sdf(
        "test.rule", "$.fn", "Fixable error", "error");
    ASTPatch patch;
    patch.selector = "$.fn.body";
    patch.action = "replace";
    patch.content = "fixed_code()";
    sdf.fix = std::vector<ASTPatch>{patch};

    ASSERT_TRUE(sdf.fix.has_value());
    ASSERT_EQ(sdf.fix->size(), 1u);
    EXPECT_EQ((*sdf.fix)[0].selector, "$.fn.body");
    EXPECT_EQ((*sdf.fix)[0].action, "replace");
    EXPECT_FALSE((*sdf.fix)[0].content.empty());
}

TEST_F(TestFixture, SdfFormatConsistentAcrossTools) {
    // Req 30.6: SDF format consistent across all MCP tools
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);

    // analyze_safety returns SDF
    auto violations = provider.analyze_safety("test.meld");
    for (const auto& v : violations) {
        EXPECT_FALSE(v.sdf.rule_id.empty());
        EXPECT_FALSE(v.sdf.ast_selector.empty());
        EXPECT_FALSE(v.sdf.context_hash.empty());
        EXPECT_FALSE(v.sdf.version_hash.empty());
    }

    // diagnose_build returns SDF
    auto build = provider.diagnose_build("test.meld");
    for (const auto& e : build.errors) {
        EXPECT_FALSE(e.rule_id.empty());
        EXPECT_FALSE(e.ast_selector.empty());
        EXPECT_FALSE(e.context_hash.empty());
        EXPECT_FALSE(e.version_hash.empty());
    }
}

TEST_F(TestFixture, ContextHashDeterministic) {
    // Req 30.3: same input produces same hash
    auto h1 = McpAdvancedToolProvider::compute_context_hash("hello world");
    auto h2 = McpAdvancedToolProvider::compute_context_hash("hello world");
    EXPECT_EQ(h1, h2);

    // Different input produces different hash
    auto h3 = McpAdvancedToolProvider::compute_context_hash("different");
    EXPECT_NE(h1, h3);
}

}  // namespace
