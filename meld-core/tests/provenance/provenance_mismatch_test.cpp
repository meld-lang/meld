#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace meld::provenance;

class ProvenanceMismatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test node with some basic structure
        test_node_ = Value(std::make_shared<Symbol>("test_function"));
        
        // Sample blueprint and code content
        original_blueprint_ = R"(
            @blueprint {
                summary: "Calculate the sum of two integers",
                rules: ["Input must be valid integers", "Return type must be int"],
                examples: [
                    {input: [1, 2], output: 3},
                    {input: [0, 0], output: 0}
                ]
            }
        )";
        
        original_code_ = R"(
            fnc add(a: int, b: int) -> int {
                return a + b;
            }
        )";
        
        modified_blueprint_ = R"(
            @blueprint {
                summary: "Calculate the sum of two integers with validation",
                rules: ["Input must be valid integers", "Return type must be int", "Handle overflow"],
                examples: [
                    {input: [1, 2], output: 3},
                    {input: [0, 0], output: 0},
                    {input: [MAX_INT, 1], output: Error}
                ]
            }
        )";
        
        modified_code_ = R"(
            fnc add(a: int, b: int) -> int {
                // Added validation
                if (a > 0 && b > MAX_INT - a) {
                    throw OverflowError("Integer overflow");
                }
                return a + b;
            }
        )";
    }
    
    Value test_node_;
    std::string original_blueprint_;
    std::string original_code_;
    std::string modified_blueprint_;
    std::string modified_code_;
};

TEST_F(ProvenanceMismatchTest, DetectCodeChangedBlueprintStale) {
    // Setup: Attach original hashes
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    // Test: Analyze with modified code but original blueprint
    auto report = Provenance::analyzeProvenance(node_with_hashes, original_blueprint_, modified_code_);
    
    // Verify: Should detect code changed but blueprint stale
    EXPECT_FALSE(report.mismatch_types.empty());
    EXPECT_TRUE(std::find(report.mismatch_types.begin(), report.mismatch_types.end(), 
                         MismatchType::CodeChangedBlueprintStale) != report.mismatch_types.end());
    EXPECT_EQ(report.severity, ProvenanceMismatchReport::Severity::Warning);
    EXPECT_FALSE(report.description.empty());
}

TEST_F(ProvenanceMismatchTest, DetectBlueprintChangedCodeStale) {
    // Setup: Attach original hashes
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    // Test: Analyze with modified blueprint but original code
    auto report = Provenance::analyzeProvenance(node_with_hashes, modified_blueprint_, original_code_);
    
    // Verify: Should detect blueprint changed but code stale
    EXPECT_FALSE(report.mismatch_types.empty());
    EXPECT_TRUE(std::find(report.mismatch_types.begin(), report.mismatch_types.end(), 
                         MismatchType::BlueprintChangedCodeStale) != report.mismatch_types.end());
    EXPECT_EQ(report.severity, ProvenanceMismatchReport::Severity::Warning);
}

TEST_F(ProvenanceMismatchTest, DetectBothChanged) {
    // Setup: Attach original hashes
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    // Test: Analyze with both blueprint and code modified
    auto report = Provenance::analyzeProvenance(node_with_hashes, modified_blueprint_, modified_code_);
    
    // Verify: Should detect both changed
    EXPECT_FALSE(report.mismatch_types.empty());
    EXPECT_TRUE(std::find(report.mismatch_types.begin(), report.mismatch_types.end(), 
                         MismatchType::BothChanged) != report.mismatch_types.end());
    EXPECT_EQ(report.severity, ProvenanceMismatchReport::Severity::Warning);
}

TEST_F(ProvenanceMismatchTest, DetectStaleAICode) {
    // Setup: Create old AI-generated code
    ProvenanceMetadata metadata("gpt-3.5", 0.8, "test-blueprint");
    // Set creation time to 8 days ago
    metadata.creation_timestamp = std::chrono::system_clock::now() - std::chrono::hours(24 * 8);
    
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    
    // Test: Analyze the old code
    auto report = Provenance::analyzeProvenance(node_with_metadata, original_blueprint_, original_code_);
    
    // Verify: Should detect stale AI code
    EXPECT_TRUE(std::find(report.mismatch_types.begin(), report.mismatch_types.end(), 
                         MismatchType::StaleAICode) != report.mismatch_types.end());
    EXPECT_EQ(report.severity, ProvenanceMismatchReport::Severity::Warning);
}

