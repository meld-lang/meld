/// @file mutability_qualifier_pass.cpp
/// @brief Semantic Analyzer — Default Mutability Qualifier Resolution Pass
///
/// Resolves NONE → VAL on all type arguments in the AST.
/// This is a normalization pass that runs early in the pipeline.
///
/// Requirements: 165.2

#include "meld/compiler/mutability_qualifier_pass.hpp"
#include "meld/compat/visit.hpp"

namespace meld::compiler {

using MQ = parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// MutabilityQualifierPass
// ===========================================================================

MutabilityQualifierPass::MutabilityQualifierPass() = default;

// ---------------------------------------------------------------------------
// resolve_defaults — static, works on a single type_annotation
// ---------------------------------------------------------------------------

size_t MutabilityQualifierPass::resolve_defaults(
    parser::ast::type_annotation& type
) {
    size_t applied = 0;

    if (!type.has_type_arguments) {
        return applied;
    }

    for (auto& arg_fwd : type.type_arguments) {
        auto& arg = arg_fwd.get();

        // Resolve NONE → VAL on this type argument
        if (arg.mutability_qualifier == MQ::NONE) {
            arg.mutability_qualifier = MQ::VAL;
            ++applied;
        }

        // Recurse into nested type arguments (e.g., Map[K, V] inside View[Map[K, V]])
        applied += resolve_defaults(arg);
    }

    return applied;
}

// ---------------------------------------------------------------------------
// resolve_and_record — instance helper
// ---------------------------------------------------------------------------

void MutabilityQualifierPass::resolve_and_record(
    parser::ast::type_annotation& type,
    MutabilityQualifierResult& result
) {
    if (type.has_type_arguments) {
        result.type_args_scanned += type.type_arguments.size();
    }
    result.defaults_applied += resolve_defaults(type);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

MutabilityQualifierResult MutabilityQualifierPass::run(
    std::vector<parser::ast::expression>& expressions,
    const std::string& /*source_file*/
) {
    MutabilityQualifierResult result;

    for (auto& expr : expressions) {
        scan_expression(expr, result);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Function scanning
// ---------------------------------------------------------------------------

void MutabilityQualifierPass::scan_function(
    parser::ast::function_definition& func,
    MutabilityQualifierResult& result
) {
    // Resolve parameter types
    for (auto& param : func.parameters) {
        resolve_and_record(param.type, result);
    }

    // Resolve return type
    if (func.has_return_type) {
        resolve_and_record(func.return_type, result);
    }

    // Resolve named return types
    for (auto& nr : func.named_returns) {
        resolve_and_record(nr.type, result);
    }

    // Recurse into body
    auto& body = func.body.get();
    scan_block(body.statements, result);
}

// ---------------------------------------------------------------------------
// Block scanning
// ---------------------------------------------------------------------------

void MutabilityQualifierPass::scan_block(
    std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    MutabilityQualifierResult& result
) {
    for (auto& stmt : statements) {
        scan_expression(stmt.get(), result);
    }
}

// ---------------------------------------------------------------------------
// Expression scanning
// ---------------------------------------------------------------------------

void MutabilityQualifierPass::scan_expression(
    parser::ast::expression& expr,
    MutabilityQualifierResult& result
) {
    meld::compat::visit([&](auto& node) {
        using T = std::decay_t<decltype(node)>;

        // val declarations with type annotations
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            auto& decl = node.get();
            if (decl.has_type_annotation) {
                resolve_and_record(decl.type_ann.get(), result);
            }
        }

        // var declarations with type annotations
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            auto& decl = node.get();
            if (decl.has_type_annotation) {
                resolve_and_record(decl.type_ann.get(), result);
            }
        }

        // Function definitions
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function(node.get(), result);
        }

        // Class definitions — scan field types
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            auto& cls = node.get();
            for (auto& field : cls.fields) {
                resolve_and_record(field.type, result);
            }
            for (auto& method : cls.methods) {
                scan_function(method, result);
            }
        }

        // Struct definitions — scan field types
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            auto& s = node.get();
            for (auto& field : s.fields) {
                resolve_and_record(field.type, result);
            }
            for (auto& method : s.methods) {
                scan_function(method, result);
            }
        }

        // Block expressions — recurse
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            // block_expression is not in the expression variant
            // (handled via function body scanning)
        }

        // Other nodes: no-op
        else {
            // Literals, identifiers, etc.
        }

    }, expr);
}

} // namespace meld::compiler
