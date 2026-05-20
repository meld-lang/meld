/// @file mcp_deterministic_test.cpp
/// Unit tests for MCP deterministic execution mode (Req 35).
/// Task 52.1 — Validates: Requirements 35.1, 35.2, 35.4, 35.5, 35.7

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct DeterministicFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "det_mod";
        root->kind = "module";

        FileSemantics sem;
        sem.path = "det.meld";
        sem.ast = root;
        model.update_file("det.meld", std::move(sem));
    }
};

TEST_F(DeterministicFixture, DeterministicDefaultOff) {
    // Req 35.1: deterministic defaults to false
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    EXPECT_FALSE(provider.is_deterministic());
    EXPECT_FALSE(provider.get_deterministic_config().has_value());
}

TEST_F(DeterministicFixture, ActivateDeterministicMode) {
    // Req 35.2: when true, activates DeterministicContext
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    provider.set_deterministic(true);
    EXPECT_TRUE(provider.is_deterministic());
    EXPECT_TRUE(provider.get_deterministic_config().has_value());
}

TEST_F(DeterministicFixture, ConfigOverrideWorks) {
    // Req 35.3, 35.4: custom config with seed, epoch, time_increment_ms
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    DeterministicConfig cfg;
    cfg.seed = 123;
    cfg.epoch = 1700000000;
    cfg.time_increment_ms = 10;
    provider.set_deterministic(true, cfg);

    auto active_cfg = provider.get_deterministic_config();
    ASSERT_TRUE(active_cfg.has_value());
    EXPECT_EQ(active_cfg->seed, 123u);
    EXPECT_EQ(active_cfg->epoch, 1700000000u);
    EXPECT_EQ(active_cfg->time_increment_ms, 10u);
}

TEST_F(DeterministicFixture, DeactivateDeterministicMode) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    provider.set_deterministic(true);
    EXPECT_TRUE(provider.is_deterministic());
    provider.set_deterministic(false);
    EXPECT_FALSE(provider.is_deterministic());
    EXPECT_FALSE(provider.get_deterministic_config().has_value());
}

TEST_F(DeterministicFixture, ResponseEchoesConfig) {
    // Req 35.5: response includes deterministic_config
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.execute_script("val x = 1", 5000, 16, true);
    EXPECT_TRUE(result.deterministic_config.has_value());
}

TEST_F(DeterministicFixture, StaticToolsExcluded) {
    // Req 35.7: deterministic does not affect static analysis tools
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("analyze_safety"));
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("query_lifecycle"));
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("trace_effect"));
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("find_intent"));
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("get_module_specs"));
    EXPECT_TRUE(McpAdvancedToolProvider::is_static_analysis_tool("refactor"));
    // Code-executing tools are NOT static
    EXPECT_FALSE(McpAdvancedToolProvider::is_static_analysis_tool("execute_script"));
    EXPECT_FALSE(McpAdvancedToolProvider::is_static_analysis_tool("dry_run_patch"));
}

}  // namespace
