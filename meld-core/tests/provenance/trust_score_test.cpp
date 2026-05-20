#include <gtest/gtest.h>
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <chrono>
#include <thread>

using namespace meld::provenance;

class TrustScoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test nodes
        test_node_ = Value(std::make_shared<Integer>(42));
    }
    
    Value test_node_;
};

TEST_F(TrustScoreTest, CalculateBasicTrustScores) {
    // Test Human origin
    ProvenanceMetadata human_metadata;
    human_metadata.origin = OriginType::Human;
    double human_score = Provenance::calculateTrustScore(human_metadata);
    EXPECT_EQ(human_score, TrustLevels::VERIFIED);
    
    // Test Verified origin
    ProvenanceMetadata verified_metadata;
    verified_metadata.origin = OriginType::Verified;
    double verified_score = Provenance::calculateTrustScore(verified_metadata);
    EXPECT_EQ(verified_score, TrustLevels::VERIFIED);
    
    // Test Agent origin with confidence
    ProvenanceMetadata agent_metadata("gpt-4", 0.8);
    double agent_score = Provenance::calculateTrustScore(agent_metadata);
    EXPECT_EQ(agent_score, 0.8);
    
    // Test Agent origin without confidence
    ProvenanceMetadata agent_no_conf;
    agent_no_conf.origin = OriginType::Agent;
    double agent_default_score = Provenance::calculateTrustScore(agent_no_conf);
    EXPECT_EQ(agent_default_score, TrustLevels::MEDIUM_CONFIDENCE);
}

TEST_F(TrustScoreTest, TimeBasedDecayForAgentCode) {
    // Create agent metadata with old timestamp
    ProvenanceMetadata agent_metadata("gpt-4", 0.9);
    
    // Set creation time to 14 days ago (2 weeks)
    auto two_weeks_ago = std::chrono::system_clock::now() - std::chrono::hours(14 * 24);
    agent_metadata.creation_timestamp = two_weeks_ago;
    
    double decayed_score = Provenance::calculateTrustScore(agent_metadata);
    
    // Should be less than original confidence due to time decay
    EXPECT_LT(decayed_score, 0.9);
    EXPECT_GT(decayed_score, 0.5); // But not below 50% (max decay)
}

TEST_F(TrustScoreTest, VerificationExpiry) {
    // Create verified metadata with old verification timestamp
    ProvenanceMetadata verified_metadata;
    verified_metadata.origin = OriginType::Verified;
    verified_metadata.reviewer_email = "reviewer@example.com";
    
    // Set verification time to 45 days ago (expired)
    auto forty_five_days_ago = std::chrono::system_clock::now() - std::chrono::hours(45 * 24);
    verified_metadata.verification_timestamp = forty_five_days_ago;
    
    double expired_score = Provenance::calculateTrustScore(verified_metadata);
    
    // Should be less than full verification due to expiry
    EXPECT_LT(expired_score, TrustLevels::VERIFIED);
    EXPECT_GT(expired_score, 0.7); // But still reasonably high
}

TEST_F(TrustScoreTest, CustomParameterCalculation) {
    // Create agent metadata
    ProvenanceMetadata agent_metadata("gpt-4", 0.8);
    
    // Set creation time to 10 days ago
    auto ten_days_ago = std::chrono::system_clock::now() - std::chrono::hours(10 * 24);
    agent_metadata.creation_timestamp = ten_days_ago;
    
    // Test with custom parameters
    double custom_score = Provenance::calculateTrustScore(
        agent_metadata,
        0.05,  // Lower decay factor (5% per week)
        5,     // Decay starts after 5 days
        60     // Verification expires after 60 days
    );
    
    // Should be different from default calculation
    double default_score = Provenance::calculateTrustScore(agent_metadata);
    EXPECT_NE(custom_score, default_score);
}

TEST_F(TrustScoreTest, TrustThresholdChecking) {
    ProvenanceMetadata high_conf_metadata("gpt-4", 0.9);
    ProvenanceMetadata low_conf_metadata("gpt-3", 0.4);
    
    // Test threshold checking
    EXPECT_TRUE(Provenance::meetsTrustThreshold(high_conf_metadata, TrustLevels::HIGH_CONFIDENCE));
    EXPECT_FALSE(Provenance::meetsTrustThreshold(low_conf_metadata, TrustLevels::HIGH_CONFIDENCE));
    EXPECT_TRUE(Provenance::meetsTrustThreshold(low_conf_metadata, TrustLevels::LOW_CONFIDENCE));
}

