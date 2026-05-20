#include <gtest/gtest.h>
#include "meld/verification/provenance_verification.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"

namespace meld {
namespace verification {
namespace test {

class ProvenanceVerificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test node
        test_node_ = Value(std::make_shared<Symbol>("test_function"));
        
        // Create a verifier
        verifier_ = std::make_unique<ProvenanceVerifier>("reviewer@example.com", "Test Reviewer");
    }
    
    Value test_node_;
    std::unique_ptr<ProvenanceVerifier> verifier_;
};

TEST_F(ProvenanceVerificationTest, MarkAsVerifiedCreatesProvenanceMetadata) {
    // Initially, node should have no provenance
    EXPECT_FALSE(provenance::Provenance::hasProvenance(test_node_));
    
    // Mark as verified
    Value verified_node = verifier_->markAsVerified(test_node_, "Manual review completed");
    
    // Should now have provenance metadata
    EXPECT_TRUE(provenance::Provenance::hasProvenance(verified_node));
    
    // Check the metadata
    auto metadata = provenance::Provenance::getProvenance(verified_node);
    ASSERT_TRUE(metadata.has_value());
    
    EXPECT_EQ(metadata->origin, provenance::OriginType::Verified);
    EXPECT_EQ(metadata->reviewer_email.value(), "reviewer@example.com");
    EXPECT_TRUE(metadata->verification_timestamp.has_value());
    EXPECT_EQ(metadata->trust_score, provenance::TrustLevels::VERIFIED);
}

TEST_F(ProvenanceVerificationTest, MarkAsVerifiedUpdatesExistingMetadata) {
    // Create initial AI-generated metadata
    provenance::ProvenanceMetadata initial_metadata("gpt-4", 0.8, "test-blueprint");
    Value ai_node = provenance::Provenance::attachProvenance(test_node_, initial_metadata);
    
    // Verify initial state
    auto initial = provenance::Provenance::getProvenance(ai_node);
    ASSERT_TRUE(initial.has_value());
    EXPECT_EQ(initial->origin, provenance::OriginType::Agent);
    EXPECT_EQ(initial->agent_model.value(), "gpt-4");
    EXPECT_FALSE(initial->reviewer_email.has_value());
    
    // Mark as verified
    Value verified_node = verifier_->markAsVerified(ai_node, "AI code reviewed and approved");
    
    // Check updated metadata
    auto verified = provenance::Provenance::getProvenance(verified_node);
    ASSERT_TRUE(verified.has_value());
    
    EXPECT_EQ(verified->origin, provenance::OriginType::Verified);
    EXPECT_EQ(verified->reviewer_email.value(), "reviewer@example.com");
    EXPECT_TRUE(verified->verification_timestamp.has_value());
    EXPECT_EQ(verified->trust_score, provenance::TrustLevels::VERIFIED);
    
    // Original AI metadata should be preserved
    EXPECT_EQ(verified->agent_model.value(), "gpt-4");
    EXPECT_EQ(verified->confidence_score.value(), 0.8);
    EXPECT_EQ(verified->blueprint_id.value(), "test-blueprint");
}

TEST_F(ProvenanceVerificationTest, IsVerifiedDetectsVerificationStatus) {
    // Initially not verified
    EXPECT_FALSE(verifier_->isVerified(test_node_));
    
    // Mark as verified
    Value verified_node = verifier_->markAsVerified(test_node_);
    
    // Should now be verified
    EXPECT_TRUE(verifier_->isVerified(verified_node));
}

TEST_F(ProvenanceVerificationTest, GetVerificationInfoReturnsCorrectData) {
    // Mark as verified with notes
    Value verified_node = verifier_->markAsVerified(test_node_, "Thorough security review");
    
    // Get verification info
    auto info = verifier_->getVerificationInfo(verified_node);
    ASSERT_TRUE(info.has_value());
    
    EXPECT_EQ(info->reviewer_email, "reviewer@example.com");
    EXPECT_EQ(info->reviewer_name, "Test Reviewer");
    EXPECT_TRUE(info->is_active);
    
    // Timestamp should be recent (within last minute)
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - info->verification_timestamp);
    EXPECT_LT(diff.count(), 60);
}