TEST_F(ProvenanceMismatchTest, DetectMissingHashes) {
    // Setup: Node with metadata but no content hashes
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    
    // Test: Analyze without stored hashes
    auto report = Provenance::analyzeProvenance(node_with_metadata, original_blueprint_, original_code_);
    
    // Verify: Should detect missing hashes
    EXPECT_TRUE(std::find(report.mismatch_types.begin(), report.mismatch_types.end(), 
                         MismatchType::HashMissing) != report.mismatch_types.end());
}

TEST_F(ProvenanceMismatchTest, GenerateCompilerWarnings) {
    // Setup: Create a report with multiple mismatch types
    ProvenanceMismatchReport report(test_node_);
    report.mismatch_types = {
        MismatchType::CodeChangedBlueprintStale,
        MismatchType::StaleAICode,
        MismatchType::HashMissing
    };
    
    // Test: Generate warnings
    auto warnings = Provenance::generateMismatchWarnings(report);
    
    // Verify: Should generate appropriate warnings
    EXPECT_EQ(warnings.size(), 3);
    
    // Check for specific warning codes
    std::vector<std::string> warning_codes;
    for (const auto& warning : warnings) {
        warning_codes.push_back(warning.code);
        EXPECT_FALSE(warning.message.empty());
        EXPECT_FALSE(warning.suggestions.empty());
    }
    
    EXPECT_TRUE(std::find(warning_codes.begin(), warning_codes.end(), "PROV001") != warning_codes.end());
    EXPECT_TRUE(std::find(warning_codes.begin(), warning_codes.end(), "PROV005") != warning_codes.end());
    EXPECT_TRUE(std::find(warning_codes.begin(), warning_codes.end(), "PROV004") != warning_codes.end());
}

TEST_F(ProvenanceMismatchTest, GenerateResolutionSuggestions) {
    // Setup: Create a report with mismatch types
    ProvenanceMismatchReport report(test_node_);
    report.mismatch_types = {
        MismatchType::CodeChangedBlueprintStale,
        MismatchType::BlueprintChangedCodeStale
    };
    
    // Test: Generate suggestions
    auto suggestions = Provenance::suggestResolutions(report);
    
    // Verify: Should generate appropriate suggestions
    EXPECT_FALSE(suggestions.empty());
    
    // Check that suggestions are sorted by priority
    for (size_t i = 1; i < suggestions.size(); ++i) {
        EXPECT_GE(suggestions[i].priority, suggestions[i-1].priority);
    }
    
    // Check for specific action types
    std::vector<ResolutionSuggestion::ActionType> action_types;
    for (const auto& suggestion : suggestions) {
        action_types.push_back(suggestion.action);
        EXPECT_FALSE(suggestion.description.empty());
    }
    
    EXPECT_TRUE(std::find(action_types.begin(), action_types.end(), 
                         ResolutionSuggestion::ActionType::UpdateBlueprint) != action_types.end());
    EXPECT_TRUE(std::find(action_types.begin(), action_types.end(), 
                         ResolutionSuggestion::ActionType::RegenerateCode) != action_types.end());
}

TEST_F(ProvenanceMismatchTest, HashFunctions) {
    // Test blueprint hashing (should normalize whitespace)
    std::string blueprint1 = "summary: 'test'";
    std::string blueprint2 = "  summary:   'test'  ";
    
    EXPECT_EQ(Provenance::hashBlueprint(blueprint1), Provenance::hashBlueprint(blueprint2));
    
    // Test code hashing (should normalize formatting)
    std::string code1 = "fnc test() { return 42; }";
    std::string code2 = "fnc test(){return 42;}";
    
    EXPECT_EQ(Provenance::hashCode(code1), Provenance::hashCode(code2));
    
    // Test that different content produces different hashes
    EXPECT_NE(Provenance::hashBlueprint("test1"), Provenance::hashBlueprint("test2"));
    EXPECT_NE(Provenance::hashCode("code1"), Provenance::hashCode("code2"));
}

