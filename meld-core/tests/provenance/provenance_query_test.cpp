#include <gtest/gtest.h>
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <vector>
#include <chrono>

using namespace meld;
using namespace meld::kernel;
using namespace meld::provenance;

class ProvenanceQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test nodes with different provenance metadata
        human_node = Value(std::make_shared<Symbol>("human_function"));
        agent_node = Value(std::make_shared<Symbol>("agent_function"));
        verified_node = Value(std::make_shared<Symbol>("verified_function"));
        no_provenance_node = Value(std::make_shared<Symbol>("no_provenance_function"));
        
        // Attach different types of provenance metadata
        ProvenanceMetadata human_metadata("alice@example.com", "abc123");
        human_node = Provenance::attachProvenance(human_node, human_metadata);
        
        ProvenanceMetadata agent_metadata("gpt-4", 0.85, "blueprint-123");
        agent_node = Provenance::attachProvenance(agent_node, agent_metadata);
        
        ProvenanceMetadata verified_metadata("gpt-4", 0.75, "blueprint-456");
        verified_node = Provenance::attachProvenance(verified_node, verified_metadata);
        verified_node = Provenance::markAsVerified(verified_node, "reviewer@example.com");
        
        // Create test collection
        all_nodes = {human_node, agent_node, verified_node, no_provenance_node};
    }
    
    Value human_node;
    Value agent_node;
    Value verified_node;
    Value no_provenance_node;
    std::vector<Value> all_nodes;
};

TEST_F(ProvenanceQueryTest, QueryByOriginType) {
    // Test filtering by Human origin
    auto filter = Provenance::QueryFilter().withOrigin(OriginType::Human);
    auto human_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(human_results.size(), 1);
    EXPECT_EQ(human_results[0], human_node);
    
    // Test filtering by Agent origin
    filter = Provenance::QueryFilter().withOrigin(OriginType::Agent);
    auto agent_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(agent_results.size(), 1);
    EXPECT_EQ(agent_results[0], agent_node);
    
    // Test filtering by Verified origin
    filter = Provenance::QueryFilter().withOrigin(OriginType::Verified);
    auto verified_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(verified_results.size(), 1);
    EXPECT_EQ(verified_results[0], verified_node);
}

TEST_F(ProvenanceQueryTest, QueryByConfidence) {
    // Test minimum confidence filter
    auto filter = Provenance::QueryFilter().withMinConfidence(0.8);
    auto high_confidence_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(high_confidence_results.size(), 1);
    EXPECT_EQ(high_confidence_results[0], agent_node);
    
    // Test maximum confidence filter
    filter = Provenance::QueryFilter().withMaxConfidence(0.8);
    auto low_confidence_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(low_confidence_results.size(), 1);
    EXPECT_EQ(low_confidence_results[0], verified_node);
    
    // Test confidence range
    filter = Provenance::QueryFilter().withMinConfidence(0.7).withMaxConfidence(0.9);
    auto range_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(range_results.size(), 2);
    // Should include both agent_node (0.85) and verified_node (0.75)
}

TEST_F(ProvenanceQueryTest, QueryByTrustLevel) {
    // Test minimum trust level filter
    auto filter = Provenance::QueryFilter().withMinTrustLevel(0.9);
    auto high_trust_results = Provenance::query(all_nodes, filter);
    
    // Should include human_node (1.0) and verified_node (1.0)
    EXPECT_EQ(high_trust_results.size(), 2);
    
    // Test lower trust level
    filter = Provenance::QueryFilter().withMinTrustLevel(0.5);
    auto medium_trust_results = Provenance::query(all_nodes, filter);
    
    // Should include all nodes with provenance
    EXPECT_EQ(medium_trust_results.size(), 3);
}

TEST_F(ProvenanceQueryTest, QueryByAgentModel) {
    // Test filtering by specific agent model
    auto filter = Provenance::QueryFilter().withAgentModel("gpt-4");
    auto gpt4_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(gpt4_results.size(), 1);
    EXPECT_EQ(gpt4_results[0], agent_node);
    
    // Test filtering by non-existent model
    filter = Provenance::QueryFilter().withAgentModel("claude-3");
    auto claude_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(claude_results.size(), 0);
}

TEST_F(ProvenanceQueryTest, QueryByAuthor) {
    // Test filtering by author email
    auto filter = Provenance::QueryFilter().withAuthor("alice@example.com");
    auto alice_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(alice_results.size(), 1);
    EXPECT_EQ(alice_results[0], human_node);
    
    // Test filtering by non-existent author
    filter = Provenance::QueryFilter().withAuthor("bob@example.com");
    auto bob_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(bob_results.size(), 0);
}

TEST_F(ProvenanceQueryTest, QueryVerifiedOnly) {
    // Test filtering for verified code only
    auto filter = Provenance::QueryFilter().verifiedOnly();
    auto verified_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(verified_results.size(), 1);
    EXPECT_EQ(verified_results[0], verified_node);
}

TEST_F(ProvenanceQueryTest, CombinedFilters) {
    // Test combining multiple filters
    auto filter = Provenance::QueryFilter()
        .withOrigin(OriginType::Agent)
        .withMinConfidence(0.8);
    auto combined_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(combined_results.size(), 1);
    EXPECT_EQ(combined_results[0], agent_node);
    
    // Test filters that should return no results
    filter = Provenance::QueryFilter()
        .withOrigin(OriginType::Human)
        .withMinConfidence(0.5); // Human code doesn't have confidence scores
    auto no_results = Provenance::query(all_nodes, filter);
    
    EXPECT_EQ(no_results.size(), 0);
}

TEST_F(ProvenanceQueryTest, SingleNodeMatching) {
    // Test single node matching
    auto filter = Provenance::QueryFilter().withOrigin(OriginType::Human);
    
    EXPECT_TRUE(Provenance::matchesQuery(human_node, filter));
    EXPECT_FALSE(Provenance::matchesQuery(agent_node, filter));
    EXPECT_FALSE(Provenance::matchesQuery(verified_node, filter));
    EXPECT_FALSE(Provenance::matchesQuery(no_provenance_node, filter));
}

TEST_F(ProvenanceQueryTest, NoProvenanceHandling) {
    // Test behavior with nodes that have no provenance metadata
    auto empty_filter = Provenance::QueryFilter();
    auto all_results = Provenance::query(all_nodes, empty_filter);
    
    // Empty filter should match all nodes, including those without provenance
    EXPECT_EQ(all_results.size(), 4);
    
    // Any specific filter should exclude nodes without provenance
    auto specific_filter = Provenance::QueryFilter().withOrigin(OriginType::Human);
    auto filtered_results = Provenance::query(all_nodes, specific_filter);
    
    EXPECT_EQ(filtered_results.size(), 1);
    EXPECT_EQ(filtered_results[0], human_node);
}

TEST_F(ProvenanceQueryTest, FluentAPIUsage) {
    // Test the fluent API builder pattern
    auto complex_filter = Provenance::QueryFilter()
        .withOrigin(OriginType::Agent)
        .withMinConfidence(0.7)
        .withMaxConfidence(0.9)
        .withMinTrustLevel(0.5)
        .withAgentModel("gpt-4");
    
    auto results = Provenance::query(all_nodes, complex_filter);
    
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], agent_node);
}