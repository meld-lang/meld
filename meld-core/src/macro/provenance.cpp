#include "meld/macro/provenance.hpp"
#include "meld/kernel/operations.hpp"
#include <format>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace meld::macro {

// ============================================================================
// ProvenanceTracker Implementation
// ============================================================================

void ProvenanceTracker::set_provenance(
    const kernel::Value& ast_node,
    std::shared_ptr<ProvenanceMetadata> provenance) {
    
    size_t node_id = get_node_id(ast_node);
    provenance_map_[node_id] = std::move(provenance);
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::get_provenance(const kernel::Value& ast_node) const {
    size_t node_id = get_node_id(ast_node);
    auto it = provenance_map_.find(node_id);
    if (it != provenance_map_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ProvenanceTracker::has_provenance(const kernel::Value& ast_node) const {
    size_t node_id = get_node_id(ast_node);
    return provenance_map_.contains(node_id);
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::create_human_provenance(const std::string& author) {
    auto provenance = std::make_shared<ProvenanceMetadata>(CodeOrigin::Human);
    provenance->author = author;
    return provenance;
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::create_ai_provenance(
    const AIModelInfo& ai_info,
    std::shared_ptr<ProvenanceMetadata> parent) {
    
    auto provenance = std::make_shared<ProvenanceMetadata>(CodeOrigin::AIGenerated);
    provenance->ai_info = ai_info;
    provenance->author = ai_info.model_name;
    provenance->parent = parent;
    return provenance;
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::create_macro_provenance(
    const MacroExpansionInfo& macro_info,
    std::shared_ptr<ProvenanceMetadata> parent) {
    
    auto provenance = std::make_shared<ProvenanceMetadata>(CodeOrigin::MacroExpanded);
    provenance->macro_info = macro_info;
    provenance->author = "macro:" + macro_info.macro_name;
    provenance->parent = parent;
    return provenance;
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::create_derived_provenance(
    const DerivationInfo& derive_info,
    std::shared_ptr<ProvenanceMetadata> parent) {
    
    auto provenance = std::make_shared<ProvenanceMetadata>(CodeOrigin::Derived);
    provenance->derive_info = derive_info;
    provenance->author = "derive:" + derive_info.trait_name;
    provenance->parent = parent;
    return provenance;
}

std::shared_ptr<ProvenanceMetadata> 
ProvenanceTracker::merge_provenance(
    const std::vector<std::shared_ptr<ProvenanceMetadata>>& provenances) {
    
    if (provenances.empty()) {
        return std::make_shared<ProvenanceMetadata>(CodeOrigin::Unknown);
    }
    
    if (provenances.size() == 1) {
        return provenances[0];
    }
    
    // Create merged provenance
    auto merged = std::make_shared<ProvenanceMetadata>(CodeOrigin::Compiler);
    merged->author = "merged";
    
    // Collect all tags and metadata
    for (const auto& prov : provenances) {
        if (prov) {
            merged->tags.insert(merged->tags.end(), prov->tags.begin(), prov->tags.end());
            merged->metadata.insert(prov->metadata.begin(), prov->metadata.end());
        }
    }
    
    // Use earliest timestamp
    merged->timestamp = provenances[0]->timestamp;
    for (const auto& prov : provenances) {
        if (prov && prov->timestamp < merged->timestamp) {
            merged->timestamp = prov->timestamp;
        }
    }
    
    return merged;
}

void ProvenanceTracker::clear() {
    provenance_map_.clear();
}

size_t ProvenanceTracker::get_node_id(const kernel::Value& ast_node) const {
    // Generate a unique ID for the AST node
    // In a full implementation, this would use a proper hash or unique identifier
    return std::hash<const void*>{}(static_cast<const void*>(&ast_node));
}

// ============================================================================
// ProvenanceAwareMacroExpander Implementation
// ============================================================================

std::expected<kernel::Value, std::string> 
ProvenanceAwareMacroExpander::expand_with_provenance(
    const kernel::Value& ast_node,
    const std::string& macro_name,
    const std::vector<std::string>& args) {
    
    // Get parent provenance
    auto parent_prov = tracker_.get_provenance(ast_node);
    
    // Create macro expansion info
    MacroExpansionInfo macro_info(
        macro_name,
        args,
        parent_prov ? (parent_prov->macro_info ? parent_prov->macro_info->expansion_depth + 1 : 1) : 0,
        "unknown"  // In a full implementation, would get actual source location
    );
    
    // Create provenance for expanded code
    auto provenance = ProvenanceTracker::create_macro_provenance(macro_info, parent_prov);
    
    // Perform macro expansion (placeholder)
    // In a full implementation, this would call the actual macro expander
    auto expanded = ast_node;  // Placeholder
    
    // Associate provenance with expanded code
    tracker_.set_provenance(expanded, provenance);
    
    return expanded;
}

std::expected<kernel::Value, std::string> 
ProvenanceAwareMacroExpander::derive_with_provenance(
    const kernel::Value& ast_node,
    const std::string& trait_name,
    const std::string& type_name) {
    
    // Get parent provenance
    auto parent_prov = tracker_.get_provenance(ast_node);
    
    // Create derivation info
    DerivationInfo derive_info(trait_name, type_name, {});
    
    // Create provenance for derived code
    auto provenance = ProvenanceTracker::create_derived_provenance(derive_info, parent_prov);
    
    // Perform derivation (placeholder)
    auto derived = ast_node;  // Placeholder
    
    // Associate provenance with derived code
    tracker_.set_provenance(derived, provenance);
    
    return derived;
}

std::expected<kernel::Value, std::string> 
ProvenanceAwareMacroExpander::apply_attribute_with_provenance(
    const kernel::Value& ast_node,
    const std::string& attribute_name) {
    
    // Get parent provenance
    auto parent_prov = tracker_.get_provenance(ast_node);
    
    // Create macro expansion info for attribute
    MacroExpansionInfo macro_info(
        "attribute:" + attribute_name,
        {},
        0,
        "unknown"
    );
    
    // Create provenance
    auto provenance = ProvenanceTracker::create_macro_provenance(macro_info, parent_prov);
    
    // Apply attribute (placeholder)
    auto result = ast_node;  // Placeholder
    
    // Associate provenance
    tracker_.set_provenance(result, provenance);
    
    return result;
}

// ============================================================================
// ProvenanceQuery Implementation
// ============================================================================

std::vector<kernel::Value> ProvenanceQuery::find_ai_generated() const {
    // In a full implementation, would search through all tracked nodes
    return {};
}

std::vector<kernel::Value> ProvenanceQuery::find_macro_expanded() const {
    return {};
}

std::vector<kernel::Value> ProvenanceQuery::find_derived() const {
    return {};
}

std::vector<kernel::Value> ProvenanceQuery::find_by_author(const std::string& author) const {
    return {};
}

std::vector<kernel::Value> ProvenanceQuery::find_by_ai_model(const std::string& model_name) const {
    return {};
}

std::vector<kernel::Value> ProvenanceQuery::find_by_macro(const std::string& macro_name) const {
    return {};
}

std::vector<std::shared_ptr<ProvenanceMetadata>> 
ProvenanceQuery::get_provenance_chain(const kernel::Value& ast_node) const {
    std::vector<std::shared_ptr<ProvenanceMetadata>> chain;
    
    auto current = tracker_.get_provenance(ast_node);
    while (current) {
        chain.push_back(current);
        current = current->parent;
    }
    
    // Reverse to get root-to-current order
    std::reverse(chain.begin(), chain.end());
    
    return chain;
}

float ProvenanceQuery::calculate_confidence(const kernel::Value& ast_node) const {
    auto provenance = tracker_.get_provenance(ast_node);
    if (!provenance) {
        return 0.0f;
    }
    
    // Calculate confidence based on origin
    switch (provenance->origin) {
        case CodeOrigin::Human:
            return 1.0f;
        
        case CodeOrigin::AIGenerated:
            if (provenance->ai_info) {
                return provenance->ai_info->confidence;
            }
            return 0.5f;
        
        case CodeOrigin::MacroExpanded:
        case CodeOrigin::Derived:
            // Inherit confidence from parent
            if (provenance->parent) {
                return calculate_confidence(ast_node) * 0.95f;  // Slight degradation
            }
            return 0.9f;
        
        case CodeOrigin::Compiler:
            return 1.0f;
        
        case CodeOrigin::Unknown:
        default:
            return 0.0f;
    }
}

bool ProvenanceQuery::is_trustworthy(const kernel::Value& ast_node, float threshold) const {
    return calculate_confidence(ast_node) >= threshold;
}

// ============================================================================
// ProvenanceSerialization Implementation
// ============================================================================

std::string ProvenanceSerialization::to_json(const ProvenanceMetadata& provenance) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"origin\": \"" << code_origin_to_string(provenance.origin) << "\",\n";
    oss << "  \"author\": \"" << provenance.author << "\",\n";
    
    // Add AI info if present
    if (provenance.ai_info) {
        oss << "  \"ai_info\": {\n";
        oss << "    \"model_name\": \"" << provenance.ai_info->model_name << "\",\n";
        oss << "    \"version\": \"" << provenance.ai_info->version << "\",\n";
        oss << "    \"confidence\": " << provenance.ai_info->confidence << "\n";
        oss << "  },\n";
    }
    
    // Add macro info if present
    if (provenance.macro_info) {
        oss << "  \"macro_info\": {\n";
        oss << "    \"macro_name\": \"" << provenance.macro_info->macro_name << "\",\n";
        oss << "    \"expansion_depth\": " << provenance.macro_info->expansion_depth << "\n";
        oss << "  },\n";
    }
    
    // Add derive info if present
    if (provenance.derive_info) {
        oss << "  \"derive_info\": {\n";
        oss << "    \"trait_name\": \"" << provenance.derive_info->trait_name << "\",\n";
        oss << "    \"source_type\": \"" << provenance.derive_info->source_type << "\"\n";
        oss << "  },\n";
    }
    
    oss << "  \"has_parent\": " << (provenance.parent ? "true" : "false") << "\n";
    oss << "}";
    
    return oss.str();
}

std::expected<ProvenanceMetadata, std::string> 
ProvenanceSerialization::from_json(const std::string& json) {
    // Placeholder implementation
    // In a full implementation, would parse JSON
    return std::unexpected("JSON parsing not implemented");
}

std::vector<uint8_t> ProvenanceSerialization::to_binary(const ProvenanceMetadata& provenance) {
    // Placeholder implementation
    return {};
}

std::expected<ProvenanceMetadata, std::string> 
ProvenanceSerialization::from_binary(const std::vector<uint8_t>& data) {
    return std::unexpected("Binary parsing not implemented");
}

std::string ProvenanceSerialization::to_readable(const ProvenanceMetadata& provenance) {
    std::ostringstream oss;
    
    oss << "Provenance Information:\n";
    oss << "  Origin: " << code_origin_to_string(provenance.origin) << "\n";
    oss << "  Author: " << provenance.author << "\n";
    
    if (provenance.ai_info) {
        oss << "  AI Model: " << provenance.ai_info->model_name 
            << " (v" << provenance.ai_info->version << ")\n";
        oss << "  Confidence: " << (provenance.ai_info->confidence * 100.0f) << "%\n";
    }
    
    if (provenance.macro_info) {
        oss << "  Macro: " << provenance.macro_info->macro_name << "\n";
        oss << "  Expansion Depth: " << provenance.macro_info->expansion_depth << "\n";
    }
    
    if (provenance.derive_info) {
        oss << "  Derived Trait: " << provenance.derive_info->trait_name << "\n";
        oss << "  Source Type: " << provenance.derive_info->source_type << "\n";
    }
    
    if (provenance.parent) {
        oss << "  Has Parent: Yes\n";
    }
    
    return oss.str();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string code_origin_to_string(CodeOrigin origin) {
    switch (origin) {
        case CodeOrigin::Human: return "Human";
        case CodeOrigin::AIGenerated: return "AIGenerated";
        case CodeOrigin::MacroExpanded: return "MacroExpanded";
        case CodeOrigin::Derived: return "Derived";
        case CodeOrigin::Compiler: return "Compiler";
        case CodeOrigin::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

std::expected<CodeOrigin, std::string> parse_code_origin(const std::string& str) {
    if (str == "Human") return CodeOrigin::Human;
    if (str == "AIGenerated") return CodeOrigin::AIGenerated;
    if (str == "MacroExpanded") return CodeOrigin::MacroExpanded;
    if (str == "Derived") return CodeOrigin::Derived;
    if (str == "Compiler") return CodeOrigin::Compiler;
    if (str == "Unknown") return CodeOrigin::Unknown;
    
    return std::unexpected("Unknown code origin: " + str);
}

std::string visualize_provenance_chain(
    const std::vector<std::shared_ptr<ProvenanceMetadata>>& chain) {
    
    std::ostringstream oss;
    
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i > 0) {
            oss << " -> ";
        }
        
        const auto& prov = chain[i];
        oss << code_origin_to_string(prov->origin);
        
        if (prov->ai_info) {
            oss << "(" << prov->ai_info->model_name << ")";
        } else if (prov->macro_info) {
            oss << "(" << prov->macro_info->macro_name << ")";
        } else if (prov->derive_info) {
            oss << "(" << prov->derive_info->trait_name << ")";
        }
    }
    
    return oss.str();
}

std::expected<void, std::string> validate_provenance(const ProvenanceMetadata& provenance) {
    // Validate AI info if present
    if (provenance.ai_info) {
        if (provenance.ai_info->confidence < 0.0f || provenance.ai_info->confidence > 1.0f) {
            return std::unexpected("AI confidence must be between 0.0 and 1.0");
        }
    }
    
    // Validate macro info if present
    if (provenance.macro_info) {
        if (provenance.macro_info->macro_name.empty()) {
            return std::unexpected("Macro name cannot be empty");
        }
    }
    
    // Validate derive info if present
    if (provenance.derive_info) {
        if (provenance.derive_info->trait_name.empty()) {
            return std::unexpected("Trait name cannot be empty");
        }
    }
    
    return {};
}

void copy_provenance(const kernel::Value& source,
                    const kernel::Value& target,
                    ProvenanceTracker& tracker) {
    
    auto provenance = tracker.get_provenance(source);
    if (provenance) {
        tracker.set_provenance(target, provenance);
    }
}

std::shared_ptr<ProvenanceMetadata> merge_node_provenance(
    const std::vector<kernel::Value>& nodes,
    const ProvenanceTracker& tracker) {
    
    std::vector<std::shared_ptr<ProvenanceMetadata>> provenances;
    
    for (const auto& node : nodes) {
        auto prov = tracker.get_provenance(node);
        if (prov) {
            provenances.push_back(prov);
        }
    }
    
    return ProvenanceTracker::merge_provenance(provenances);
}

} // namespace meld::macro
