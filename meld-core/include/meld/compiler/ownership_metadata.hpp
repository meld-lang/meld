#pragma once

#include "meld/parser/ast.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>

namespace meld::compiler {

// Forward declarations
class OwnershipMetadataManager;

// Ownership metadata that can be attached to AST nodes
struct OwnershipMetadata {
    // Basic ownership information
    bool is_owned = true;           // Whether this expression owns its value
    bool is_moved = false;          // Whether this value has been moved
    bool is_borrowed = false;       // Whether this is a borrowed reference
    
    // Borrowing information
    std::optional<BorrowType> borrow_type;  // Type of borrow if borrowed
    std::optional<LifetimeId> lifetime;     // Lifetime if this is a reference
    
    // Move semantics
    bool consumes_operand = false;  // Whether this operation consumes its operand
    std::vector<std::string> moved_variables;  // Variables moved by this expression
    
    // Provenance tracking for AI-generated code
    provenance::ProvenanceMetadata provenance;
    
    // Source information
    std::string source_symbol;     // Original variable name if applicable
    parser::ast::identifier source_location;  // Where this ownership originated
    
    OwnershipMetadata() = default;
    
    OwnershipMetadata(bool owned, const std::string& symbol = "")
        : is_owned(owned), source_symbol(symbol) {}
    
    // Create metadata for a moved value
    static OwnershipMetadata create_moved(const std::string& symbol, 
                                         const parser::ast::identifier& location) {
        OwnershipMetadata metadata(false, symbol);
        metadata.is_moved = true;
        metadata.source_location = location;
        return metadata;
    }
    
    // Create metadata for a borrowed value
    static OwnershipMetadata create_borrowed(const std::string& symbol,
                                           BorrowType borrow_type,
                                           LifetimeId lifetime,
                                           const parser::ast::identifier& location) {
        OwnershipMetadata metadata(false, symbol);
        metadata.is_borrowed = true;
        metadata.borrow_type = borrow_type;
        metadata.lifetime = lifetime;
        metadata.source_location = location;
        return metadata;
    }
};

// Manager for ownership metadata attached to AST nodes
class OwnershipMetadataManager {
public:
    OwnershipMetadataManager();
    ~OwnershipMetadataManager();
    
    // Attach ownership metadata to an AST node
    void set_metadata(const parser::ast::expression& node, const OwnershipMetadata& metadata);
    
    // Get ownership metadata for an AST node
    std::optional<OwnershipMetadata> get_metadata(const parser::ast::expression& node) const;
    
    // Check if an AST node has ownership metadata
    bool has_metadata(const parser::ast::expression& node) const;
    
    // Remove metadata for an AST node
    void remove_metadata(const parser::ast::expression& node);
    
    // Clear all metadata
    void clear();
    
    // Get metadata for a specific identifier
    std::optional<OwnershipMetadata> get_identifier_metadata(const std::string& name) const;
    
    // Set metadata for a specific identifier
    void set_identifier_metadata(const std::string& name, const OwnershipMetadata& metadata);
    
    // Track variable moves
    void track_move(const std::string& variable, const parser::ast::identifier& location);
    
    // Track variable borrows
    void track_borrow(const std::string& variable, BorrowType borrow_type, 
                     LifetimeId lifetime, const parser::ast::identifier& location);
    
    // Get all moved variables in current scope
    std::vector<std::string> get_moved_variables() const;
    
    // Get all borrowed variables in current scope
    std::vector<std::pair<std::string, BorrowInfo>> get_borrowed_variables() const;
    
    // Integration with AI provenance
    void mark_ai_generated(const parser::ast::expression& node, 
                          const provenance::ProvenanceMetadata& provenance);
    
    // Check if a node was AI-generated
    bool is_ai_generated(const parser::ast::expression& node) const;
    
    // Get provenance information for a node
    std::optional<provenance::ProvenanceMetadata> get_provenance(
        const parser::ast::expression& node) const;

private:
    // Map from AST node addresses to ownership metadata
    std::unordered_map<const void*, OwnershipMetadata> node_metadata_;
    
    // Map from identifier names to ownership metadata
    std::unordered_map<std::string, OwnershipMetadata> identifier_metadata_;
    
    // Track moved variables
    std::unordered_map<std::string, parser::ast::identifier> moved_variables_;
    
    // Track borrowed variables
    std::unordered_map<std::string, BorrowInfo> borrowed_variables_;
    
    // Helper to get unique identifier for AST node
    const void* get_node_id(const parser::ast::expression& node) const;
};

// RAII helper for ownership scope management
class OwnershipScope {
public:
    OwnershipScope(OwnershipMetadataManager& manager);
    ~OwnershipScope();
    
    // Enter a new scope (e.g., function body, block)
    void enter_scope();
    
    // Exit current scope and clean up metadata
    void exit_scope();
    
    // Mark a variable as moved in current scope
    void move_variable(const std::string& name, const parser::ast::identifier& location);
    
    // Mark a variable as borrowed in current scope
    void borrow_variable(const std::string& name, BorrowType borrow_type,
                        LifetimeId lifetime, const parser::ast::identifier& location);

private:
    OwnershipMetadataManager& manager_;
    std::vector<std::unordered_set<std::string>> scope_stack_;
    size_t current_scope_level_;
};

// Utility functions for working with ownership metadata

// Check if an expression transfers ownership
bool transfers_ownership(const parser::ast::expression& expr, 
                        const OwnershipMetadataManager& manager);

// Check if an expression borrows a value
bool borrows_value(const parser::ast::expression& expr,
                  const OwnershipMetadataManager& manager);

// Get the lifetime of an expression if it's a reference
std::optional<LifetimeId> get_expression_lifetime(const parser::ast::expression& expr,
                                                 const OwnershipMetadataManager& manager);

// Check if two expressions have compatible ownership
bool ownership_compatible(const parser::ast::expression& lhs,
                         const parser::ast::expression& rhs,
                         const OwnershipMetadataManager& manager);

// Extract all identifiers that are moved by an expression
std::vector<std::string> extract_moved_identifiers(const parser::ast::expression& expr,
                                                   const OwnershipMetadataManager& manager);

// Extract all identifiers that are borrowed by an expression
std::vector<std::pair<std::string, BorrowType>> extract_borrowed_identifiers(
    const parser::ast::expression& expr,
    const OwnershipMetadataManager& manager);

} // namespace meld::compiler