#include <gtest/gtest.h>
#include "meld/api/meld_binary.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <chrono>
#include <filesystem>

using namespace meld::api;
using namespace meld::provenance;
using namespace meld::kernel;

namespace {
// Helper: add a node to the graph using a dummy identifier expression
NodeId addTestNode(SemanticGraph& g, const std::string& name) {
    meld::parser::ast::identifier id;
    id.name = name;
    meld::parser::ast::expression expr(id);
    return g.addNode(NodeType::IDENTIFIER, expr, SourceLocation("test.meld", 0, 0, 0));
}
}

class MeldBinaryProvenanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        test_dir_ = std::filesystem::temp_directory_path() / "meld_binary_provenance_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::filesystem::path test_dir_;
};

TEST_F(MeldBinaryProvenanceTest, PreservesProvenanceInBinaryFormat) {
    // Create test nodes with different provenance metadata
    Value human_node = createSymbol("human_function");
    Value agent_node = createSymbol("agent_function");
    Value verified_node = createSymbol("verified_function");
    
    // Attach different types of provenance metadata
    ProvenanceMetadata human_metadata("alice@example.com", "abc123");
    ProvenanceMetadata agent_metadata("gpt-4", 0.85, "blueprint-001");
    ProvenanceMetadata verified_metadata = agent_metadata;
    
    human_node = Provenance::attachProvenance(human_node, human_metadata);
    agent_node = Provenance::attachProvenance(agent_node, agent_metadata);
    verified_node = Provenance::markAsVerified(
        Provenance::attachProvenance(verified_node, agent_metadata), 
        "reviewer@example.com"
    );
    
    // Create a semantic graph with these nodes
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "human_node");
    addTestNode(*graph, "agent_node");
    addTestNode(*graph, "verified_node");
    
    // Create MELD-B binary from the graph
    auto binary = MeldBinary::fromSemanticGraph(graph);
    ASSERT_NE(binary, nullptr);
    
    // Save to file and reload
    std::string test_file = (test_dir_ / "test_provenance.mldb").string();
    binary->save(test_file);
    
    auto loaded_binary = MeldBinary::load(test_file);
    ASSERT_NE(loaded_binary, nullptr);
    
    // Verify provenance table was preserved
    const auto& provenance_table = loaded_binary->getProvenanceTable();
    EXPECT_EQ(provenance_table.size(), 3);
    
    // Check that we have one entry of each origin type
    bool found_human = false, found_agent = false, found_verified = false;
    
    for (const auto& entry : provenance_table) {
        if (entry.origin_type == 0) { // Human
            found_human = true;
            EXPECT_EQ(entry.getAuthorEmail(loaded_binary->getStringPool()), "alice@example.com");
            EXPECT_EQ(entry.trust_score, 1.0);
        } else if (entry.origin_type == 1) { // Agent
            found_agent = true;
            EXPECT_EQ(entry.getAgentModel(loaded_binary->getStringPool()), "gpt-4");
            EXPECT_DOUBLE_EQ(entry.confidence_score, 0.85);
            EXPECT_EQ(entry.getBlueprintId(loaded_binary->getStringPool()), "blueprint-001");
        } else if (entry.origin_type == 2) { // Verified
            found_verified = true;
            EXPECT_EQ(entry.getReviewerEmail(loaded_binary->getStringPool()), "reviewer@example.com");
            EXPECT_GT(entry.verification_timestamp, 0);
        }
    }
    
    EXPECT_TRUE(found_human);
    EXPECT_TRUE(found_agent);
    EXPECT_TRUE(found_verified);
}

