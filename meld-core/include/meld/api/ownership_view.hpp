#pragma once

#include "meld/types/ownership.hpp"
#include "meld/ownership/provenance_integration.hpp"
#include "meld/kernel/primitives.hpp"
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <optional>
#include <string>

namespace meld::holographic {

// ============================================================================
// OWNERSHIP-AWARE HOLOGRAPHIC VIEW
// Task 13.3: Extend holographic view with ownership info
// Requirement: 9.4
// ============================================================================

// Ownership annotation for holographic view
// Compact representation of ownership information
struct OwnershipAnnotation {
    ownership::OwnershipState state;
    bool is_ai_generated;
    float confidence;
    std::string origin;  // "human", "ai:GPT-4", "compiler", etc.
    
    OwnershipAnnotation(ownership::OwnershipState s, bool ai, float conf, std::string orig)
        : state(s), is_ai_generated(ai), confidence(conf), origin(std::move(orig)) {}
};

// Borrow annotation for holographic view
struct BorrowAnnotation {
    ownership::BorrowType borrow_type;
    size_t lifetime_id;
    std::string location;
    
    BorrowAnnotation(ownership::BorrowType type, size_t lifetime, std::string loc)
        : borrow_type(type), lifetime_id(lifetime), location(std::move(loc)) {}
};

// Holographic node with ownership
// Represents a code element with ownership information
struct HolographicNode {
    std::string node_id;
    std::string node_type;  // "function", "variable", "expression", etc.
    std::string content;
    
    std::optional<OwnershipAnnotation> ownership;
    std::vector<BorrowAnnotation> borrows;
    
    std::vector<std::string> children;  // Child node IDs
    std::optional<std::string> parent;  // Parent node ID
    
    std::map<std::string, std::string> metadata;
    
    HolographicNode(std::string id, std::string type, std::string cont)
        : node_id(std::move(id))
        , node_type(std::move(type))
        , content(std::move(cont)) {}
};

// Ownership-aware holographic view
// Compressed representation of code with ownership information
class OwnershipAwareHolographicView {
public:
    // Add node to view
    void add_node(const HolographicNode& node);
    
    // Get node by ID
    std::optional<HolographicNode> get_node(const std::string& node_id) const;
    
    // Add ownership annotation to node
    void annotate_ownership(const std::string& node_id,
                           const OwnershipAnnotation& annotation);
    
    // Add borrow annotation to node
    void annotate_borrow(const std::string& node_id,
                        const BorrowAnnotation& annotation);
    
    // Get all nodes with ownership
    std::vector<HolographicNode> get_owned_nodes() const;
    
    // Get all nodes with borrows
    std::vector<HolographicNode> get_borrowed_nodes() const;
    
    // Get AI-generated nodes
    std::vector<HolographicNode> get_ai_generated_nodes() const;
    
    // Compress view to binary format
    std::vector<uint8_t> compress() const;
    
    // Decompress view from binary format
    static std::expected<OwnershipAwareHolographicView, std::string>
    decompress(const std::vector<uint8_t>& data);
    
    // Get semantic summary
    std::string get_semantic_summary() const;
    
    // Validate ownership consistency
    std::expected<void, std::string> validate_ownership() const;
    
    // Get all nodes
    const std::map<std::string, HolographicNode>& nodes() const { return nodes_; }
    
private:
    std::map<std::string, HolographicNode> nodes_;
};

// Holographic view builder
// Builds ownership-aware holographic view from AST
class HolographicViewBuilder {
public:
    HolographicViewBuilder(const ownership::ProvenanceAwareOwnershipTracker& tracker)
        : tracker_(tracker) {}
    
    // Build view from AST
    std::expected<OwnershipAwareHolographicView, std::string>
    build_from_ast(const kernel::Value& ast_root);
    
    // Build view from function
    std::expected<OwnershipAwareHolographicView, std::string>
    build_from_function(const kernel::Value& function_ast);
    
    // Build view from module
    std::expected<OwnershipAwareHolographicView, std::string>
    build_from_module(const kernel::Value& module_ast);
    
private:
    const ownership::ProvenanceAwareOwnershipTracker& tracker_;
    size_t next_node_id_ = 0;
    
