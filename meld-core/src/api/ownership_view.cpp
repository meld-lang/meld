#include "meld/api/ownership_view.hpp"
#include <sstream>
#include <algorithm>
#include <format>
#include <cmath>

namespace meld::holographic {

// ============================================================================
// OwnershipAwareHolographicView Implementation
// ============================================================================

void OwnershipAwareHolographicView::add_node(const HolographicNode& node) {
    nodes_[node.node_id] = node;
}

std::optional<HolographicNode> 
OwnershipAwareHolographicView::get_node(const std::string& node_id) const {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void OwnershipAwareHolographicView::annotate_ownership(
    const std::string& node_id,
    const OwnershipAnnotation& annotation) {
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.ownership = annotation;
    }
}

void OwnershipAwareHolographicView::annotate_borrow(
    const std::string& node_id,
    const BorrowAnnotation& annotation) {
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.borrows.push_back(annotation);
    }
}

std::vector<HolographicNode> 
OwnershipAwareHolographicView::get_owned_nodes() const {
    std::vector<HolographicNode> owned;
    
    for (const auto& [id, node] : nodes_) {
        if (node.ownership && node.ownership->state == ownership::OwnershipState::Owned) {
            owned.push_back(node);
        }
    }
    
    return owned;
}

std::vector<HolographicNode> 
OwnershipAwareHolographicView::get_borrowed_nodes() const {
    std::vector<HolographicNode> borrowed;
    
    for (const auto& [id, node] : nodes_) {
        if (!node.borrows.empty()) {
            borrowed.push_back(node);
        }
    }
    
    return borrowed;
}

std::vector<HolographicNode> 
OwnershipAwareHolographicView::get_ai_generated_nodes() const {
    std::vector<HolographicNode> ai_nodes;
    
    for (const auto& [id, node] : nodes_) {
        if (node.ownership && node.ownership->is_ai_generated) {
            ai_nodes.push_back(node);
        }
    }
    
    return ai_nodes;
}

std::vector<uint8_t> OwnershipAwareHolographicView::compress() const {
    // Simplified compression - in full implementation would use proper compression
    std::vector<uint8_t> compressed;
    
    // Add node count
    size_t node_count = nodes_.size();
    compressed.push_back(static_cast<uint8_t>(node_count & 0xFF));
    compressed.push_back(static_cast<uint8_t>((node_count >> 8) & 0xFF));
    
    // Add nodes (simplified)
    for (const auto& [id, node] : nodes_) {
        // In full implementation, would serialize each node
        compressed.push_back(0x01);  // Node marker
    }
    
    return compressed;
}

std::expected<OwnershipAwareHolographicView, std::string>
OwnershipAwareHolographicView::decompress(const std::vector<uint8_t>& data) {
    if (data.size() < 2) {
        return std::unexpected("Invalid compressed data");
    }
    
    OwnershipAwareHolographicView view;
    
    // Extract node count
    size_t node_count = data[0] | (data[1] << 8);
    
    // In full implementation, would deserialize nodes
    
    return view;
}

std::string OwnershipAwareHolographicView::get_semantic_summary() const {
    std::ostringstream oss;
    
    oss << "Holographic View Summary\n";
    oss << "========================\n\n";
    
    oss << "Total nodes: " << nodes_.size() << "\n";
    oss << "Owned nodes: " << get_owned_nodes().size() << "\n";
    oss << "Borrowed nodes: " << get_borrowed_nodes().size() << "\n";
    oss << "AI-generated nodes: " << get_ai_generated_nodes().size() << "\n";
    
    return oss.str();
}

std::expected<void, std::string> 
OwnershipAwareHolographicView::validate_ownership() const {
    // Validate ownership consistency across all nodes
    for (const auto& [id, node] : nodes_) {
        if (node.ownership) {
            if (node.ownership->confidence < 0.0f || node.ownership->confidence > 1.0f) {
                return std::unexpected(
                    std::format("Invalid confidence for node {}: {}",
                               id, node.ownership->confidence)
                );
            }
        }
    }
    
    return {};
}

// ============================================================================
// HolographicViewBuilder Implementation
// ============================================================================

std::expected<OwnershipAwareHolographicView, std::string>
HolographicViewBuilder::build_from_ast(const kernel::Value& ast_root) {
    OwnershipAwareHolographicView view;
    
    // Create root node
    HolographicNode root(generate_node_id(), "root", "program");
    
    // Extract ownership if available
    auto ownership = extract_ownership(ast_root);
    if (ownership) {
        root.ownership = *ownership;
    }
    
    // Extract borrows
    root.borrows = extract_borrows(ast_root);
    
    view.add_node(root);
    
    return view;
}

