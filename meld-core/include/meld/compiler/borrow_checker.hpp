#pragma once

#include "meld/parser/ast.hpp"
#include "meld/provenance/provenance.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <expected>

namespace meld::compiler {

// Forward declarations
struct LifetimeId;
struct BorrowInfo;
struct OwnershipInfo;
struct BorrowError;

// Lifetime identifier for tracking reference lifetimes
struct LifetimeId {
    size_t id;
    std::string name;  // Optional human-readable name
    
    LifetimeId() : id(0) {}
    LifetimeId(size_t id, const std::string& name = "") : id(id), name(name) {}
    
    bool operator==(const LifetimeId& other) const { return id == other.id; }
    bool operator<(const LifetimeId& other) const { return id < other.id; }
};

// Types of borrowing operations
enum class BorrowType {
    Immutable,  // &T - shared reference
    Mutable     // &mut T - exclusive reference
};

// Information about an active borrow
struct BorrowInfo {
    LifetimeId lifetime;
    BorrowType borrow_type;
    parser::ast::identifier source_location;  // Where the borrow originated
    provenance::ProvenanceMetadata provenance;  // AI vs human generated
    std::string borrowed_symbol;  // Name of the borrowed variable
    
    BorrowInfo() = default;
    BorrowInfo(LifetimeId lifetime, BorrowType type, const parser::ast::identifier& loc, 
               const std::string& symbol)
        : lifetime(lifetime), borrow_type(type), source_location(loc), borrowed_symbol(symbol) {}
};

// Ownership information for variables
struct OwnershipInfo {
    bool is_owned;           // True if this variable owns its data
    bool is_moved;           // True if this variable has been moved from
    bool is_copyable;        // True if the type implements Copy trait
    std::optional<LifetimeId> lifetime;  // Lifetime if this is a reference
    provenance::ProvenanceMetadata provenance;  // Tracking for AI-generated code
    
    OwnershipInfo(bool owned = true, bool copyable = false) 
        : is_owned(owned), is_moved(false), is_copyable(copyable) {}
};

// Borrow checking errors
struct BorrowError {
    enum class Type {
        UseAfterMove,
        MutableBorrowWhileImmutableExists,
        MultipleMutableBorrows,
        BorrowOutlivesOwner,
        MoveWhileBorrowed
    };
    
    Type error_type;
    std::string message;
    parser::ast::identifier location;
    std::string context;
    
    BorrowError(Type type, const std::string& msg, const parser::ast::identifier& loc, 
                const std::string& ctx = "")
        : error_type(type), message(msg), location(loc), context(ctx) {}
};

// Lifetime constraint for inference
struct LifetimeConstraint {
    LifetimeId shorter;
    LifetimeId longer;
    parser::ast::identifier source_location;
    std::string reason;
    
    LifetimeConstraint(LifetimeId shorter, LifetimeId longer, 
                      const parser::ast::identifier& loc, const std::string& reason)
        : shorter(shorter), longer(longer), source_location(loc), reason(reason) {}
};

// Lifetime assignment result
struct LifetimeAssignment {
    std::unordered_map<size_t, LifetimeId> assignments;
    std::vector<LifetimeConstraint> constraints;
};

// Lifetime inference engine
class LifetimeInference {
public:
    LifetimeInference();
    
    // Generate lifetime constraints from function AST
    std::vector<LifetimeConstraint> infer_constraints(
        const parser::ast::function_definition& function
    );
    
    // Solve lifetime constraints
    std::expected<LifetimeAssignment, BorrowError> solve_constraints(
        const std::vector<LifetimeConstraint>& constraints
    );
    
    // Create a new lifetime identifier
    LifetimeId create_lifetime(const std::string& name = "");
    
private:
    size_t next_lifetime_id_;
    std::unordered_map<std::string, LifetimeId> named_lifetimes_;
    
    // Helper methods for constraint generation
    void analyze_function_signature(
        const parser::ast::function_definition& function,
        std::vector<LifetimeConstraint>& constraints
    );
    
    void analyze_function_body(
        const parser::ast::block_expression& body,
        std::vector<LifetimeConstraint>& constraints
    );
    
    void analyze_expression(
        const parser::ast::expression& expr,
        std::vector<LifetimeConstraint>& constraints
    );
};

// Main borrow checker
class BorrowChecker {
public:
    BorrowChecker();
    
    // Analyze a function for ownership and borrowing violations
    std::expected<void, BorrowError> check_function(
        const parser::ast::function_definition& function
    );
    
