/// @file mcp_deterministic_execution_test.cpp
/// MCP deterministic execution integration test (Task 55.3).
/// Validates: Requirements 35.2, 35.5
/// Executes the same script twice with deterministic=true and verifies
/// identical output.

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct DeterministicExecFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "det_exec_mod";
        root->kind = "module";

        FileSemantics sem;
        sem.path = "det_exec.meld";
        sem.ast = root;
        model.update_file("det_exec.meld", std::move(sem));
    }
};

TEST_F(DeterministicExecFixture, SameScriptProducesIdenticalOutput) {
    // Req 35.2, 35.5: deterministic=true → identical output
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);

    DeterministicConfig cfg;
    cfg.seed = 42;
    cfg.epoch = 1704067200;  // 2024-01-01T00:00:00Z
    cfg.time_increment_ms = 1;

    auto result1 = provider.execute_script("val x = 1", 5000, 16, true, cfg);
    auto result2 = provider.execute_script("val x = 1", 5000, 16, true, cfg);

    // Both runs should produce the same result
    EXPECT_EQ(result1.success, result2.success);
    EXPECT_EQ(result1.output, result2.output);

    // Both should echo the deterministic config
    ASSERT_TRUE(result1.deterministic_config.has_value());
    ASSERT_TRUE(result2.deterministic_config.has_value());
    EXPECT_EQ(result1.deterministic_config->seed, result2.deterministic_config->seed);
    EXPECT_EQ(result1.deterministic_config->epoch, result2.deterministic_config->epoch);
}

TEST_F(DeterministicExecFixture, NonDeterministicDoesNotEchoConfig) {
    // Req 35.6: when deterministic=false, no config echoed
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.execute_script("val x = 1", 5000, 16, false);
    EXPECT_FALSE(result.deterministic_config.has_value());
}

}  // namespace
