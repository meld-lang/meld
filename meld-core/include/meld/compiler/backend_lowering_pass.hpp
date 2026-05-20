#pragma once

/// @file backend_lowering_pass.hpp
/// @brief C++ Backend Lowering Pass — Hold[T]/View[T]/move() code generation
///
/// Translates Meld AST nodes involving Hold[T], View[T], and std.mem.move()
/// into C++ code fragments. This pass runs after all semantic analysis
/// passes and produces C++ code strings for the backend.
///
/// Lowering rules:
///   Hold[T] declarations  → meld::std_mem::Own<T>
///   View[T] declarations  → meld::std_mem::Link<T>
///   if val x = view_ref   → if (auto x = view_ref.upgrade()) { ... }
///   std.mem.move(x)       → meld::std_mem::mem_move(x)
///
/// Requirements: 1.2, 1.3, 2.2, 2.3, 2.4, 4.4

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Lowered code fragment — a single C++ code emission
// ---------------------------------------------------------------------------

/// Represents a single lowered C++ code fragment produced by the pass.
struct LoweredFragment {
    /// The kind of Meld construct that was lowered.
    enum class Kind {
        OwnDeclaration,      ///< Hold[T] variable declaration
        LinkDeclaration,     ///< View[T] variable declaration
        LinkUpgrade,         ///< if val x = view_ref → if (auto x = ...)
        MoveCall,            ///< std.mem.move(x) → mem_move(x)
        OwnAssignment,       ///< Hold[T] copy assignment (retain/release)
        LinkAssignment,      ///< View[T] copy assignment (weak_count)
        OwnScopeExit,        ///< Hold[T] scope exit (release)
        LinkScopeExit        ///< View[T] scope exit (weak_count--)
    };

    Kind kind;

    /// The generated C++ code string.
    std::string cpp_code;

    /// The original Meld source construct (for diagnostics).
    std::string meld_source;

    /// The inner type name T (extracted from Hold[T] or View[T]).
    std::string inner_type;

    /// The variable name in the generated code.
    std::string variable_name;

    /// Source location of the original Meld construct.
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// BackendLoweringResult — output of the pass
// ---------------------------------------------------------------------------

/// Diagnostic produced by the backend lowering pass.
struct BackendLoweringDiagnostic {
    enum class Level { Info, Warning, Error };
    Level level;
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

/// Result of running the backend lowering pass.
struct BackendLoweringResult {
    bool success = true;

    /// All lowered C++ code fragments, in source order.
    std::vector<LoweredFragment> fragments;

    /// Diagnostics produced during lowering.
    std::vector<BackendLoweringDiagnostic> diagnostics;

    /// Counts for verification.
    size_t own_declarations_lowered = 0;
    size_t link_declarations_lowered = 0;
    size_t link_upgrades_lowered = 0;
    size_t move_calls_lowered = 0;
};

// ---------------------------------------------------------------------------
// BackendLoweringPass — the C++ backend lowering pass
// ---------------------------------------------------------------------------

/// The Backend Lowering Pass translates Meld AST nodes involving Hold[T],
/// View[T], and std.mem.move() into C++ code fragments.
///
/// This pass consults the IntrinsicResolutionRegistry to identify:
///   - Hold[T] types (memory_strategy with is_owning == true)
///   - View[T] types (memory_strategy with is_owning == false)
///   - std.mem.move() calls (memory_move intrinsic)
///
/// Lowering rules:
///   Hold[T]  → meld::std_mem::Own<T>  (non-atomic ++/-- on ref_count_)
///   View[T] → meld::std_mem::Link<T> (non-atomic ++/-- on weak_count_)
///   if val x = view_ref → if (auto x = view_ref.upgrade()) { ... }
///   std.mem.move(x) → meld::std_mem::mem_move(x)
class BackendLoweringPass {
public:
    BackendLoweringPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param registry     The intrinsic registry (from IntrinsicResolutionPass).
    /// @param source_file  Source file path for diagnostics.
    BackendLoweringResult run(
        const std::vector<parser::ast::expression>& expressions,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file = ""
    );

    // ----- Individual lowering methods (public for testing) -----

    /// Lower a Hold[T] variable declaration to C++.
    /// Input:  val x = SomeClass(...)   [inferred as Hold[SomeClass]]
    /// Output: meld::std_mem::Own<SomeClass> x = ...;
    LoweredFragment lower_own_declaration(
        const std::string& var_name,
        const std::string& inner_type,
        const std::string& initializer,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    /// Lower a View[T] variable declaration to C++.
    /// Input:  val observer: View[Node] = std.mem.view(owner)
    /// Output: meld::std_mem::Link<Node> observer = meld::std_mem::link(owner);
    LoweredFragment lower_link_declaration(
        const std::string& var_name,
        const std::string& inner_type,
        const std::string& initializer,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    /// Lower a View[T] upgrade via `if val` to C++.
    /// Input:  if val x = view_ref { body } else { else_body }
    /// Output: if (auto x = view_ref.upgrade()) { body } else { else_body }
    LoweredFragment lower_link_upgrade(
        const std::string& bound_name,
        const std::string& link_expr,
        const std::string& inner_type,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    /// Lower a std.mem.move() call to C++.
    /// Input:  std.mem.move(x)
    /// Output: meld::std_mem::mem_move(x)
    LoweredFragment lower_move_call(
        const std::string& source_var,
        const std::string& inner_type,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    /// Lower a Hold[T] scope exit (destructor / release).
    /// Output: // ~Own<T> → ref_count_-- (non-atomic)
    LoweredFragment lower_own_scope_exit(
        const std::string& var_name,
        const std::string& inner_type,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    /// Lower a View[T] scope exit (destructor / weak_count--).
    /// Output: // ~Link<T> → weak_count_-- (non-atomic)
    LoweredFragment lower_link_scope_exit(
        const std::string& var_name,
        const std::string& inner_type,
        const std::string& source_file = "",
        size_t line = 0, size_t column = 0
    ) const;

    // ----- Type detection helpers -----

    /// Check if a type annotation represents Hold[T].
    static bool is_own_type(const std::string& type_name);

    /// Check if a type annotation represents View[T].
    static bool is_link_type(const std::string& type_name);

    /// Extract the inner type T from "Hold[T]" or "View[T]".
    /// Returns empty string if not a recognized wrapper type.
    static std::string extract_inner_type(const std::string& type_name);

    /// Convert a Meld type name to its C++ lowered equivalent.
    /// Hold[T] → meld::std_mem::Own<T>
    /// View[T] → meld::std_mem::Link<T>
    static std::string to_cpp_type(const std::string& meld_type);

private:
    /// Scan a function definition for Hold/View/move constructs.
    void scan_function(
        const parser::ast::function_definition& func,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        BackendLoweringResult& result
    );

    /// Scan a block of statements.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& stmts,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        BackendLoweringResult& result
    );

    /// Scan a single expression for lowering opportunities.
    void scan_expression(
        const parser::ast::expression& expr,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        BackendLoweringResult& result
    );

    /// Check if a function call is std.mem.move().
    bool is_move_call(
        const parser::ast::function_call& call,
        const IntrinsicResolutionRegistry& registry
    ) const;

    /// Check if an if-val expression is a View[T] upgrade.
    /// NOTE: if_expression AST node not yet implemented — stubbed.
    // bool is_link_upgrade(...) const;
};

} // namespace meld::compiler