    // Generate unique node ID
    std::string generate_node_id();
    
    // Extract ownership annotation from AST node
    std::optional<OwnershipAnnotation> extract_ownership(
        const kernel::Value& ast_node
    );
    
    // Extract borrow annotations from AST node
    std::vector<BorrowAnnotation> extract_borrows(
        const kernel::Value& ast_node
    );
};

// Ownership visualization for IDEs
// Generates visual representations of ownership for IDE display
class OwnershipVisualization {
public:
    OwnershipVisualization(const OwnershipAwareHolographicView& view)
        : view_(view) {}
    
    // Generate ownership graph (DOT format)
    std::string generate_ownership_graph() const;
    
    // Generate borrow timeline
    std::string generate_borrow_timeline() const;
    
    // Generate confidence heatmap
    std::string generate_confidence_heatmap() const;
    
    // Generate AI provenance tree
    std::string generate_provenance_tree() const;
    
    // Generate inline ownership annotations (for code editor)
    std::map<std::string, std::string> generate_inline_annotations() const;
    
    // Generate ownership summary for hover tooltips
    std::string generate_hover_summary(const std::string& node_id) const;
    
private:
    const OwnershipAwareHolographicView& view_;
};

// Semantic analysis with ownership
// Analyzes code semantics including ownership patterns
class OwnershipSemanticAnalyzer {
public:
    OwnershipSemanticAnalyzer(const OwnershipAwareHolographicView& view)
        : view_(view) {}
    
    // Analyze ownership patterns
    std::map<std::string, size_t> analyze_ownership_patterns() const;
    
    // Find potential ownership issues
    std::vector<std::string> find_ownership_issues() const;
    
    // Calculate ownership complexity
    float calculate_ownership_complexity() const;
    
    // Analyze AI-generated ownership quality
    std::map<std::string, float> analyze_ai_ownership_quality() const;
    
    // Find unused owned values
    std::vector<std::string> find_unused_owned_values() const;
    
    // Find unnecessary borrows
    std::vector<std::string> find_unnecessary_borrows() const;
    
    // Generate optimization suggestions
    std::vector<std::string> generate_optimization_suggestions() const;
    
private:
    const OwnershipAwareHolographicView& view_;
};

// Code compression with ownership
// Compresses code while preserving ownership information
class OwnershipAwareCodeCompression {
public:
    // Compress code with ownership
    static std::vector<uint8_t> compress_code(
        const std::string& code,
        const OwnershipAwareHolographicView& view
    );
    
    // Decompress code with ownership
    static std::expected<std::pair<std::string, OwnershipAwareHolographicView>, std::string>
    decompress_code(const std::vector<uint8_t>& compressed);
    
    // Calculate compression ratio
    static float calculate_compression_ratio(
        const std::string& original,
        const std::vector<uint8_t>& compressed
    );
    
    // Validate compressed data
    static std::expected<void, std::string>
    validate_compressed(const std::vector<uint8_t>& compressed);
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Create ownership annotation from provenance
OwnershipAnnotation create_ownership_annotation(
    const std::shared_ptr<ownership::OwnershipProvenance>& provenance,
    ownership::OwnershipState state
);

// Create borrow annotation from borrow event
BorrowAnnotation create_borrow_annotation(
    const ownership::BorrowEvent& event,
    size_t lifetime_id
);

// Serialize holographic node
std::string serialize_holographic_node(const HolographicNode& node);

// Deserialize holographic node
std::expected<HolographicNode, std::string>
deserialize_holographic_node(const std::string& data);

// Merge holographic views
OwnershipAwareHolographicView merge_holographic_views(
    const std::vector<OwnershipAwareHolographicView>& views
);

// Calculate view similarity
float calculate_view_similarity(
    const OwnershipAwareHolographicView& view1,
    const OwnershipAwareHolographicView& view2
);

// Extract ownership statistics
std::map<std::string, size_t> extract_ownership_statistics(
    const OwnershipAwareHolographicView& view
);

// Visualize ownership state
std::string visualize_ownership_state(ownership::OwnershipState state);

// Visualize borrow type
std::string visualize_borrow_type(ownership::BorrowType type);

} // namespace meld::holographic
