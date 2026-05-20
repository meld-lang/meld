/// @file mcp_refactor_test.cpp
/// Unit tests for refactor tool (Req 34).
/// Task 51.3 — Validates: Requirements 34.1, 34.2, 34.4, 34.6, 34.8

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct RefactorFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "refactor_mod";
        root->kind = "module";

        auto fn = std::make_shared<ASTNode>();
        fn->name = "old_function";
        fn->kind = "fnc";
        fn->type_info = "() -> Void";
        fn->location = {std::filesystem::path("refactor.meld"), 1, 0};
        root->children.push_back(fn);

        FileSemantics sem;
        sem.path = "refactor.meld";
        sem.ast = root;
        model.update_file("refactor.meld", std::move(sem));
    }
};

TEST_F(RefactorFixture, RenameIntentProducesTransform) {
    // Req 34.1, 34.2: rename intent produces AST_Transform
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("rename", "old_function");
    EXPECT_FALSE(result.ambiguous);
    ASSERT_FALSE(result.transforms.empty());
    EXPECT_EQ(result.transforms[0].intent_type, "rename");
    EXPECT_EQ(result.transforms[0].target, "old_function");
    EXPECT_FALSE(result.transforms[0].transform_json.empty());
}

TEST_F(RefactorFixture, ExtractFunctionIntentWorks) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("extract_function", "old_function");
    EXPECT_FALSE(result.ambiguous);
    ASSERT_FALSE(result.transforms.empty());
    EXPECT_EQ(result.transforms[0].intent_type, "extract_function");
}

TEST_F(RefactorFixture, DryRunDefaultTrue) {
    // Req 34.6: dry_run parameter defaults to true
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("rename", "old_function");
    EXPECT_TRUE(result.dry_run);
}

TEST_F(RefactorFixture, InvalidIntentReturnsDisambiguation) {
    // Req 34.8: ambiguous intent returns disambiguation candidates
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("unknown_intent", "old_function");
    EXPECT_TRUE(result.ambiguous);
    EXPECT_FALSE(result.disambiguation_candidates.empty());
    // Should list supported intents
    EXPECT_GE(result.disambiguation_candidates.size(), 5u);
}

TEST_F(RefactorFixture, TargetNotFoundReportsError) {
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("rename", "nonexistent");
    ASSERT_FALSE(result.transforms.empty());
    EXPECT_FALSE(result.transforms[0].validation_diagnostics.empty());
}

TEST_F(RefactorFixture, ValidTransformHasNoValidationErrors) {
    // Req 34.5: validate transforms before returning
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto result = provider.refactor("rename", "old_function");
    ASSERT_FALSE(result.transforms.empty());
    EXPECT_TRUE(result.transforms[0].validation_diagnostics.empty());
}

}  // namespace
