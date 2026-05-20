#include "meld/ownership/provenance_integration.hpp"
#include <sstream>
#include <algorithm>
#include <format>

namespace meld::ownership {

// ============================================================================
// ProvenanceAwareOwnershipTracker Implementation
// ============================================================================

void ProvenanceAwareOwnershipTracker::track_ownership(
    const std::string& symbol,
    OwnershipState state,
    std::shared_ptr<OwnershipProvenance> provenance) {
    
    provenance_map_[symbol] = std::move(provenance);
}

void ProvenanceAwareOwnershipTracker::track_borrow(
    const std::string& symbol,
    BorrowType borrow_type,
    std::shared_ptr<OwnershipProvenance> provenance) {
    
    // Create borrow event
    BorrowEvent event(
        borrow_history_map_[symbol].size(),
        borrow_type,
        "unknown",  // Would be actual source location in full implementation
        provenance->origin,
        provenance->confidence
    );
    
    // Add to history
    borrow_history_map_[symbol].push_back(event);
    
    // Update provenance
    if (provenance_map_.contains(symbol)) {
        provenance_map_[symbol]->borrow_history.push_back(event);
    }
}

std::shared_ptr<OwnershipProvenance> 
ProvenanceAwareOwnershipTracker::get_provenance(const std::string& symbol) const {
    auto it = provenance_map_.find(symbol);
    if (it != provenance_map_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<BorrowEvent> 
ProvenanceAwareOwnershipTracker::get_borrow_history(const std::string& symbol) const {
    auto it = borrow_history_map_.find(symbol);
    if (it != borrow_history_map_.end()) {
        return it->second;
    }
    return {};
}

bool ProvenanceAwareOwnershipTracker::is_ai_generated(const std::string& symbol) const {
    auto prov = get_provenance(symbol);
    return prov && prov->origin == macro::CodeOrigin::AIGenerated;
}

float ProvenanceAwareOwnershipTracker::get_confidence(const std::string& symbol) const {
    auto prov = get_provenance(symbol);
    return prov ? prov->confidence : 0.0f;
}

std::expected<void, std::string> 
ProvenanceAwareOwnershipTracker::validate_with_confidence(
    const std::string& symbol,
    float threshold) const {
    
    auto prov = get_provenance(symbol);
    if (!prov) {
        return std::unexpected("No provenance found for symbol: " + symbol);
    }
    
    if (prov->confidence < threshold) {
        return std::unexpected(
            std::format("Confidence {} below threshold {} for symbol: {}",
                       prov->confidence, threshold, symbol)
        );
    }
    
    return {};
}

void ProvenanceAwareOwnershipTracker::clear() {
    provenance_map_.clear();
    borrow_history_map_.clear();
}

// ============================================================================
// ConfidenceBasedBorrowChecker Implementation
// ============================================================================

std::expected<void, std::string> 
ConfidenceBasedBorrowChecker::check_borrow(
    const std::string& symbol,
    BorrowType borrow_type,
    const std::string& location) {
    
    // Check if symbol has provenance
    auto prov = tracker_.get_provenance(symbol);
    if (!prov) {
        return std::unexpected("No provenance found for symbol: " + symbol);
    }
    
    // Check confidence threshold
    if (!meets_confidence_threshold(symbol)) {
        return std::unexpected(
            std::format("Low confidence ({}) for borrow of symbol: {}",
                       prov->confidence, symbol)
        );
    }
    
    // Check for conflicting borrows
    auto history = tracker_.get_borrow_history(symbol);
    for (const auto& event : history) {
        if (event.borrow_type == BorrowType::Mutable && 
            borrow_type == BorrowType::Mutable) {
            return std::unexpected(
                "Cannot have multiple mutable borrows of: " + symbol
            );
        }
        if (event.borrow_type == BorrowType::Mutable || 
            borrow_type == BorrowType::Mutable) {
            return std::unexpected(
                "Cannot mix mutable and immutable borrows of: " + symbol
            );
        }
    }
    
    return {};
}

std::expected<void, std::string> 
ConfidenceBasedBorrowChecker::check_move(
    const std::string& symbol,
    const std::string& location) {
    
    // Check if symbol has provenance
    auto prov = tracker_.get_provenance(symbol);
    if (!prov) {
        return std::unexpected("No provenance found for symbol: " + symbol);
    }
    
    // Check confidence threshold
    if (!meets_confidence_threshold(symbol)) {
        return std::unexpected(
            std::format("Low confidence ({}) for move of symbol: {}",
                       prov->confidence, symbol)
        );
    }
    
    // Check for active borrows
    auto history = tracker_.get_borrow_history(symbol);
    if (!history.empty()) {
        return std::unexpected(
            "Cannot move symbol with active borrows: " + symbol
        );
    }
    
    return {};
}

std::vector<std::string> 
ConfidenceBasedBorrowChecker::validate_all_borrows() {
    std::vector<std::string> warnings;
    
    // In a full implementation, would iterate through all tracked symbols
    // and validate their borrow patterns
    
    return warnings;
}

std::vector<std::string> 
ConfidenceBasedBorrowChecker::get_confidence_warnings() const {
    std::vector<std::string> warnings;
    
    // In a full implementation, would check all symbols for low confidence
    
    return warnings;
}

bool ConfidenceBasedBorrowChecker::meets_confidence_threshold(
    const std::string& symbol) const {
    
    float confidence = tracker_.get_confidence(symbol);
    return confidence >= confidence_threshold_;
}

// ============================================================================
// AIAwareOwnershipAnalyzer Implementation
// ============================================================================

std::vector<std::string> 
AIAwareOwnershipAnalyzer::find_ai_generated_ownership() const {
    std::vector<std::string> results;
    
    // In a full implementation, would iterate through all tracked symbols
    // and find those with AI-generated provenance
    
    return results;
}

std::vector<std::string> 
AIAwareOwnershipAnalyzer::find_low_confidence_ownership(float threshold) const {
    std::vector<std::string> results;
    
    // In a full implementation, would find all symbols with confidence below threshold
    
    return results;
}

std::map<std::string, size_t> 
AIAwareOwnershipAnalyzer::analyze_by_ai_model() const {
    std::map<std::string, size_t> model_counts;
    
    // In a full implementation, would count ownership by AI model
    
    return model_counts;
}

float AIAwareOwnershipAnalyzer::calculate_average_ai_confidence() const {
    // In a full implementation, would calculate average confidence
    // for all AI-generated ownership
    return 0.0f;
}

std::string AIAwareOwnershipAnalyzer::generate_quality_report() const {
    std::ostringstream oss;
    
    oss << "Ownership Quality Report\n";
    oss << "========================\n\n";
    
    auto ai_ownership = find_ai_generated_ownership();
    oss << "AI-Generated Ownership: " << ai_ownership.size() << "\n";
    
    float avg_confidence = calculate_average_ai_confidence();
    oss << "Average AI Confidence: " << (avg_confidence * 100.0f) << "%\n";
    
    auto low_confidence = find_low_confidence_ownership(0.8f);
    oss << "Low Confidence Items: " << low_confidence.size() << "\n";
    
    return oss.str();
}

std::vector<std::string> 
AIAwareOwnershipAnalyzer::suggest_improvements() const {
    std::vector<std::string> suggestions;
    
    // Find low-confidence ownership
    auto low_confidence = find_low_confidence_ownership(0.8f);
    
    for (const auto& symbol : low_confidence) {
        suggestions.push_back(
            "Consider manually reviewing ownership for: " + symbol
        );
    }
    
    return suggestions;
}

// ============================================================================
// ProvenanceAwareOwnershipInference Implementation
// ============================================================================

std::expected<OwnershipState, std::string> 
ProvenanceAwareOwnershipInference::infer_ownership(
    const std::string& symbol,
    const kernel::Value& ast_node,
    const macro::ProvenanceTracker& prov_tracker) {
    
    // Get provenance from AST node
    auto macro_prov = prov_tracker.get_provenance(ast_node);
    
    // Create ownership provenance
    auto ownership_prov = create_inferred_provenance(ast_node, prov_tracker);
    
    // Infer ownership state (simplified logic)
    OwnershipState state = OwnershipState::Owned;
    
    // Track ownership with provenance
    tracker_.track_ownership(symbol, state, ownership_prov);
    
    return state;
}

std::expected<std::vector<OwnershipState>, std::string> 
ProvenanceAwareOwnershipInference::infer_parameter_ownership(
    const std::vector<std::string>& params,
    const kernel::Value& function_ast,
    const macro::ProvenanceTracker& prov_tracker) {
    
    std::vector<OwnershipState> states;
    
    for (const auto& param : params) {
        auto result = infer_ownership(param, function_ast, prov_tracker);
        if (!result) {
            return std::unexpected(result.error());
        }
        states.push_back(*result);
    }
    
    return states;
}

std::expected<OwnershipState, std::string> 
ProvenanceAwareOwnershipInference::infer_return_ownership(
    const kernel::Value& function_ast,
    const macro::ProvenanceTracker& prov_tracker) {
    
    // Infer return value ownership
    return infer_ownership("return", function_ast, prov_tracker);
}

std::shared_ptr<OwnershipProvenance> 
ProvenanceAwareOwnershipInference::create_inferred_provenance(
    const kernel::Value& ast_node,
    const macro::ProvenanceTracker& prov_tracker) {
    
    // Get macro provenance
    auto macro_prov = prov_tracker.get_provenance(ast_node);
    
    if (macro_prov) {
        // Convert macro provenance to ownership provenance
        return convert_to_ownership_provenance(
            macro_prov,
            OwnershipSource::InferredByCompiler
        );
    }
    
    // Create default inferred provenance
    return create_inferred_ownership_provenance();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string ownership_source_to_string(OwnershipSource source) {
    switch (source) {
        case OwnershipSource::InferredByCompiler:
            return "InferredByCompiler";
        case OwnershipSource::ExplicitAnnotation:
            return "ExplicitAnnotation";
        case OwnershipSource::AIGenerated:
            return "AIGenerated";
        case OwnershipSource::MacroExpanded:
            return "MacroExpanded";
        case OwnershipSource::Derived:
            return "Derived";
        default:
            return "Unknown";
    }
}

std::expected<OwnershipSource, std::string> 
parse_ownership_source(const std::string& str) {
    if (str == "InferredByCompiler") return OwnershipSource::InferredByCompiler;
    if (str == "ExplicitAnnotation") return OwnershipSource::ExplicitAnnotation;
    if (str == "AIGenerated") return OwnershipSource::AIGenerated;
    if (str == "MacroExpanded") return OwnershipSource::MacroExpanded;
    if (str == "Derived") return OwnershipSource::Derived;
    
    return std::unexpected("Unknown ownership source: " + str);
}

std::shared_ptr<OwnershipProvenance> create_human_ownership_provenance(
    const std::string& author,
    OwnershipSource source) {
    
    auto prov = std::make_shared<OwnershipProvenance>(
        macro::CodeOrigin::Human,
        source,
        1.0f  // Full confidence for human-written code
    );
    prov->author = author;
    return prov;
}

std::shared_ptr<OwnershipProvenance> create_ai_ownership_provenance(
    const macro::AIModelInfo& ai_info,
    OwnershipSource source) {
    
    auto prov = std::make_shared<OwnershipProvenance>(
        macro::CodeOrigin::AIGenerated,
        source,
        ai_info.confidence
    );
    prov->ai_info = ai_info;
    prov->author = ai_info.model_name;
    return prov;
}

std::shared_ptr<OwnershipProvenance> create_inferred_ownership_provenance(
    OwnershipSource source) {
    
    auto prov = std::make_shared<OwnershipProvenance>(
        macro::CodeOrigin::Compiler,
        source,
        0.95f  // High confidence for compiler inference
    );
    prov->author = "compiler";
    return prov;
}

std::shared_ptr<OwnershipProvenance> merge_ownership_provenance(
    const std::vector<std::shared_ptr<OwnershipProvenance>>& provenances) {
    
    if (provenances.empty()) {
        return create_inferred_ownership_provenance();
    }
    
    if (provenances.size() == 1) {
        return provenances[0];
    }
    
    // Create merged provenance
    auto merged = std::make_shared<OwnershipProvenance>(
        macro::CodeOrigin::Compiler,
        OwnershipSource::InferredByCompiler,
        0.0f
    );
    merged->author = "merged";
    
    // Calculate average confidence
    float total_confidence = 0.0f;
    for (const auto& prov : provenances) {
        if (prov) {
            total_confidence += prov->confidence;
        }
    }
    merged->confidence = total_confidence / provenances.size();
    
    // Use earliest timestamp
    merged->timestamp = provenances[0]->timestamp;
    for (const auto& prov : provenances) {
        if (prov && prov->timestamp < merged->timestamp) {
            merged->timestamp = prov->timestamp;
        }
    }
    
    return merged;
}

std::shared_ptr<OwnershipProvenance> convert_to_ownership_provenance(
    const std::shared_ptr<macro::ProvenanceMetadata>& macro_prov,
    OwnershipSource source) {
    
    if (!macro_prov) {
        return create_inferred_ownership_provenance(source);
    }
    
    // Determine confidence based on origin
    float confidence = 1.0f;
    if (macro_prov->ai_info) {
        confidence = macro_prov->ai_info->confidence;
    } else if (macro_prov->origin == macro::CodeOrigin::Compiler) {
        confidence = 0.95f;
    }
    
    // Create ownership provenance
    auto ownership_prov = std::make_shared<OwnershipProvenance>(
        macro_prov->origin,
        source,
        confidence
    );
    
    ownership_prov->author = macro_prov->author;
    ownership_prov->timestamp = macro_prov->timestamp;
    ownership_prov->ai_info = macro_prov->ai_info;
    
    // Convert parent if present
    if (macro_prov->parent) {
        ownership_prov->parent = convert_to_ownership_provenance(
            macro_prov->parent,
            source
        );
    }
    
    return ownership_prov;
}

std::expected<void, std::string> validate_ownership_provenance(
    const OwnershipProvenance& provenance) {
    
    // Validate confidence
    if (provenance.confidence < 0.0f || provenance.confidence > 1.0f) {
        return std::unexpected("Confidence must be between 0.0 and 1.0");
    }
    
    // Validate AI info if present
    if (provenance.ai_info) {
        if (provenance.ai_info->confidence < 0.0f || 
            provenance.ai_info->confidence > 1.0f) {
            return std::unexpected("AI confidence must be between 0.0 and 1.0");
        }
    }
    
    // Validate author
    if (provenance.author.empty()) {
        return std::unexpected("Author cannot be empty");
    }
    
    return {};
}

std::string serialize_ownership_provenance(const OwnershipProvenance& provenance) {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"origin\": \"" << macro::code_origin_to_string(provenance.origin) << "\",\n";
    oss << "  \"ownership_source\": \"" << ownership_source_to_string(provenance.ownership_source) << "\",\n";
    oss << "  \"confidence\": " << provenance.confidence << ",\n";
    oss << "  \"author\": \"" << provenance.author << "\",\n";
    
    if (provenance.ai_info) {
        oss << "  \"ai_info\": {\n";
        oss << "    \"model_name\": \"" << provenance.ai_info->model_name << "\",\n";
        oss << "    \"version\": \"" << provenance.ai_info->version << "\",\n";
        oss << "    \"confidence\": " << provenance.ai_info->confidence << "\n";
        oss << "  },\n";
    }
    
    oss << "  \"borrow_history_count\": " << provenance.borrow_history.size() << ",\n";
    oss << "  \"has_parent\": " << (provenance.parent ? "true" : "false") << "\n";
    oss << "}";
    
    return oss.str();
}

std::string visualize_ownership_provenance_chain(
    const std::vector<std::shared_ptr<OwnershipProvenance>>& chain) {
    
    std::ostringstream oss;
    
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i > 0) {
            oss << " -> ";
        }
        
        const auto& prov = chain[i];
        oss << macro::code_origin_to_string(prov->origin);
        oss << "(" << ownership_source_to_string(prov->ownership_source) << ")";
        oss << "[" << (prov->confidence * 100.0f) << "%]";
    }
    
    return oss.str();
}

} // namespace meld::ownership
