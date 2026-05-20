#pragma once

#include "meld/types/ownership.hpp"
#include "meld/macro/provenance.hpp"
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <optional>

namespace meld::ownership {

// ============================================================================
// OWNERSHIP-AWARE PROVENANCE INTEGRATION
// Task 13.1: Integrate ownership with AI provenance tracking
// Requirements: 9.1, 9.5
// ============================================================================

// Ownership source tracking
enum class OwnershipSource {
    InferredByCompiler,         // Compiler inferred ownership
    ExplicitAnnotation,         // Developer explicitly annotated
    AIGenerated,                // AI model generated ownership
    MacroExpanded,              // Macro expansion created ownership
    Derived                     // Derived from trait implementation
};

// Borrow event tracking
struct BorrowEvent {
    size_t borrow_id;                   // Unique borrow identifier
    BorrowType borrow_type;             // Immutable or mutable
    std::string location;               // Source location
    macro::CodeOrigin origin;           // Who created this borrow
    float confidence;                   // Confidence in correctness (0.0-1.0)
    std::chrono::system_clock::time_point timestamp;
    
    BorrowEvent(size_t id, BorrowType type, std::string loc, 
                macro::CodeOrigin orig, float conf)
        : borrow_id(id)
        , borrow_type(type)
        , location(std::move(loc))
        , origin(orig)
        , confidence(conf)
        , timestamp(std::chrono::system_clock::now()) {}
};

// Ownership provenance metadata
// Extends ProvenanceMetadata with ownership-specific information
struct OwnershipProvenance {
    macro::CodeOrigin origin;                   // Code origin
    OwnershipSource ownership_source;           // How ownership was determined
    float confidence;                           // Confidence score (0.0-1.0)
    
    std::optional<macro::AIModelInfo> ai_info;  // AI generation info
    std::vector<BorrowEvent> borrow_history;    // History of borrows
    
    std::string author;                         // Author (human or AI)
    std::chrono::system_clock::time_point timestamp;
    
    // Parent provenance for tracking transformation chains
    std::shared_ptr<OwnershipProvenance> parent;
    
    // Additional metadata
    std::map<std::string, std::string> metadata;
    
    OwnershipProvenance(macro::CodeOrigin orig, OwnershipSource src, float conf)
        : origin(orig)
        , ownership_source(src)
        , confidence(conf)
        , timestamp(std::chrono::system_clock::now()) {}
};

// Provenance-aware ownership tracker
// Tracks ownership with full provenance information
class ProvenanceAwareOwnershipTracker {
public:
    // Track ownership with provenance
    void track_ownership(const std::string& symbol,
                        OwnershipState state,
                        std::shared_ptr<OwnershipProvenance> provenance);
    
    // Track borrow with provenance
    void track_borrow(const std::string& symbol,
                     BorrowType borrow_type,
                     std::shared_ptr<OwnershipProvenance> provenance);
    
    // Get ownership provenance for a symbol
    std::shared_ptr<OwnershipProvenance> get_provenance(const std::string& symbol) const;
    
    // Get borrow history for a symbol
    std::vector<BorrowEvent> get_borrow_history(const std::string& symbol) const;
    
    // Check if ownership is AI-generated
    bool is_ai_generated(const std::string& symbol) const;
    
    // Get confidence score for ownership
    float get_confidence(const std::string& symbol) const;
    
    // Validate ownership with confidence threshold
    std::expected<void, std::string> validate_with_confidence(
        const std::string& symbol,
        float threshold = 0.8f
    ) const;
    
    // Clear all tracking data
    void clear();
    
private:
    std::map<std::string, std::shared_ptr<OwnershipProvenance>> provenance_map_;
    std::map<std::string, std::vector<BorrowEvent>> borrow_history_map_;
};

// Confidence-based borrow checker
// Extends borrow checker with confidence-aware analysis
class ConfidenceBasedBorrowChecker {
public:
    ConfidenceBasedBorrowChecker(ProvenanceAwareOwnershipTracker& tracker,
                                float confidence_threshold = 0.8f)
        : tracker_(tracker)
        , confidence_threshold_(confidence_threshold) {}
    
    // Check borrow with confidence analysis
    std::expected<void, std::string> check_borrow(
        const std::string& symbol,
        BorrowType borrow_type,
        const std::string& location
    );
    
    // Check move with confidence analysis
    std::expected<void, std::string> check_move(
        const std::string& symbol,
        const std::string& location
    );
    
    // Validate all borrows with confidence threshold
    std::vector<std::string> validate_all_borrows();
    
