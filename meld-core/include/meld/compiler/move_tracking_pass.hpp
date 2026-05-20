#pragma once

/// @file move_tracking_pass.hpp
/// @brief Semantic Analyzer — Move Tracking Pass
///
/// Tracks the invalidation state of every local binding. When
/// std.mem.move(x) is encountered, marks x as MOVED. Subsequent
/// reads/writes to a moved binding emit E4002. Conditional moves
/// (moved in some but not all branches) produce CONDITIONALLY_MOVED.
///
/// Requirements: 4.2, 4.5, 7.2

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Binding state — per-binding invalidation tracking
// ---------------------------------------------------------------------------

/// State machine per binding:
///   LIVE → (move) → MOVED
///   LIVE → (conditional move in one branch) → CONDITIONALLY_MOVED
///   MOVED → (any access) → ERROR
///   CONDITIONALLY_MOVED → (any access) → ERROR
enum class BindingState {
    LIVE,                  ///< Binding is valid and accessible
    MOVED,                 ///< Binding has been moved; any access is an error
    CONDITIONALLY_MOVED    ///< Moved in some but not all branches
};

// ---------------------------------------------------------------------------
// Move tracking diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the move tracking pass.
struct MoveTrackingDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4002"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// Scope — tracks binding states within a lexical scope
// ---------------------------------------------------------------------------

/// A lexical scope that tracks binding states. Scopes nest: a child scope
/// inherits the parent's binding states and can shadow or modify them.
class BindingScope {
public:
    explicit BindingScope(BindingScope* parent = nullptr);

    /// Declare a new binding in this scope (starts as LIVE).
    void declare(const std::string& name);

    /// Get the state of a binding, searching up the scope chain.
    BindingState get_state(const std::string& name) const;

    /// Check if a binding exists in this scope or any parent.
    bool has_binding(const std::string& name) const;

    /// Mark a binding as MOVED.
    void mark_moved(const std::string& name);

    /// Mark a binding as CONDITIONALLY_MOVED.
    void mark_conditionally_moved(const std::string& name);

    /// Get all bindings that have been modified in this scope (not inherited).
    std::unordered_map<std::string, BindingState> local_states() const;

    /// Get the parent scope.
    BindingScope* parent() const { return parent_; }

private:
    BindingScope* parent_;
    std::unordered_map<std::string, BindingState> states_;
};

// ---------------------------------------------------------------------------
// MoveTrackingResult — output of the pass
// ---------------------------------------------------------------------------

struct MoveTrackingResult {
    bool success = true;
    std::vector<MoveTrackingDiagnostic> diagnostics;
    size_t bindings_tracked = 0;
    size_t moves_detected = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// MoveTrackingPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Move Tracking Pass scans function bodies for std.mem.move() calls
/// and tracks binding invalidation. It uses the IntrinsicResolutionRegistry
/// to identify move() calls via the memory_move intrinsic tag.
///
/// This pass runs after the Intrinsic Resolution Pass.
class MoveTrackingPass {
public:
    MoveTrackingPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param registry     The intrinsic registry (from IntrinsicResolutionPass).
    /// @param source_file  Source file path for diagnostics.
    MoveTrackingResult run(
        const std::vector<parser::ast::expression>& expressions,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file = ""
    );

    /// Check if a function call is a move() call using the registry.
    static bool is_move_call(
        const parser::ast::function_call& call,
        const IntrinsicResolutionRegistry& registry
    );

    /// Extract the binding name from a move() call's argument.
    /// Returns empty string if the argument is not a simple identifier.
    static std::string extract_move_target(
        const parser::ast::function_call& call
    );

private:
    /// Analyze a single function definition.
    void analyze_function(
        const parser::ast::function_definition& func,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        MoveTrackingResult& result
    );

    /// Analyze a block of statements within a scope.
    void analyze_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        BindingScope& scope,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        MoveTrackingResult& result
    );

    /// Analyze a single expression for move calls and use-after-move.
    void analyze_expression(
        const parser::ast::expression& expr,
        BindingScope& scope,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        MoveTrackingResult& result
    );

    /// Check if an identifier references a moved binding and emit E4002.
    void check_identifier_access(
        const parser::ast::identifier& id,
        const BindingScope& scope,
        const std::string& source_file,
        MoveTrackingResult& result
    );

    /// Collect all identifier references in an expression (for use-after-move checking).
    void collect_identifier_reads(
        const parser::ast::expression& expr,
        std::vector<parser::ast::identifier>& out_ids
    );

    /// Merge branch scopes at a join point for conditional move tracking.
    /// If a binding is MOVED in some branches but not all, it becomes
    /// CONDITIONALLY_MOVED in the parent scope.
    void merge_branch_scopes(
        const std::vector<BindingScope*>& branches,
        BindingScope& target
    );

    /// Emit an E4002 diagnostic.
    void emit_use_after_move(
        const std::string& binding_name,
        bool is_conditional,
        const std::string& source_file,
        size_t line,
        size_t column,
        MoveTrackingResult& result
    );
};

} // namespace meld::compiler
