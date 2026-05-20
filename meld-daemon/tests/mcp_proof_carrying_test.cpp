/// @file mcp_proof_carrying_test.cpp
/// Unit tests for proof-carrying failures (Req 31).
/// Task 50.1 — Validates: Requirements 31.1–31.6

#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace meld::daemon;

namespace {

struct ProofCarryingFixture : public ::testing::Test {
    SemanticModel model;
    DependencyGraph dep_graph;
    VectorIndex vector_index{std::make_shared<PassthroughEmbeddingProvider>()};

    void SetUp() override {
        auto root = std::make_shared<ASTNode>();
        root->name = "unsafe_mod";
        root->kind = "module";

        auto fn = std::make_shared<ASTNode>();
        fn->name = "leak_ref";
        fn->kind = "fnc";
        fn->type_info = "View[T] -> T";
        fn->location = {std::filesystem::path("unsafe.meld"), 10, 4};
        root->children.push_back(fn);

        Diagnostic d;
        d.rule_id = "safety.use_after_free";
        d.message = "Use after free of View[T]";
        d.severity = DiagnosticSeverity::Error;
        d.location = {std::filesystem::path("unsafe.meld"), 10, 4};

        FileSemantics sem;
        sem.path = "unsafe.meld";
        sem.ast = root;
        sem.diagnostics = {d};
        model.update_file("unsafe.meld", std::move(sem));
    }
};

TEST_F(ProofCarryingFixture, ViolationIncludesProvenanceTrace) {
    // Req 31.1: provenance_trace with Origin, Escape, Conflict
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());

    const auto& trace = violations[0].provenance_trace;
    ASSERT_EQ(trace.size(), 3u);
    EXPECT_EQ(trace[0].phase, "Origin");
    EXPECT_EQ(trace[1].phase, "Escape");
    EXPECT_EQ(trace[2].phase, "Conflict");
}

TEST_F(ProofCarryingFixture, EachPhaseHasRequiredFields) {
    // Req 31.2: source_location, ast_selector, description
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());

    for (const auto& phase : violations[0].provenance_trace) {
        EXPECT_FALSE(phase.source_location.empty()) << "Phase: " << phase.phase;
        EXPECT_FALSE(phase.ast_selector.empty()) << "Phase: " << phase.phase;
        EXPECT_FALSE(phase.description.empty()) << "Phase: " << phase.phase;
    }
}

TEST_F(ProofCarryingFixture, OriginIdentifiesAllocation) {
    // Req 31.3: Origin identifies allocation/binding
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());
    const auto& origin = violations[0].provenance_trace[0];
    EXPECT_EQ(origin.phase, "Origin");
    EXPECT_TRUE(origin.description.find("Allocation") != std::string::npos ||
                origin.description.find("binding") != std::string::npos);
}

TEST_F(ProofCarryingFixture, EscapeIdentifiesScopeEscape) {
    // Req 31.4: Escape identifies where reference left safe scope
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());
    const auto& escape = violations[0].provenance_trace[1];
    EXPECT_EQ(escape.phase, "Escape");
    EXPECT_TRUE(escape.description.find("escape") != std::string::npos);
}

TEST_F(ProofCarryingFixture, ConflictIdentifiesUnsafeAccess) {
    // Req 31.5: Conflict identifies the unsafe access
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());
    const auto& conflict = violations[0].provenance_trace[2];
    EXPECT_EQ(conflict.phase, "Conflict");
    EXPECT_TRUE(conflict.description.find("Unsafe") != std::string::npos ||
                conflict.description.find("unsafe") != std::string::npos);
}

TEST_F(ProofCarryingFixture, ProvenanceInAgentContext) {
    // Req 31.6: provenance_trace in SDF agent_context
    McpAdvancedToolProvider provider(model, dep_graph, vector_index);
    auto violations = provider.analyze_safety("unsafe.meld");
    ASSERT_FALSE(violations.empty());
    EXPECT_FALSE(violations[0].agent_context.empty());
    EXPECT_TRUE(violations[0].agent_context.find("provenance") != std::string::npos);
    EXPECT_TRUE(violations[0].agent_context.find("Origin") != std::string::npos);
    EXPECT_TRUE(violations[0].agent_context.find("Escape") != std::string::npos);
    EXPECT_TRUE(violations[0].agent_context.find("Conflict") != std::string::npos);
}

}  // namespace