std::expected<OwnershipAwareHolographicView, std::string>
HolographicViewBuilder::build_from_function(const kernel::Value& function_ast) {
    OwnershipAwareHolographicView view;
    
    // Create function node
    HolographicNode func(generate_node_id(), "function", "function_def");
    
    auto ownership = extract_ownership(function_ast);
    if (ownership) {
        func.ownership = *ownership;
    }
    
    func.borrows = extract_borrows(function_ast);
    
    view.add_node(func);
    
    return view;
}

std::expected<OwnershipAwareHolographicView, std::string>
HolographicViewBuilder::build_from_module(const kernel::Value& module_ast) {
    OwnershipAwareHolographicView view;
    
    // Create module node
    HolographicNode module(generate_node_id(), "module", "module_def");
    
    auto ownership = extract_ownership(module_ast);
    if (ownership) {
        module.ownership = *ownership;
    }
    
    view.add_node(module);
    
    return view;
}

std::string HolographicViewBuilder::generate_node_id() {
    return std::format("node_{}", next_node_id_++);
}

std::optional<OwnershipAnnotation> 
HolographicViewBuilder::extract_ownership(const kernel::Value& ast_node) {
    // In full implementation, would extract from AST metadata
    // For now, return placeholder
    return std::nullopt;
}

std::vector<BorrowAnnotation> 
HolographicViewBuilder::extract_borrows(const kernel::Value& ast_node) {
    // In full implementation, would extract from AST metadata
    return {};
}

// ============================================================================
// OwnershipVisualization Implementation
// ============================================================================

std::string OwnershipVisualization::generate_ownership_graph() const {
    std::ostringstream oss;
    
    oss << "digraph OwnershipGraph {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box];\n\n";
    
    for (const auto& [id, node] : view_.nodes()) {
        oss << "  " << id << " [label=\"" << node.content;
        
        if (node.ownership) {
            oss << "\\n" << visualize_ownership_state(node.ownership->state);
            oss << "\\nConf: " << (node.ownership->confidence * 100.0f) << "%";
        }
        
        oss << "\"];\n";
        
        // Add edges to children
        for (const auto& child_id : node.children) {
            oss << "  " << id << " -> " << child_id << ";\n";
        }
    }
    
    oss << "}\n";
    
    return oss.str();
}

std::string OwnershipVisualization::generate_borrow_timeline() const {
    std::ostringstream oss;
    
    oss << "Borrow Timeline\n";
    oss << "===============\n\n";
    
    for (const auto& [id, node] : view_.nodes()) {
        if (!node.borrows.empty()) {
            oss << node.content << ":\n";
            
            for (const auto& borrow : node.borrows) {
                oss << "  - " << visualize_borrow_type(borrow.borrow_type);
                oss << " at " << borrow.location;
                oss << " (lifetime " << borrow.lifetime_id << ")\n";
            }
        }
    }
    
    return oss.str();
}

std::string OwnershipVisualization::generate_confidence_heatmap() const {
    std::ostringstream oss;
    
    oss << "Confidence Heatmap\n";
    oss << "==================\n\n";
    
    for (const auto& [id, node] : view_.nodes()) {
        if (node.ownership) {
            float conf = node.ownership->confidence;
            
            // Generate visual bar
            int bar_length = static_cast<int>(conf * 20);
            std::string bar(bar_length, '█');
            std::string empty(20 - bar_length, '░');
            
            oss << node.content << ": " << bar << empty;
            oss << " " << (conf * 100.0f) << "%\n";
        }
    }
    
    return oss.str();
}

std::string OwnershipVisualization::generate_provenance_tree() const {
    std::ostringstream oss;
    
    oss << "Provenance Tree\n";
    oss << "===============\n\n";
    
    for (const auto& [id, node] : view_.nodes()) {
        if (node.ownership) {
            oss << node.content << " (" << node.ownership->origin << ")\n";
            
            if (node.ownership->is_ai_generated) {
                oss << "  └─ AI Generated\n";
            }
        }
    }
    
    return oss.str();
}

std::map<std::string, std::string> 
OwnershipVisualization::generate_inline_annotations() const {
    std::map<std::string, std::string> annotations;
    
    for (const auto& [id, node] : view_.nodes()) {
        if (node.ownership) {
            std::string annotation = visualize_ownership_state(node.ownership->state);
            
            if (node.ownership->is_ai_generated) {
                annotation += " [AI]";
            }
            
            annotations[id] = annotation;
        }
    }
    
    return annotations;
}