TEST_F(TrustScoreTest, TrustLevelCategories) {
    EXPECT_EQ(Provenance::getTrustLevelCategory(1.0), "VERIFIED");
    EXPECT_EQ(Provenance::getTrustLevelCategory(0.95), "HIGH_CONFIDENCE");
    EXPECT_EQ(Provenance::getTrustLevelCategory(0.75), "MEDIUM_CONFIDENCE");
    EXPECT_EQ(Provenance::getTrustLevelCategory(0.55), "LOW_CONFIDENCE");
    EXPECT_EQ(Provenance::getTrustLevelCategory(0.3), "UNTRUSTED");
}

TEST_F(TrustScoreTest, ThresholdValidation) {
    EXPECT_TRUE(Provenance::isValidTrustThreshold(0.0));
    EXPECT_TRUE(Provenance::isValidTrustThreshold(0.5));
    EXPECT_TRUE(Provenance::isValidTrustThreshold(1.0));
    EXPECT_FALSE(Provenance::isValidTrustThreshold(-0.1));
    EXPECT_FALSE(Provenance::isValidTrustThreshold(1.1));
}

class TrustLevelEnforcerTest : public ::testing::Test {
protected:
    void SetUp() override {
        enforcer_ = std::make_unique<TrustLevelEnforcer>(TrustLevels::MEDIUM_CONFIDENCE);
        
        // Create test nodes with different trust levels
        high_trust_node_ = Value(std::make_shared<Integer>(1));
        medium_trust_node_ = Value(std::make_shared<Integer>(2));
        low_trust_node_ = Value(std::make_shared<Integer>(3));
        
        // Attach provenance metadata
        ProvenanceMetadata high_metadata("gpt-4", 0.95);
        high_trust_node_ = Provenance::attachProvenance(high_trust_node_, high_metadata);
        
        ProvenanceMetadata medium_metadata("gpt-4", 0.75);
        medium_trust_node_ = Provenance::attachProvenance(medium_trust_node_, medium_metadata);
        
        ProvenanceMetadata low_metadata("gpt-3", 0.4);
        low_trust_node_ = Provenance::attachProvenance(low_trust_node_, low_metadata);
    }
    
    std::unique_ptr<TrustLevelEnforcer> enforcer_;
    Value high_trust_node_;
    Value medium_trust_node_;
    Value low_trust_node_;
};

TEST_F(TrustLevelEnforcerTest, CheckIndividualNodes) {
    EXPECT_TRUE(enforcer_->checkNode(high_trust_node_));
    EXPECT_TRUE(enforcer_->checkNode(medium_trust_node_));
    EXPECT_FALSE(enforcer_->checkNode(low_trust_node_));
}

TEST_F(TrustLevelEnforcerTest, FindViolations) {
    std::vector<Value> nodes = {high_trust_node_, medium_trust_node_, low_trust_node_};
    auto violations = enforcer_->findViolations(nodes);
    
    EXPECT_EQ(violations.size(), 1);
    // The low trust node should be the only violation
}

TEST_F(TrustLevelEnforcerTest, GenerateWarnings) {
    std::vector<Value> violations = {low_trust_node_};
    auto warnings = enforcer_->generateTrustViolationWarnings(violations);
    
    EXPECT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].code, "TRUST001");
    EXPECT_EQ(warnings[0].level, CompilerWarning::Level::Error);
    EXPECT_FALSE(warnings[0].suggestions.empty());
}

TEST_F(TrustLevelEnforcerTest, AnalyzeTrustLevels) {
    std::vector<Value> nodes = {high_trust_node_, medium_trust_node_, low_trust_node_};
    auto stats = enforcer_->analyzeTrustLevels(nodes);
    
    EXPECT_EQ(stats.total_nodes, 3);
    EXPECT_EQ(stats.violations, 1);
    EXPECT_GT(stats.average_trust_score, 0.0);
    EXPECT_LT(stats.average_trust_score, 1.0);
    EXPECT_EQ(stats.high_confidence_nodes, 1);
    EXPECT_EQ(stats.medium_confidence_nodes, 1);
    EXPECT_EQ(stats.low_confidence_nodes, 1);
}

TEST_F(TrustLevelEnforcerTest, SetMinimumTrustLevel) {
    // Initially set to medium confidence
    EXPECT_EQ(enforcer_->getMinimumTrustLevel(), TrustLevels::MEDIUM_CONFIDENCE);
    
    // Change to high confidence
    enforcer_->setMinimumTrustLevel(TrustLevels::HIGH_CONFIDENCE);
    EXPECT_EQ(enforcer_->getMinimumTrustLevel(), TrustLevels::HIGH_CONFIDENCE);
    
    // Now medium trust node should also be a violation
    std::vector<Value> nodes = {high_trust_node_, medium_trust_node_, low_trust_node_};
    auto violations = enforcer_->findViolations(nodes);
    EXPECT_EQ(violations.size(), 2); // medium and low trust nodes
}