TEST_F(ProvenanceMismatchTest, ContentHashAttachment) {
    // Test attaching content hashes
    Value node_with_hashes = Provenance::attachContentHashes(test_node_, original_blueprint_, original_code_);
    
    // Verify hashes were attached
    EXPECT_TRUE(meta_has(node_with_hashes, Provenance::BLUEPRINT_HASH_KEY));
    EXPECT_TRUE(meta_has(node_with_hashes, Provenance::CODE_HASH_KEY));
    
    // Test change detection
    EXPECT_FALSE(Provenance::hasContentChanged(node_with_hashes, original_blueprint_, Provenance::BLUEPRINT_HASH_KEY));
    EXPECT_TRUE(Provenance::hasContentChanged(node_with_hashes, modified_blueprint_, Provenance::BLUEPRINT_HASH_KEY));
    
    EXPECT_FALSE(Provenance::hasContentChanged(node_with_hashes, original_code_, Provenance::CODE_HASH_KEY));
    EXPECT_TRUE(Provenance::hasContentChanged(node_with_hashes, modified_code_, Provenance::CODE_HASH_KEY));
}

TEST_F(ProvenanceMismatchTest, LegacyDetectMismatchesInterface) {
    // Test backward compatibility with the original interface
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    // Test with modified code
    auto mismatches = Provenance::detectMismatches(node_with_hashes, original_blueprint_);
    
    // Should return string descriptions
    EXPECT_FALSE(mismatches.empty());
    for (const auto& mismatch : mismatches) {
        EXPECT_FALSE(mismatch.empty());
    }
}

// Integration test for the complete mismatch detection workflow
TEST_F(ProvenanceMismatchTest, CompleteWorkflow) {
    // 1. Create initial code with provenance
    ProvenanceMetadata metadata("gpt-4", 0.9, "test-blueprint");
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    // 2. Simulate code modification
    auto report = Provenance::analyzeProvenance(node_with_hashes, original_blueprint_, modified_code_);
    
    // 3. Generate warnings
    auto warnings = Provenance::generateMismatchWarnings(report);
    
    // 4. Generate suggestions
    auto suggestions = Provenance::suggestResolutions(report);
    
    // 5. Verify complete workflow
    EXPECT_FALSE(report.mismatch_types.empty());
    EXPECT_FALSE(warnings.empty());
    EXPECT_FALSE(suggestions.empty());
    
    // Verify that warnings and suggestions are related to detected mismatches
    EXPECT_EQ(warnings.size(), report.mismatch_types.size());
    
    // Verify that at least one suggestion is automated
    bool has_automated_suggestion = false;
    for (const auto& suggestion : suggestions) {
        if (suggestion.automated) {
            has_automated_suggestion = true;
            EXPECT_FALSE(suggestion.command.empty());
            break;
        }
    }
    EXPECT_TRUE(has_automated_suggestion);
}

// Test edge cases and error conditions
TEST_F(ProvenanceMismatchTest, EdgeCases) {
    // Test with empty content
    auto report_empty = Provenance::analyzeProvenance(test_node_, "", "");
    EXPECT_TRUE(std::find(report_empty.mismatch_types.begin(), report_empty.mismatch_types.end(), 
                         MismatchType::HashMissing) != report_empty.mismatch_types.end());
    
    // Test with node that has no provenance metadata
    auto report_no_metadata = Provenance::analyzeProvenance(test_node_, original_blueprint_, original_code_);
    EXPECT_TRUE(std::find(report_no_metadata.mismatch_types.begin(), report_no_metadata.mismatch_types.end(), 
                         MismatchType::HashMissing) != report_no_metadata.mismatch_types.end());
    
    // Test with identical content (should have no mismatches except possibly missing hashes)
    ProvenanceMetadata metadata("gpt-4", 0.9);
    Value node_with_metadata = Provenance::attachProvenance(test_node_, metadata);
    Value node_with_hashes = Provenance::attachContentHashes(node_with_metadata, original_blueprint_, original_code_);
    
    auto report_identical = Provenance::analyzeProvenance(node_with_hashes, original_blueprint_, original_code_);
    
    // Should have no content change mismatches
    EXPECT_TRUE(std::find(report_identical.mismatch_types.begin(), report_identical.mismatch_types.end(), 
                         MismatchType::CodeChangedBlueprintStale) == report_identical.mismatch_types.end());
    EXPECT_TRUE(std::find(report_identical.mismatch_types.begin(), report_identical.mismatch_types.end(), 
                         MismatchType::BlueprintChangedCodeStale) == report_identical.mismatch_types.end());
    EXPECT_TRUE(std::find(report_identical.mismatch_types.begin(), report_identical.mismatch_types.end(), 
                         MismatchType::BothChanged) == report_identical.mismatch_types.end());
}