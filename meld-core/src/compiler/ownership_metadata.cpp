#include "meld/compiler/ownership_metadata.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

// OwnershipMetadataManager implementation
OwnershipMetadataManager::OwnershipMetadataManager() = default;
OwnershipMetadataManager::~OwnershipMetadataManager() = default;

void OwnershipMetadataManager::set_metadata(const parser::ast::expression& node, 
                                           const OwnershipMetadata& metadata) {
    const void* node_id = get_node_id(node);
    node_metadata_[node_id] = metadata;
}

std::optional<OwnershipMetadata> OwnershipMetadataManager::get_metadata(
    const parser::ast::expression& node) const {
    
    const void* node_id = get_node_id(node);
    auto it = node_metadata_.find(node_id);
    if (it != node_metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool OwnershipMetadataManager::has_metadata(const parser::ast::expression& node) const {
    const void* node_id = get_node_id(node);
    return node_metadata_.find(node_id) != node_metadata_.end();
}

void OwnershipMetadataManager::remove_metadata(const parser::ast::expression& node) {
    const void* node_id = get_node_id(node);
    node_metadata_.erase(node_id);
}

void OwnershipMetadataManager::clear() {
    node_metadata_.clear();
    identifier_metadata_.clear();
    moved_variables_.clear();
    borrowed_variables_.clear();
}

std::optional<OwnershipMetadata> OwnershipMetadataManager::get_identifier_metadata(
    const std::string& name) const {
    
    auto it = identifier_metadata_.find(name);
    if (it != identifier_metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void OwnershipMetadataManager::set_identifier_metadata(const std::string& name, 
                                                      const OwnershipMetadata& metadata) {
    identifier_metadata_[name] = metadata;
}

void OwnershipMetadataManager::track_move(const std::string& variable, 
                                         const parser::ast::identifier& location) {
    moved_variables_[variable] = location;
    
    // Update identifier metadata to mark as moved
    auto metadata = get_identifier_metadata(variable);
    if (metadata) {
        metadata->is_moved = true;
        metadata->is_owned = false;
        set_identifier_metadata(variable, *metadata);
    } else {
        // Create new metadata for moved variable
        OwnershipMetadata new_metadata = OwnershipMetadata::create_moved(variable, location);
        set_identifier_metadata(variable, new_metadata);
    }
}

void OwnershipMetadataManager::track_borrow(const std::string& variable, 
                                           BorrowType borrow_type,
                                           LifetimeId lifetime, 
                                           const parser::ast::identifier& location) {
    BorrowInfo borrow_info(lifetime, borrow_type, location, variable);
    borrowed_variables_[variable] = borrow_info;
    
    // Update identifier metadata to mark as borrowed
    auto metadata = get_identifier_metadata(variable);
    if (metadata) {
        metadata->is_borrowed = true;
        metadata->borrow_type = borrow_type;
        metadata->lifetime = lifetime;
        set_identifier_metadata(variable, *metadata);
    } else {
        // Create new metadata for borrowed variable
        OwnershipMetadata new_metadata = OwnershipMetadata::create_borrowed(
            variable, borrow_type, lifetime, location);
        set_identifier_metadata(variable, new_metadata);
    }
}

std::vector<std::string> OwnershipMetadataManager::get_moved_variables() const {
    std::vector<std::string> result;
    result.reserve(moved_variables_.size());
    
    for (const auto& [variable, _] : moved_variables_) {
        result.push_back(variable);
    }
    
    return result;
}

std::vector<std::pair<std::string, BorrowInfo>> OwnershipMetadataManager::get_borrowed_variables() const {
    std::vector<std::pair<std::string, BorrowInfo>> result;
    result.reserve(borrowed_variables_.size());
    
    for (const auto& [variable, borrow_info] : borrowed_variables_) {
        result.emplace_back(variable, borrow_info);
    }
    
    return result;
}

void OwnershipMetadataManager::mark_ai_generated(const parser::ast::expression& node, 
                                                const provenance::ProvenanceMetadata& provenance) {
    auto metadata = get_metadata(node);
    if (metadata) {
        metadata->provenance = provenance;
        set_metadata(node, *metadata);
    } else {
        OwnershipMetadata new_metadata;
        new_metadata.provenance = provenance;
        set_metadata(node, new_metadata);
    }
}

bool OwnershipMetadataManager::is_ai_generated(const parser::ast::expression& node) const {
    auto metadata = get_metadata(node);
    if (metadata) {
        // Check if provenance indicates AI generation
        // This would depend on the specific provenance system implementation
        return metadata->provenance.origin == provenance::OriginType::Agent;
    }
    return false;
}

std::optional<provenance::ProvenanceMetadata> OwnershipMetadataManager::get_provenance(
    const parser::ast::expression& node) const {
    
    auto metadata = get_metadata(node);
    if (metadata) {
        return metadata->provenance;
    }
    return std::nullopt;
}

const void* OwnershipMetadataManager::get_node_id(const parser::ast::expression& node) const {
    // Use the address of the variant as a unique identifier
    // This works because AST nodes are typically stored in stable memory locations
    return meld::compat::visit<const void*>([](const auto& n) -> const void* {
        return static_cast<const void*>(&n);
    }, node);
}

// OwnershipScope implementation
OwnershipScope::OwnershipScope(OwnershipMetadataManager& manager) 
    : manager_(manager), current_scope_level_(0) {
    enter_scope();
}

OwnershipScope::~OwnershipScope() {
    // Clean up all scopes
    while (current_scope_level_ > 0) {
        exit_scope();
    }
}

void OwnershipScope::enter_scope() {
    scope_stack_.emplace_back();
    current_scope_level_++;
}

void OwnershipScope::exit_scope() {
    if (current_scope_level_ == 0) {
        return;  // No scope to exit
    }
    
    // Clean up metadata for variables in the current scope
    const auto& current_scope = scope_stack_.back();
    for (const std::string& variable : current_scope) {
        // Remove metadata for variables that go out of scope
        auto metadata = manager_.get_identifier_metadata(variable);
        if (metadata) {
            // Reset ownership state when exiting scope
            metadata->is_moved = false;
            metadata->is_borrowed = false;
            metadata->borrow_type = std::nullopt;
            metadata->lifetime = std::nullopt;
            manager_.set_identifier_metadata(variable, *metadata);
        }
    }
    
    scope_stack_.pop_back();
    current_scope_level_--;
}

void OwnershipScope::move_variable(const std::string& name, 
                                  const parser::ast::identifier& location) {
    manager_.track_move(name, location);
    
    // Add to current scope
    if (!scope_stack_.empty()) {
        scope_stack_.back().insert(name);
    }
}

void OwnershipScope::borrow_variable(const std::string& name, 
                                    BorrowType borrow_type,
                                    LifetimeId lifetime, 
                                    const parser::ast::identifier& location) {
    manager_.track_borrow(name, borrow_type, lifetime, location);
    
    // Add to current scope
    if (!scope_stack_.empty()) {
        scope_stack_.back().insert(name);
    }
}

// Utility functions
bool transfers_ownership(const parser::ast::expression& expr, 
                        const OwnershipMetadataManager& manager) {
    auto metadata = manager.get_metadata(expr);
    if (metadata) {
        return metadata->consumes_operand || metadata->is_moved;
    }
    
    // Check if this is an identifier that would transfer ownership
    return meld::compat::visit<bool>([&manager](const auto& node) -> bool {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            auto id_metadata = manager.get_identifier_metadata(node.name);
            return id_metadata && id_metadata->is_owned && !id_metadata->is_borrowed;
        }
        
        return false;
    }, expr);
}

bool borrows_value(const parser::ast::expression& expr,
                  const OwnershipMetadataManager& manager) {
    auto metadata = manager.get_metadata(expr);
    if (metadata) {
        return metadata->is_borrowed;
    }
    
    return meld::compat::visit<bool>([&manager](const auto& node) -> bool {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            auto id_metadata = manager.get_identifier_metadata(node.name);
            return id_metadata && id_metadata->is_borrowed;
        }
        
        return false;
    }, expr);
}

std::optional<LifetimeId> get_expression_lifetime(const parser::ast::expression& expr,
                                                 const OwnershipMetadataManager& manager) {
    auto metadata = manager.get_metadata(expr);
    if (metadata && metadata->lifetime) {
        return metadata->lifetime;
    }
    
    return meld::compat::visit<std::optional<LifetimeId>>([&manager](const auto& node) -> std::optional<LifetimeId> {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            auto id_metadata = manager.get_identifier_metadata(node.name);
            if (id_metadata && id_metadata->lifetime) {
                return id_metadata->lifetime;
            }
        }
        
        return std::nullopt;
    }, expr);
}

bool ownership_compatible(const parser::ast::expression& lhs,
                         const parser::ast::expression& rhs,
                         const OwnershipMetadataManager& manager) {
    auto lhs_metadata = manager.get_metadata(lhs);
    auto rhs_metadata = manager.get_metadata(rhs);
    
    // If either side doesn't have metadata, assume compatible for now
    if (!lhs_metadata || !rhs_metadata) {
        return true;
    }
    
    // Check basic ownership compatibility
    if (lhs_metadata->is_moved || rhs_metadata->is_moved) {
        return false;  // Can't use moved values
    }
    
    // Check borrow compatibility
    if (lhs_metadata->is_borrowed && rhs_metadata->is_borrowed) {
        // Both borrowed - check for mutable borrow conflicts
        if (lhs_metadata->borrow_type == BorrowType::Mutable || 
            rhs_metadata->borrow_type == BorrowType::Mutable) {
            return false;  // Mutable borrow conflicts with any other borrow
        }
    }
    
    return true;
}

std::vector<std::string> extract_moved_identifiers(const parser::ast::expression& expr,
                                                   const OwnershipMetadataManager& manager) {
    std::vector<std::string> moved_identifiers;
    
    auto metadata = manager.get_metadata(expr);
    if (metadata) {
        moved_identifiers = metadata->moved_variables;
    }
    
    // Also check if the expression itself is an identifier that gets moved
    meld::compat::visit([&moved_identifiers, &manager](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            if (transfers_ownership(parser::ast::expression(node), manager)) {
                moved_identifiers.push_back(node.name);
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Function calls might move their arguments
            for (const auto& arg : node.get().arguments) {
                auto arg_moved = extract_moved_identifiers(arg, manager);
                moved_identifiers.insert(moved_identifiers.end(), 
                                       arg_moved.begin(), arg_moved.end());
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // Binary operations might move their operands
            auto left_moved = extract_moved_identifiers(node.get().left, manager);
            auto right_moved = extract_moved_identifiers(node.get().right, manager);
            
            moved_identifiers.insert(moved_identifiers.end(), 
                                   left_moved.begin(), left_moved.end());
            moved_identifiers.insert(moved_identifiers.end(), 
                                   right_moved.begin(), right_moved.end());
        }
    }, expr);
    
    return moved_identifiers;
}

std::vector<std::pair<std::string, BorrowType>> extract_borrowed_identifiers(
    const parser::ast::expression& expr,
    const OwnershipMetadataManager& manager) {
    
    std::vector<std::pair<std::string, BorrowType>> borrowed_identifiers;
    
    meld::compat::visit([&borrowed_identifiers, &manager](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            auto id_metadata = manager.get_identifier_metadata(node.name);
            if (id_metadata && id_metadata->is_borrowed && id_metadata->borrow_type) {
                borrowed_identifiers.emplace_back(node.name, *id_metadata->borrow_type);
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Function calls might borrow their arguments
            for (const auto& arg : node.get().arguments) {
                auto arg_borrowed = extract_borrowed_identifiers(arg, manager);
                borrowed_identifiers.insert(borrowed_identifiers.end(), 
                                          arg_borrowed.begin(), arg_borrowed.end());
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // Binary operations might borrow their operands
            auto left_borrowed = extract_borrowed_identifiers(node.get().left, manager);
            auto right_borrowed = extract_borrowed_identifiers(node.get().right, manager);
            
            borrowed_identifiers.insert(borrowed_identifiers.end(), 
                                       left_borrowed.begin(), left_borrowed.end());
            borrowed_identifiers.insert(borrowed_identifiers.end(), 
                                       right_borrowed.begin(), right_borrowed.end());
        }
    }, expr);
    
    return borrowed_identifiers;
}

} // namespace meld::compiler