TEST_F(MeldBinaryProvenanceTest, QueryByOriginType) {
    // Create test nodes with different origins
    Value human_node = createSymbol("human_func");
    Value agent_node = createSymbol("agent_func");
    Value verified_node = createSymbol("verified_func");
    
    ProvenanceMetadata human_metadata("dev@example.com");
    ProvenanceMetadata agent_metadata("claude-3", 0.9);
    ProvenanceMetadata verified_metadata = agent_metadata;
    
    human_node = Provenance::attachProvenance(human_node, human_metadata);
    agent_node = Provenance::attachProvenance(agent_node, agent_metadata);
    verified_node = Provenance::markAsVerified(
        Provenance::attachProvenance(verified_node, agent_metadata), 
        "reviewer@example.com"
    );
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "human_node");
    addTestNode(*graph, "agent_node");
    addTestNode(*graph, "verified_node");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Query by origin type
    auto human_nodes = binary->queryByOrigin(0); // Human
    auto agent_nodes = binary->queryByOrigin(1); // Agent
    auto verified_nodes = binary->queryByOrigin(2); // Verified
    
    EXPECT_EQ(human_nodes.size(), 1);
    EXPECT_EQ(agent_nodes.size(), 1);
    EXPECT_EQ(verified_nodes.size(), 1);
}

TEST_F(MeldBinaryProvenanceTest, QueryByTrustLevel) {
    // Create nodes with different trust levels
    Value high_trust_node = createSymbol("high_trust");
    Value medium_trust_node = createSymbol("medium_trust");
    Value low_trust_node = createSymbol("low_trust");
    
    ProvenanceMetadata high_trust("dev@example.com"); // Human = 1.0 trust
    ProvenanceMetadata medium_trust("gpt-4", 0.8);
    ProvenanceMetadata low_trust("gpt-3.5", 0.4);
    
    high_trust_node = Provenance::attachProvenance(high_trust_node, high_trust);
    medium_trust_node = Provenance::attachProvenance(medium_trust_node, medium_trust);
    low_trust_node = Provenance::attachProvenance(low_trust_node, low_trust);
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "high_trust_node");
    addTestNode(*graph, "medium_trust_node");
    addTestNode(*graph, "low_trust_node");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Query by trust level
    auto high_trust_results = binary->queryByTrustLevel(0.9); // Should find 1 (human)
    auto medium_trust_results = binary->queryByTrustLevel(0.7); // Should find 2 (human + high confidence agent)
    auto all_results = binary->queryByTrustLevel(0.0); // Should find all 3
    
    EXPECT_EQ(high_trust_results.size(), 1);
    EXPECT_EQ(medium_trust_results.size(), 2);
    EXPECT_EQ(all_results.size(), 3);
}

TEST_F(MeldBinaryProvenanceTest, QueryByAuthor) {
    // Create nodes with different authors
    Value alice_node = createSymbol("alice_func");
    Value bob_node = createSymbol("bob_func");
    Value agent_node = createSymbol("agent_func");
    
    ProvenanceMetadata alice_metadata("alice@example.com");
    ProvenanceMetadata bob_metadata("bob@example.com");
    ProvenanceMetadata agent_metadata("gpt-4", 0.8);
    
    alice_node = Provenance::attachProvenance(alice_node, alice_metadata);
    bob_node = Provenance::attachProvenance(bob_node, bob_metadata);
    agent_node = Provenance::attachProvenance(agent_node, agent_metadata);
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "alice_node");
    addTestNode(*graph, "bob_node");
    addTestNode(*graph, "agent_node");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Query by author
    auto alice_results = binary->queryByAuthor("alice@example.com");
    auto bob_results = binary->queryByAuthor("bob@example.com");
    auto nonexistent_results = binary->queryByAuthor("charlie@example.com");
    
    EXPECT_EQ(alice_results.size(), 1);
    EXPECT_EQ(bob_results.size(), 1);
    EXPECT_EQ(nonexistent_results.size(), 0);
}

