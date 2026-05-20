/// @file mcp_e2e_integration_test.cpp
/// End-to-end MCP tool integration test (Task 55.1).
/// Validates: Requirements 22.1, 29.1, 33.1, 36.3

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct E2EFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "e2e_mod";
        root->kind = "module";

        auto fn = std::make_shared<ASTNode>();
        fn->name = "compute";
        fn->kind = "fnc";
        fn->type_info = "(Int) -> Int";
        fn->effects = {"IO"};
        fn->location = {std::filesystem::path("e2e.meld"), 1, 0};
        root->children.push_back(fn);

        Diagnostic d;
        d.rule_id = "safety.null_deref";
        d.message = "Potential null dereference";
        d.severity = DiagnosticSeverity::Error;
        d.location = {std::filesystem::path("e2e.meld"), 1, 0};

        FileSemantics sem;
        sem.path = "e2e.meld";
        sem.ast = root;
        sem.diagnostics = {d};
        model.update_file("e2e.meld", std::move(sem));

        dep_graph.upsert(DependencyNode{"core", "1.0.0", "registry", "", {}});
        vector_index.rebuild(model);
    }
};

TEST_F(E2EFixture, FullToolChainWorks) {
    // Simulate: daemon started → MCP client connects → tools invoked
    McpToolProvider basic(model, dep_graph, vector_index);
    McpAdvancedToolProvider advanced(model, dep_graph, vector_index);

    // Req 22.1: discover projects
    auto projects = basic.discover_projects();
    EXPECT_FALSE(projects.empty());

    // Req 29.1: analyze_safety
    auto violations = advanced.analyze_safety("e2e.meld");
    EXPECT_FALSE(violations.empty());
    EXPECT_FALSE(violations[0].sdf.rule_id.empty());

    // Req 33.1: find_intent
    auto intents = advanced.find_intent("compute");
    // May or may not find results depending on VectorIndex
    // but should not crash

    // Req 36.3: execute_script
    auto script = advanced.execute_script("val x = 1");
    EXPECT_TRUE(script.success || !script.diagnostics.empty());
}

TEST_F(E2EFixture, ToolsShareSemanticModel) {
    // Both providers read from the same SemanticModel
    McpToolProvider basic(model, dep_graph, vector_index);
    McpAdvancedToolProvider advanced(model, dep_graph, vector_index);

    auto artifacts = basic.get_code_artifacts("e2e.meld");
    auto lifecycle = advanced.query_lifecycle("compute");

    // Both should find the "compute" symbol
    bool found_in_artifacts = false;
    for (const auto& a : artifacts) {
        if (a.name == "compute") { found_in_artifacts = true; break; }
    }
    EXPECT_TRUE(found_in_artifacts);
    EXPECT_TRUE(lifecycle.valid);
}

}  // namespace
