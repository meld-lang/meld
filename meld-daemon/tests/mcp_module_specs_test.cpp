/// @file mcp_module_specs_test.cpp
/// Unit tests for get_module_specs (Req 32).
/// Task 51.1 — Validates: Requirements 32.1, 32.2, 32.3, 32.4, 32.7

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct ModuleSpecsFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "math_utils";
        root->kind = "module";

        auto fn1 = std::make_shared<ASTNode>();
        fn1->name = "add";
        fn1->kind = "fnc";
        fn1->type_info = "(Int, Int) -> Int";
        fn1->effects = {"Pure"};
        fn1->location = {std::filesystem::path("math_utils.meld"), 2, 0};
        root->children.push_back(fn1);

        auto fn2 = std::make_shared<ASTNode>();
        fn2->name = "multiply";
        fn2->kind = "fnc";
        fn2->type_info = "(Int, Int) -> Int";
        fn2->location = {std::filesystem::path("math_utils.meld"), 6, 0};
        root->children.push_back(fn2);

        FileSemantics sem;
        sem.path = "math_utils.meld";
        sem.ast = root;
        model.update_file("math_utils.meld", std::move(sem));
    }
};

TEST_F(ModuleSpecsFixture, RetrievesAllSpecsForModule) {
    // Req 32.1: returns all @blueprint spec blocks
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto specs = provider.get_module_specs("math_utils");
    EXPECT_EQ(specs.module_coordinate, "math_utils");
    EXPECT_EQ(specs.specs.size(), 2u);
}

TEST_F(ModuleSpecsFixture, SpecEntryHasRequiredFields) {
    // Req 32.2: symbol name, action, expected result, effects, status
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto specs = provider.get_module_specs("math_utils");
    ASSERT_FALSE(specs.specs.empty());
    const auto& entry = specs.specs[0];
    EXPECT_FALSE(entry.symbol_name.empty());
    EXPECT_FALSE(entry.action_source.empty());
    EXPECT_FALSE(entry.expected_result.empty());
    EXPECT_FALSE(entry.verification_status.empty());
}

TEST_F(ModuleSpecsFixture, VerificationStatusIsVerified) {
    // Req 32.3: verified for passing specs
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto specs = provider.get_module_specs("math_utils");
    for (const auto& s : specs.specs) {
        EXPECT_EQ(s.verification_status, "verified");
    }
}

TEST_F(ModuleSpecsFixture, SymbolFilterReturnsOnlyMatchingSpecs) {
    // Req 32.4: optional symbol parameter filters
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto specs = provider.get_module_specs("math_utils", "add");
    ASSERT_EQ(specs.specs.size(), 1u);
    EXPECT_EQ(specs.specs[0].symbol_name, "add");
}

TEST_F(ModuleSpecsFixture, FallbackToExamplesWhenNoSpecs) {
    // Req 32.7: empty list with fallback to examples
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto specs = provider.get_module_specs("nonexistent_module");
    EXPECT_TRUE(specs.specs.empty());
    EXPECT_FALSE(specs.examples.empty());
}

}  // namespace