TEST_F(ProvenanceVerificationTest, RevokeVerificationRestoresOriginalState) {
    // Create AI-generated node
    provenance::ProvenanceMetadata ai_metadata("gpt-4", 0.7);
    Value ai_node = provenance::Provenance::attachProvenance(test_node_, ai_metadata);
    
    // Mark as verified
    Value verified_node = verifier_->markAsVerified(ai_node);
    EXPECT_TRUE(verifier_->isVerified(verified_node));
    
    // Revoke verification
    Value revoked_node = verifier_->revokeVerification(verified_node, "Found security issue");
    
    // Should no longer be verified
    EXPECT_FALSE(verifier_->isVerified(revoked_node));
    
    // Should restore to Agent origin
    auto metadata = provenance::Provenance::getProvenance(revoked_node);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->origin, provenance::OriginType::Agent);
    EXPECT_FALSE(metadata->reviewer_email.has_value());
    EXPECT_FALSE(metadata->verification_timestamp.has_value());
}

TEST_F(ProvenanceVerificationTest, BatchVerificationWorksCorrectly) {
    // Create multiple test nodes
    std::vector<Value> nodes;
    for (int i = 0; i < 3; ++i) {
        nodes.push_back(Value(std::make_shared<Symbol>("test_function_" + std::to_string(i))));
    }
    
    // Mark all as verified
    std::vector<Value> verified_nodes = verifier_->markAsVerified(nodes, "Batch review");
    
    EXPECT_EQ(verified_nodes.size(), 3);
    
    // All should be verified
    for (const auto& node : verified_nodes) {
        EXPECT_TRUE(verifier_->isVerified(node));
        
        auto metadata = provenance::Provenance::getProvenance(node);
        ASSERT_TRUE(metadata.has_value());
        EXPECT_EQ(metadata->reviewer_email.value(), "reviewer@example.com");
    }
}

TEST_F(ProvenanceVerificationTest, VerificationAuditTrailIsRecorded) {
    // Mark as verified
    Value verified_node = verifier_->markAsVerified(test_node_, "Security audit passed");
    
    // Check audit trail
    auto trail = VerificationAudit::getAuditTrail(verified_node);
    EXPECT_GE(trail.size(), 1);
    
    // Find the verification entry
    bool found_verification = false;
    for (const auto& entry : trail) {
        if (entry.event_type == "verified" && entry.reviewer_email == "reviewer@example.com") {
            found_verification = true;
            EXPECT_EQ(entry.details.verification_type, "manual");
            EXPECT_EQ(entry.details.verification_level, "basic");
            break;
        }
    }
    EXPECT_TRUE(found_verification);
}

TEST_F(ProvenanceVerificationTest, VerificationStatsAreCalculated) {
    // Mark several nodes as verified
    for (int i = 0; i < 5; ++i) {
        Value node = Value(std::make_shared<Symbol>("func_" + std::to_string(i)));
        verifier_->markAsVerified(node, "Test verification " + std::to_string(i));
    }
    
    // Get stats
    auto stats = VerificationAudit::getStats();
    
    EXPECT_GE(stats.total_verifications, 5);
    EXPECT_GE(stats.active_verifications, 5);
    EXPECT_EQ(stats.revoked_verifications, 0);
    
    // Should include our reviewer in top reviewers
    bool found_reviewer = false;
    for (const auto& reviewer : stats.top_reviewers) {
        if (reviewer == "reviewer@example.com") {
            found_reviewer = true;
            break;
        }
    }
    EXPECT_TRUE(found_reviewer);
}

} // namespace test
} // namespace verification
} // namespace meld