TEST_F(MeldBinaryProvenanceTest, QueryByAgentModel) {
    // Create nodes with different agent models
    Value gpt4_node = createSymbol("gpt4_func");
    Value claude_node = createSymbol("claude_func");
    Value human_node = createSymbol("human_func");
    
    ProvenanceMetadata gpt4_metadata("gpt-4", 0.9);
    ProvenanceMetadata claude_metadata("claude-3", 0.85);
    ProvenanceMetadata human_metadata("dev@example.com");
    
    gpt4_node = Provenance::attachProvenance(gpt4_node, gpt4_metadata);
    claude_node = Provenance::attachProvenance(claude_node, claude_metadata);
    human_node = Provenance::attachProvenance(human_node, human_metadata);
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "gpt4_node");
    addTestNode(*graph, "claude_node");
    addTestNode(*graph, "human_node");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Query by agent model
    auto gpt4_results = binary->queryByAgentModel("gpt-4");
    auto claude_results = binary->queryByAgentModel("claude-3");
    auto nonexistent_results = binary->queryByAgentModel("gemini");
    
    EXPECT_EQ(gpt4_results.size(), 1);
    EXPECT_EQ(claude_results.size(), 1);
    EXPECT_EQ(nonexistent_results.size(), 0);
}

TEST_F(MeldBinaryProvenanceTest, QueryVerifiedOnly) {
    // Create mix of verified and unverified nodes
    Value verified_node = createSymbol("verified_func");
    Value unverified_agent_node = createSymbol("unverified_agent_func");
    Value human_node = createSymbol("human_func");
    
    ProvenanceMetadata agent_metadata("gpt-4", 0.8);
    ProvenanceMetadata human_metadata("dev@example.com");
    
    verified_node = Provenance::markAsVerified(
        Provenance::attachProvenance(verified_node, agent_metadata), 
        "reviewer@example.com"
    );
    unverified_agent_node = Provenance::attachProvenance(unverified_agent_node, agent_metadata);
    human_node = Provenance::attachProvenance(human_node, human_metadata);
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "verified_node");
    addTestNode(*graph, "unverified_agent_node");
    addTestNode(*graph, "human_node");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Query verified only - should find verified agent code but not unverified
    auto verified_results = binary->queryVerifiedOnly();
    
    EXPECT_EQ(verified_results.size(), 1);
    
    // Verify it's the correct node by checking provenance
    auto provenance_entries = binary->getProvenanceForNodes(verified_results);
    EXPECT_EQ(provenance_entries.size(), 1);
    EXPECT_GT(provenance_entries[0].verification_timestamp, 0);
}

TEST_F(MeldBinaryProvenanceTest, GetProvenanceForNodes) {
    // Create test nodes
    Value node1 = createSymbol("func1");
    Value node2 = createSymbol("func2");
    
    ProvenanceMetadata metadata1("alice@example.com");
    ProvenanceMetadata metadata2("gpt-4", 0.9);
    
    node1 = Provenance::attachProvenance(node1, metadata1);
    node2 = Provenance::attachProvenance(node2, metadata2);
    
    auto graph = std::make_shared<SemanticGraph>();
    addTestNode(*graph, "node1");
    addTestNode(*graph, "node2");
    
    auto binary = MeldBinary::fromSemanticGraph(graph);
    
    // Get all nodes and their provenance
    auto all_nodes = binary->queryByTrustLevel(0.0); // Get all nodes
    auto provenance_entries = binary->getProvenanceForNodes(all_nodes);
    
    EXPECT_EQ(provenance_entries.size(), 2);
    
    // Verify we have one human and one agent entry
    bool found_human = false, found_agent = false;
    for (const auto& entry : provenance_entries) {
        if (entry.origin_type == 0) found_human = true;
        if (entry.origin_type == 1) found_agent = true;
    }
    
    EXPECT_TRUE(found_human);
    EXPECT_TRUE(found_agent);
}

// String pool test removed - requires access to private MeldBinary internals

TEST_F(MeldBinaryProvenanceTest, DISABLED_StringPoolIntegration) {
    GTEST_SKIP() << "Requires friend access to MeldBinary::string_pool_";
}