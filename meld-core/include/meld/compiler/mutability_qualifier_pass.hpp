#pragma once

/// @file mutability_qualifier_pass.hpp
/// @brief Semantic Analyzer — Default Mutability Qualifier Resolution Pass
///
/// When a generic type parameter has no explicit `val`/`var` qualifier
/// (MutabilityQualifier::NONE), this pass resolves it to `VAL` (immutable
/// by default), consistent with Meld's "immutable by default" philosophy.
///
/// This is a normalization pass that MUTATES the AST in place, running
/// early in the pipeline before downstream passes that depend on resolved
/// mutability qualifiers.
///
/// Requirements: 165.2

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// MutabilityQualifierResult — output of the pass
// ---------------------------------------------------------------------------

struct MutabilityQualifierResult {
    bool success = true;
    size_t type_args_scanned = 0;
    size_t defaults_applied = 0;  // How many NONE → VAL resolutions
};

// ---------------------------------------------------------------------------
// MutabilityQualifierPass — the normalization pass
// ---------------------------------------------------------------------------

/// Scans all type annotations in the AST and resolves NONE → VAL on
/// type arguments. This ensures downstream passes can assume every
/// type argument has an explicit mutability qualifier.
class MutabilityQualifierPass {
public:
    MutabilityQualifierPass();

    /// Run the pass over a set of parsed expressions.
    /// NOTE: non-const — modifies AST in place.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    MutabilityQualifierResult run(
        std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Resolve a single type_annotation's type arguments.
    /// Mutates the type_annotation in place: NONE → VAL.
    /// Returns the number of defaults applied.
    static size_t resolve_defaults(parser::ast::type_annotation& type);

private:
    /// Scan a single expression for type annotations to resolve.
    void scan_expression(
        parser::ast::expression& expr,
        MutabilityQualifierResult& result
    );

    /// Scan a block of statements.
    void scan_block(
        std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        MutabilityQualifierResult& result
    );

    /// Scan a function definition for type annotations.
    void scan_function(
        parser::ast::function_definition& func,
        MutabilityQualifierResult& result
    );

    /// Resolve defaults on a type_annotation and accumulate results.
    void resolve_and_record(
        parser::ast::type_annotation& type,
        MutabilityQualifierResult& result
    );
};

} // namespace meld::compiler