    // Check a single expression
    std::expected<void, BorrowError> check_expression(
        const parser::ast::expression& expr
    );
    
    // Track AI-generated borrows with provenance
    void track_ai_generated_borrows(
        const parser::ast::expression& node, 
        const provenance::ProvenanceMetadata& provenance
    );
    
    // Get ownership information for a symbol
    std::optional<OwnershipInfo> get_ownership_info(const std::string& symbol) const;
    
    // Set ownership information for a symbol
    void set_ownership_info(const std::string& symbol, const OwnershipInfo& info);
    
    // Check if a symbol is currently borrowed
    bool is_borrowed(const std::string& symbol) const;
    
    // Get active borrows for a symbol
    std::vector<BorrowInfo> get_active_borrows(const std::string& symbol) const;
    
    // Enhanced static analysis methods for Task 2.3
    
    // Use-after-move detection (Requirement 1.2)
    std::expected<void, BorrowError> analyze_use_after_move(
        const parser::ast::expression& expr
    );
    
    // Exclusive mutable access enforcement (Requirement 1.3)
    std::expected<void, BorrowError> enforce_exclusive_mutable_access(
        const std::string& symbol,
        const parser::ast::identifier& location
    );
    
    // Compile-time data race detection (Requirement 1.5)
    std::expected<void, BorrowError> detect_data_races(
        const parser::ast::function_definition& function
    );
    
    // Advanced lifetime analysis
    std::expected<void, BorrowError> analyze_lifetime_relationships(
        const parser::ast::expression& expr
    );
    
    // Concurrent access pattern analysis
    std::expected<void, BorrowError> analyze_concurrent_access_patterns(
        const parser::ast::function_definition& function
    );
    
    // Enhanced borrow scope tracking
    void enter_scope();
    void exit_scope();
    void cleanup_expired_borrows(LifetimeId scope_lifetime);
    
private:
    // Symbol table for ownership tracking
    std::unordered_map<std::string, OwnershipInfo> ownership_table_;
    
    // Active borrows tracking
    std::unordered_map<std::string, std::vector<BorrowInfo>> active_borrows_;
    
    // Lifetime inference engine
    std::unique_ptr<LifetimeInference> lifetime_inference_;
    
    // Current function being analyzed (for context)
    const parser::ast::function_definition* current_function_;
    
    // Scope tracking for enhanced borrow analysis
    std::vector<LifetimeId> scope_stack_;
    size_t current_scope_depth_;
    
    // Helper methods for different expression types
    std::expected<void, BorrowError> check_val_declaration(
        const parser::ast::val_declaration& decl
    );
    
    std::expected<void, BorrowError> check_var_declaration(
        const parser::ast::var_declaration& decl
    );
    
    std::expected<void, BorrowError> check_function_call(
        const parser::ast::function_call& call
    );
    
    std::expected<void, BorrowError> check_binary_operation(
        const parser::ast::binary_operation& op
    );
    
    std::expected<void, BorrowError> check_identifier_usage(
        const parser::ast::identifier& id
    );
    
    // Ownership transfer operations
    std::expected<void, BorrowError> move_value(
        const std::string& symbol,
        const parser::ast::identifier& location
    );
    
    std::expected<void, BorrowError> borrow_value(
        const std::string& symbol,
        BorrowType borrow_type,
        LifetimeId lifetime,
        const parser::ast::identifier& location
    );
    
    // Validation helpers
    bool can_move(const std::string& symbol) const;
    bool can_borrow(const std::string& symbol, BorrowType borrow_type) const;
    
    // Error creation helpers
    BorrowError make_use_after_move_error(
        const std::string& symbol,
        const parser::ast::identifier& location
    ) const;
    
    BorrowError make_borrow_conflict_error(
        const std::string& symbol,
        BorrowType attempted_borrow,
        const parser::ast::identifier& location
    ) const;
    
    // Helper method for concurrent access analysis
    std::expected<void, BorrowError> analyze_expression_for_concurrency(
        const parser::ast::expression& expr,
        const std::vector<std::string>& concurrent_indicators
    );
};

} // namespace meld::compiler

// Hash specialization for LifetimeId
namespace std {
    template<>
    struct hash<meld::compiler::LifetimeId> {
        size_t operator()(const meld::compiler::LifetimeId& lifetime) const {
            return hash<size_t>()(lifetime.id);
        }
    };
}