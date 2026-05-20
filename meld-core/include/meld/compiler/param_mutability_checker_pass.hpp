#pragma once

/// @file param_mutability_checker_pass.hpp
/// @brief Semantic Analyzer — Parameter Mutability Checker Pass
///
/// Enforces `var` parameter declarations for function parameters that
/// are mutated in the function body. Scans all function definitions
/// (top-level, class methods, struct methods) to detect mutations of
/// parameters. Emits errors when:
///   - A parameter is mutated without being declared `var` (E5002)
///   - A parameter is declared `var` but never mutated (W5002)
///
/// Requirements: 57.5, 57.6, 57.7

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Parameter mutability diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the parameter mutability checker pass.
struct ParamMutabilityDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E5002" or "W5002"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// ParamMutabilityResult — output of the pass
// ---------------------------------------------------------------------------

struct ParamMutabilityResult {
    bool success = true;
    std::vector<ParamMutabilityDiagnostic> diagnostics;
    size_t functions_checked = 0;
    size_t param_mutations_detected = 0;
    size_t errors_emitted = 0;
    size_t warnings_emitted = 0;
};

// ---------------------------------------------------------------------------
// ParamMutabilityCheckerPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Parameter Mutability Checker Pass scans function bodies for
/// mutations of parameters. It enforces that:
///   1. Parameters mutated in the body must be declared with `var`
///   2. Parameters declared `var` must actually be mutated
///
/// Mutation of a parameter is detected by:
///   - Direct assignment: `param = value` (binary_operation "=" where
///     LHS is an identifier matching a parameter name)
///   - Field assignment: `param.field = value` (binary_operation "="
///     where LHS is a dot-access on an identifier matching a parameter)
///
/// This pass runs after parsing and operates on all function_definition
/// nodes found in the AST (top-level, class methods, struct methods).
class ParamMutabilityCheckerPass {
public:
    ParamMutabilityCheckerPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    ParamMutabilityResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a binary operation is a direct assignment to a named parameter.
    /// Returns the parameter name if it is, empty string otherwise.
    static std::string get_direct_assign_target(
        const parser::ast::binary_operation& binop
    );

    /// Check if a binary operation is a field assignment on a named parameter.
    /// Returns the parameter name if it is, empty string otherwise.
    static std::string get_field_assign_target(
        const parser::ast::binary_operation& binop
    );

private:
    /// Analyze a single function definition for parameter mutations.
    void analyze_function(
        const parser::ast::function_definition& func,
        const std::string& context_name,
        const std::string& source_file,
        ParamMutabilityResult& result
    );

    /// Analyze all methods in a class definition.
    void analyze_class(
        const parser::ast::class_definition& class_def,
        const std::string& source_file,
        ParamMutabilityResult& result
    );

    /// Analyze all methods in a struct definition.
    void analyze_struct(
        const parser::ast::struct_definition& struct_def,
        const std::string& source_file,
        ParamMutabilityResult& result
    );

    /// Collect the set of parameter names that are mutated in an expression.
    void collect_mutated_params(
        const parser::ast::expression& expr,
        const std::set<std::string>& param_names,
        std::set<std::string>& mutated
    );

    /// Collect mutated params across a block of statements.
    void collect_mutated_params_in_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::set<std::string>& param_names,
        std::set<std::string>& mutated
    );

    /// Emit E5002: parameter mutated without `var` declaration.
    void emit_param_mutated_without_var(
        const std::string& param_name,
        const std::string& func_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        ParamMutabilityResult& result
    );

    /// Emit W5002: parameter declared `var` but never mutated.
    void emit_unnecessary_var_param(
        const std::string& param_name,
        const std::string& func_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        ParamMutabilityResult& result
    );
};

} // namespace meld::compiler
