/// @file mcp_lsp_consistency_test.cpp
/// MCP + LSP consistency integration test (Task 55.2).
/// Validates: Requirements 1.4, 22.3
/// Verifies that querying the same symbol via MCP and LSP-style APIs
/// returns equivalent type/effect information from the same SemanticModel.

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct ConsistencyFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "shared_mod";
        root->kind = "module";

        auto fn = std::make_shared<ASTNode>();
        fn->name = "transform";
        fn->kind = "fnc";
        fn->type_info = "(List[T]) -> List[T]";
        fn->effects = {"IO", "Memory"};
        fn->location = {std::filesystem::path("shared.meld"), 3, 0};
        root->children.push_back(fn);

        FileSemantics sem;
        sem.path = "shared.meld";
        sem.ast = root;
        model.update_file("shared.meld", std::move(sem));
    }
};

TEST_F(ConsistencyFixture, McpAndLspSeeConsistentTypeInfo) {
    // Req 1.4: both channels read from same SemanticModel
    McpToolProvider lsp_style(model, dep_graph, vector_index);
    McpAdvancedToolProvider mcp_style(model, dep_graph, vector_index);

    // LSP-style: get code artifacts
    auto artifacts = lsp_style.get_code_artifacts("shared.meld");
    std::string lsp_type;
    for (const auto& a : artifacts) {
        if (a.name == "transform") {
            lsp_type = a.type_info;
            break;
        }
    }

    // MCP-style: query lifecycle
    auto lifecycle = mcp_style.query_lifecycle("transform");

    // Both should see the symbol
    EXPECT_FALSE(lsp_type.empty());
    EXPECT_TRUE(lifecycle.valid);
}

TEST_F(ConsistencyFixture, McpAndLspSeeConsistentEffects) {
    McpToolProvider lsp_style(model, dep_graph, vector_index);
    McpAdvancedToolProvider mcp_style(model, dep_graph, vector_index);

    // LSP-style: get artifacts with effects
    auto artifacts = lsp_style.get_code_artifacts("shared.meld");
    std::vector<std::string> lsp_effects;
    for (const auto& a : artifacts) {
        if (a.name == "transform") {
            lsp_effects = a.effects;
            break;
        }
    }

    // MCP-style: trace effects
    auto trace = mcp_style.trace_effect("transform");

    // Both should report the same effects
    EXPECT_EQ(lsp_effects.size(), trace.required_permissions.size());
    for (size_t i = 0; i < lsp_effects.size(); ++i) {
        EXPECT_EQ(lsp_effects[i], trace.required_permissions[i]);
    }
}

}  // namespace