    // Get warnings for low-confidence ownership
    std::vector<std::string> get_confidence_warnings() const;
    
    // Set confidence threshold
    void set_confidence_threshold(float threshold) {
        confidence_threshold_ = threshold;
    }
    
    float get_confidence_threshold() const {
        return confidence_threshold_;
    }
    
private:
    ProvenanceAwareOwnershipTracker& tracker_;
    float confidence_threshold_;
    
    // Check if confidence meets threshold
    bool meets_confidence_threshold(const std::string& symbol) const;
};

// AI-aware ownership analyzer
// Analyzes ownership patterns in AI-generated code
class AIAwareOwnershipAnalyzer {
public:
    AIAwareOwnershipAnalyzer(const ProvenanceAwareOwnershipTracker& tracker)
        : tracker_(tracker) {}
    
    // Find all AI-generated ownership
    std::vector<std::string> find_ai_generated_ownership() const;
    
    // Find low-confidence ownership
    std::vector<std::string> find_low_confidence_ownership(float threshold = 0.8f) const;
    
    // Analyze ownership patterns by AI model
    std::map<std::string, size_t> analyze_by_ai_model() const;
    
    // Calculate average confidence for AI-generated code
    float calculate_average_ai_confidence() const;
    
    // Generate ownership quality report
    std::string generate_quality_report() const;
    
    // Suggest ownership improvements
    std::vector<std::string> suggest_improvements() const;
    
private:
    const ProvenanceAwareOwnershipTracker& tracker_;
};

// Provenance-aware ownership inference
// Infers ownership with provenance tracking
class ProvenanceAwareOwnershipInference {
public:
    ProvenanceAwareOwnershipInference(ProvenanceAwareOwnershipTracker& tracker)
        : tracker_(tracker) {}
    
    // Infer ownership for a symbol
    std::expected<OwnershipState, std::string> infer_ownership(
        const std::string& symbol,
        const kernel::Value& ast_node,
        const macro::ProvenanceTracker& prov_tracker
    );
    
    // Infer ownership for function parameters
    std::expected<std::vector<OwnershipState>, std::string> infer_parameter_ownership(
        const std::vector<std::string>& params,
        const kernel::Value& function_ast,
        const macro::ProvenanceTracker& prov_tracker
    );
    
    // Infer ownership for return values
    std::expected<OwnershipState, std::string> infer_return_ownership(
        const kernel::Value& function_ast,
        const macro::ProvenanceTracker& prov_tracker
    );
    
    // Create provenance for inferred ownership
    std::shared_ptr<OwnershipProvenance> create_inferred_provenance(
        const kernel::Value& ast_node,
        const macro::ProvenanceTracker& prov_tracker
    );
    
private:
    ProvenanceAwareOwnershipTracker& tracker_;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Convert OwnershipSource to string
std::string ownership_source_to_string(OwnershipSource source);

// Parse OwnershipSource from string
std::expected<OwnershipSource, std::string> parse_ownership_source(const std::string& str);

// Create ownership provenance for human-written code
std::shared_ptr<OwnershipProvenance> create_human_ownership_provenance(
    const std::string& author,
    OwnershipSource source = OwnershipSource::ExplicitAnnotation
);

// Create ownership provenance for AI-generated code
std::shared_ptr<OwnershipProvenance> create_ai_ownership_provenance(
    const macro::AIModelInfo& ai_info,
    OwnershipSource source = OwnershipSource::AIGenerated
);

// Create ownership provenance for compiler-inferred code
std::shared_ptr<OwnershipProvenance> create_inferred_ownership_provenance(
    OwnershipSource source = OwnershipSource::InferredByCompiler
);

// Merge ownership provenance from multiple sources
std::shared_ptr<OwnershipProvenance> merge_ownership_provenance(
    const std::vector<std::shared_ptr<OwnershipProvenance>>& provenances
);

// Convert macro provenance to ownership provenance
std::shared_ptr<OwnershipProvenance> convert_to_ownership_provenance(
    const std::shared_ptr<macro::ProvenanceMetadata>& macro_prov,
    OwnershipSource source
);

// Validate ownership provenance
std::expected<void, std::string> validate_ownership_provenance(
    const OwnershipProvenance& provenance
);

// Serialize ownership provenance to JSON
std::string serialize_ownership_provenance(const OwnershipProvenance& provenance);

// Visualize ownership provenance chain
std::string visualize_ownership_provenance_chain(
    const std::vector<std::shared_ptr<OwnershipProvenance>>& chain
);

} // namespace meld::ownership
