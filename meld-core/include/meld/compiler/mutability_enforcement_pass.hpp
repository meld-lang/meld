#pragma once

/// @file mutability_enforcement_pass.hpp
/// @brief Semantic Analyzer — Mutability Enforcement Pass
///
/// Enforces mutability contracts on generic type parameters:
/// - When a type parameter is `val`, rejects calls to mutating methods (E4010)
/// - When a type parameter is `var`, permits mutating calls only if the
///   enclosing function has `@effect(state)` (E4011)
/// - Rejects val→var qualifier upgrades without explicit checked cast (E4012)
///
/// This pass runs AFTER the MutabilityQualifierPass (which normalizes
/// NONE → VAL), so every type argument has an explicit qualifier.
///
/// Requirements: 165.3, 165.4, 165.5, 165.6

#include "meld/parser/ast.hpp"
#include <string>
#include <unordered_set>
#include <vector>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Diagnostic output
// ---------------------------------------------------------------------------

struct MutabilityEnforcementDiagnostic {
    enum class Level { Info, Warning, Error };
    Level level;
    std::string code;    // E4010 or E4011
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

struct MutabilityEnforcementResult {
    bool success = true;
    std::vector<MutabilityEnforcementDiagnostic> diagnostics;
    size_t val_violations = 0;
    size_t effect_violations = 0;
    size_t qualifier_upgrade_violations = 0;
    size_t effect_state_inferred = 0;
    std::vector<std::string> functions_with_inferred_state;
};

// ---------------------------------------------------------------------------
// MutabilityEnforcementPass
// ---------------------------------------------------------------------------

class MutabilityEnforcementPass {
public:
    MutabilityEnforcementPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions (const — read-only).
    /// @param source_file  Source file path for diagnostics.
    MutabilityEnforcementResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a method name is a known mutating method.
    static bool is_mutating_method(const std::string& method_name);

    /// Check if a type argument qualifier assignment is compatible.
    /// var → val is allowed (downgrade — restricting permissions).
    /// val → var is NOT compatible (upgrade — would need checked cast).
    /// Returns true if compatible, false if not.
    static bool is_qualifier_compatible(
        parser::ast::type_annotation::MutabilityQualifier source,
        parser::ast::type_annotation::MutabilityQualifier target
    );

    /// Check a qualifier assignment and emit E4012 if incompatible.
    /// Returns true if compatible, false if E4012 was emitted.
    bool check_qualifier_assignment(
        parser::ast::type_annotation::MutabilityQualifier source,
        parser::ast::type_annotation::MutabilityQualifier target,
        const std::string& source_file,
        size_t line, size_t column,
        MutabilityEnforcementResult& result
    );

private:
    /// Binding info tracked during scanning.
    struct BindingInfo {
        std::string name;
        parser::ast::type_annotation::MutabilityQualifier first_arg_qualifier =
            parser::ast::type_annotation::MutabilityQualifier::NONE;
        bool has_generic_type = false;
    };

    /// Check if a function has @effect(state) or @uses(state).
    static bool has_effect_state(const parser::ast::function_definition& func);

    /// Scan a function body for mutability violations.
    void scan_function(
        const parser::ast::function_definition& func,
        const std::string& source_file,
        MutabilityEnforcementResult& result
    );

    /// Scan a block of statements.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::vector<BindingInfo>& bindings,
        bool enclosing_has_effect_state,
        const std::string& source_file,
        MutabilityEnforcementResult& result
    );

    /// Scan a single expression.
    void scan_expression(
        const parser::ast::expression& expr,
        std::vector<BindingInfo>& bindings,
        bool enclosing_has_effect_state,
        const std::string& source_file,
        MutabilityEnforcementResult& result
    );

    /// Extract binding info from a type annotation.
    static BindingInfo make_binding(
        const std::string& name,
        const parser::ast::type_annotation& type
    );

    /// Emit E4010 diagnostic.
    void emit_val_violation(
        const std::string& method_name,
        const std::string& source_file,
        size_t line, size_t column,
        MutabilityEnforcementResult& result
    );

    /// Emit E4011 diagnostic.
    void emit_effect_violation(
        const std::string& method_name,
        const std::string& source_file,
        size_t line, size_t column,
        MutabilityEnforcementResult& result
    );

    /// Emit E4012 diagnostic (val→var upgrade attempt).
    void emit_qualifier_upgrade_violation(
        const std::string& source_file,
        size_t line, size_t column,
        MutabilityEnforcementResult& result
    );

    /// The set of known mutating method names.
    static const std::unordered_set<std::string>& mutating_methods();
};

} // namespace meld::compiler