std::string OwnershipVisualization::generate_hover_summary(
    const std::string& node_id) const {
    
    auto node = view_.get_node(node_id);
    if (!node) {
        return "Node not found";
    }
    
    std::ostringstream oss;
    
    oss << "Node: " << node->content << "\n";
    oss << "Type: " << node->node_type << "\n";
    
    if (node->ownership) {
        oss << "Ownership: " << visualize_ownership_state(node->ownership->state) << "\n";
        oss << "Confidence: " << (node->ownership->confidence * 100.0f) << "%\n";
        oss << "Origin: " << node->ownership->origin << "\n";
    }
    
    if (!node->borrows.empty()) {
        oss << "Borrows: " << node->borrows.size() << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// OwnershipSemanticAnalyzer Implementation
// ============================================================================

std::map<std::string, size_t> 
OwnershipSemanticAnalyzer::analyze_ownership_patterns() const {
    std::map<std::string, size_t> patterns;
    
    patterns["owned"] = view_.get_owned_nodes().size();
    patterns["borrowed"] = view_.get_borrowed_nodes().size();
    patterns["ai_generated"] = view_.get_ai_generated_nodes().size();
    
    return patterns;
}

std::vector<std::string> 
OwnershipSemanticAnalyzer::find_ownership_issues() const {
    std::vector<std::string> issues;
    
    for (const auto& [id, node] : view_.nodes()) {
        if (node.ownership && node.ownership->confidence < 0.8f) {
            issues.push_back(
                std::format("Low confidence ownership in {}: {}%",
                           node.content, node.ownership->confidence * 100.0f)
            );
        }
    }
    
    return issues;
}

float OwnershipSemanticAnalyzer::calculate_ownership_complexity() const {
    float complexity = 0.0f;
    
    for (const auto& [id, node] : view_.nodes()) {
        if (node.ownership) {
            complexity += 1.0f;
        }
        complexity += node.borrows.size() * 0.5f;
    }
    
    return complexity;
}

std::map<std::string, float> 
OwnershipSemanticAnalyzer::analyze_ai_ownership_quality() const {
    std::map<std::string, float> quality;
    
    auto ai_nodes = view_.get_ai_generated_nodes();
    
    if (ai_nodes.empty()) {
        return quality;
    }
    
    float total_confidence = 0.0f;
    for (const auto& node : ai_nodes) {
        if (node.ownership) {
            total_confidence += node.ownership->confidence;
        }
    }
    
    quality["average_confidence"] = total_confidence / ai_nodes.size();
    quality["ai_node_count"] = static_cast<float>(ai_nodes.size());
    
    return quality;
}

std::vector<std::string> 
OwnershipSemanticAnalyzer::find_unused_owned_values() const {
    std::vector<std::string> unused;
    
    // In full implementation, would analyze usage patterns
    
    return unused;
}

std::vector<std::string> 
OwnershipSemanticAnalyzer::find_unnecessary_borrows() const {
    std::vector<std::string> unnecessary;
    
    // In full implementation, would analyze borrow patterns
    
    return unnecessary;
}

std::vector<std::string> 
OwnershipSemanticAnalyzer::generate_optimization_suggestions() const {
    std::vector<std::string> suggestions;
    
    auto issues = find_ownership_issues();
    for (const auto& issue : issues) {
        suggestions.push_back("Review: " + issue);
    }
    
    return suggestions;
}

// ============================================================================
// OwnershipAwareCodeCompression Implementation
// ============================================================================

std::vector<uint8_t> OwnershipAwareCodeCompression::compress_code(
    const std::string& code,
    const OwnershipAwareHolographicView& view) {
    
    // Simplified compression
    std::vector<uint8_t> compressed;
    
    // Add code length
    size_t code_len = code.size();
    compressed.push_back(static_cast<uint8_t>(code_len & 0xFF));
    compressed.push_back(static_cast<uint8_t>((code_len >> 8) & 0xFF));
    
    // Add code bytes
    compressed.insert(compressed.end(), code.begin(), code.end());
    
    // Add view
    auto view_compressed = view.compress();
    compressed.insert(compressed.end(), view_compressed.begin(), view_compressed.end());
    
    return compressed;
}

std::expected<std::pair<std::string, OwnershipAwareHolographicView>, std::string>
OwnershipAwareCodeCompression::decompress_code(const std::vector<uint8_t>& compressed) {
    if (compressed.size() < 2) {
        return std::unexpected("Invalid compressed data");
    }
    
    // Extract code length
    size_t code_len = compressed[0] | (compressed[1] << 8);
    
    if (compressed.size() < 2 + code_len) {
        return std::unexpected("Truncated compressed data");
    }
    
    // Extract code
    std::string code(compressed.begin() + 2, compressed.begin() + 2 + code_len);
    
    // Extract view
    std::vector<uint8_t> view_data(compressed.begin() + 2 + code_len, compressed.end());
    auto view_result = OwnershipAwareHolographicView::decompress(view_data);
    
    if (!view_result) {
        return std::unexpected(view_result.error());
    }
    
    return std::make_pair(code, *view_result);
}

float OwnershipAwareCodeCompression::calculate_compression_ratio(
    const std::string& original,
    const std::vector<uint8_t>& compressed) {
    
    if (original.empty()) {
        return 0.0f;
    }
    
    return static_cast<float>(compressed.size()) / static_cast<float>(original.size());
}

std::expected<void, std::string>
OwnershipAwareCodeCompression::validate_compressed(
    const std::vector<uint8_t>& compressed) {
    
    if (compressed.size() < 2) {
        return std::unexpected("Compressed data too small");
    }
    
    return {};
}

// ============================================================================
// Helper Functions
// ============================================================================

OwnershipAnnotation create_ownership_annotation(
    const std::shared_ptr<ownership::OwnershipProvenance>& provenance,
    ownership::OwnershipState state) {
    
    if (!provenance) {
        return OwnershipAnnotation(state, false, 0.0f, "unknown");
    }
    
    bool is_ai = (provenance->origin == macro::CodeOrigin::AIGenerated);
    
    return OwnershipAnnotation(
        state,
        is_ai,
        provenance->confidence,
        provenance->author
    );
}

BorrowAnnotation create_borrow_annotation(
    const ownership::BorrowEvent& event,
    size_t lifetime_id) {
    
    return BorrowAnnotation(
        event.borrow_type,
        lifetime_id,
        event.location
    );
}

std::string serialize_holographic_node(const HolographicNode& node) {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"node_id\": \"" << node.node_id << "\",\n";
    oss << "  \"node_type\": \"" << node.node_type << "\",\n";
    oss << "  \"content\": \"" << node.content << "\",\n";
    
    if (node.ownership) {
        oss << "  \"ownership\": {\n";
        oss << "    \"state\": \"" << visualize_ownership_state(node.ownership->state) << "\",\n";
        oss << "    \"confidence\": " << node.ownership->confidence << ",\n";
        oss << "    \"origin\": \"" << node.ownership->origin << "\"\n";
        oss << "  },\n";
    }
    
    oss << "  \"borrows\": " << node.borrows.size() << "\n";
    oss << "}";
    
    return oss.str();
}

std::expected<HolographicNode, std::string>
deserialize_holographic_node(const std::string& data) {
    // Simplified deserialization
    return std::unexpected("Deserialization not implemented");
}

OwnershipAwareHolographicView merge_holographic_views(
    const std::vector<OwnershipAwareHolographicView>& views) {
    
    OwnershipAwareHolographicView merged;
    
    for (const auto& view : views) {
        for (const auto& [id, node] : view.nodes()) {
            merged.add_node(node);
        }
    }
    
    return merged;
}

float calculate_view_similarity(
    const OwnershipAwareHolographicView& view1,
    const OwnershipAwareHolographicView& view2) {
    
    size_t common_nodes = 0;
    size_t total_nodes = view1.nodes().size() + view2.nodes().size();
    
    for (const auto& [id, node] : view1.nodes()) {
        if (view2.get_node(id)) {
            common_nodes++;
        }
    }
    
    if (total_nodes == 0) {
        return 1.0f;
    }
    
    return (2.0f * common_nodes) / total_nodes;
}

std::map<std::string, size_t> extract_ownership_statistics(
    const OwnershipAwareHolographicView& view) {
    
    std::map<std::string, size_t> stats;
    
    stats["total_nodes"] = view.nodes().size();
    stats["owned_nodes"] = view.get_owned_nodes().size();
    stats["borrowed_nodes"] = view.get_borrowed_nodes().size();
    stats["ai_generated_nodes"] = view.get_ai_generated_nodes().size();
    
    return stats;
}

std::string visualize_ownership_state(ownership::OwnershipState state) {
    switch (state) {
        case ownership::OwnershipState::Owned:
            return "Owned";
        case ownership::OwnershipState::Borrowed:
            return "Borrowed";
        case ownership::OwnershipState::Moved:
            return "Moved";
        default:
            return "Unknown";
    }
}

std::string visualize_borrow_type(ownership::BorrowType type) {
    switch (type) {
        case ownership::BorrowType::Immutable:
            return "Immutable";
        case ownership::BorrowType::Mutable:
            return "Mutable";
        default:
            return "Unknown";
    }
}

} // namespace meld::holographic
