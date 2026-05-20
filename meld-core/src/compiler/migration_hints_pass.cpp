/// @file migration_hints_pass.cpp
/// @brief Semantic Analyzer — Migration Hints Pass implementation
///
/// Emits info-level diagnostics for old type/function names:
///   I4010: Own[T]  → Hold[T]
///   I4011: Link[T] → View[T]
///   I4012: link()  → view()
///   I4013: Dict    → Map
///   I4014: Deque   → Queue
///   I4015: vec     → List
///
/// Requirements: 119.7, 120.8, 166.3, 166.4

#include "meld/compiler/migration_hints_pass.hpp"
#include "meld/compat/visit.hpp"

namespace meld::compiler {

// ===========================================================================
// MigrationHintsPass
// ===========================================================================

MigrationHintsPass::MigrationHintsPass() = default;

const std::unordered_set<std::string>& MigrationHintsPass::old_own_type_names() {
    static const std::unordered_set<std::string> names = {
        "Own",
        "std.mem.Own",
        "mem.Own"
    };
    return names;
}

const std::unordered_set<std::string>& MigrationHintsPass::old_link_type_names() {
    static const std::unordered_set<std::string> names = {
        "Link",
        "std.mem.Link",
        "mem.Link"
    };
    return names;
}

const std::unordered_set<std::string>& MigrationHintsPass::old_link_call_names() {
    static const std::unordered_set<std::string> names = {
        "link",
        "std.mem.link",
        "mem.link"
    };
    return names;
}

const std::unordered_set<std::string>& MigrationHintsPass::old_dict_type_names() {
    static const std::unordered_set<std::string> names = {
        "Dict",
        "dict"
    };
    return names;
}

const std::unordered_set<std::string>& MigrationHintsPass::old_deque_type_names() {
    static const std::unordered_set<std::string> names = {
        "Deque",
        "deque"
    };
    return names;
}

const std::unordered_set<std::string>& MigrationHintsPass::old_vec_type_names() {
    static const std::unordered_set<std::string> names = {
        "vec"
    };
    return names;
}

// ---------------------------------------------------------------------------
// Type annotation checking
// ---------------------------------------------------------------------------

void MigrationHintsPass::check_type_annotation(
    const parser::ast::type_annotation& type,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    const std::string& outer_name = type.type_name.name;

    if (old_own_type_names().count(outer_name) > 0) {
        result.diagnostics.push_back(MigrationHintDiagnostic{
            .level = MigrationHintDiagnostic::Level::Info,
            .code = "I4010",
            .message = "Did you mean Hold[T]? Own[T] has been renamed to Hold[T].",
            .source_file = source_file,
            .line = 0,
            .column = 0
        });
        result.own_hints++;
    }

    if (old_link_type_names().count(outer_name) > 0) {
        result.diagnostics.push_back(MigrationHintDiagnostic{
            .level = MigrationHintDiagnostic::Level::Info,
            .code = "I4011",
            .message = "Did you mean View[T]? Link[T] has been renamed to View[T].",
            .source_file = source_file,
            .line = 0,
            .column = 0
        });
        result.link_hints++;
    }

    if (old_dict_type_names().count(outer_name) > 0) {
        result.diagnostics.push_back(MigrationHintDiagnostic{
            .level = MigrationHintDiagnostic::Level::Info,
            .code = "I4013",
            .message = "Did you mean Map? Dict has been renamed to Map.",
            .source_file = source_file,
            .line = 0,
            .column = 0
        });
        result.dict_hints++;
    }

    if (old_deque_type_names().count(outer_name) > 0) {
        result.diagnostics.push_back(MigrationHintDiagnostic{
            .level = MigrationHintDiagnostic::Level::Info,
            .code = "I4014",
            .message = "Did you mean Queue? Deque has been renamed to Queue.",
            .source_file = source_file,
            .line = 0,
            .column = 0
        });
        result.deque_hints++;
    }

    if (old_vec_type_names().count(outer_name) > 0) {
        result.diagnostics.push_back(MigrationHintDiagnostic{
            .level = MigrationHintDiagnostic::Level::Info,
            .code = "I4015",
            .message = "Did you mean List? vec has been renamed to List.",
            .source_file = source_file,
            .line = 0,
            .column = 0
        });
        result.vec_hints++;
    }

    // Recurse into type arguments (e.g., List[Own[T]])
    if (type.has_type_arguments) {
        for (const auto& arg : type.type_arguments) {
            check_type_annotation(arg.get(), source_file, result);
        }
    }
}

// ---------------------------------------------------------------------------
// AST scanning
// ---------------------------------------------------------------------------

void MigrationHintsPass::scan_function(
    const parser::ast::function_definition& func,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    // Check parameter types
    for (const auto& param : func.parameters) {
        check_type_annotation(param.type, source_file, result);
    }

    // Check return type
    if (func.has_return_type) {
        check_type_annotation(func.return_type, source_file, result);
    }

    // Scan function body
    const auto& body = func.body.get();
    scan_block(body.statements, source_file, result);
}

void MigrationHintsPass::scan_class(
    const parser::ast::class_definition& cls,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    for (const auto& field : cls.fields) {
        check_type_annotation(field.type, source_file, result);
    }
}

void MigrationHintsPass::scan_struct(
    const parser::ast::struct_definition& s,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    for (const auto& field : s.fields) {
        check_type_annotation(field.type, source_file, result);
    }
}

void MigrationHintsPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), source_file, result);
    }
}

void MigrationHintsPass::scan_expression(
    const parser::ast::expression& expr,
    const std::string& source_file,
    MigrationHintsResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // val declaration — check type annotation and initializer
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            if (decl.has_type_annotation) {
                check_type_annotation(decl.type_ann.get(), source_file, result);
            }
            scan_expression(decl.value.get(), source_file, result);
        }

        // var declaration — check type annotation and initializer
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            if (decl.has_type_annotation) {
                check_type_annotation(decl.type_ann.get(), source_file, result);
            }
            scan_expression(decl.value.get(), source_file, result);
        }

        // Function call — check for old link() name
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            const auto& call = node.get();
            const std::string& call_name = call.function_name.name;
            if (old_link_call_names().count(call_name) > 0) {
                result.diagnostics.push_back(MigrationHintDiagnostic{
                    .level = MigrationHintDiagnostic::Level::Info,
                    .code = "I4012",
                    .message = "Did you mean std.mem.view()? link() has been renamed to view().",
                    .source_file = source_file,
                    .line = 0,
                    .column = 0
                });
                result.link_call_hints++;
            }
            // Recurse into arguments
            for (const auto& arg : call.arguments) {
                scan_expression(arg.get(), source_file, result);
            }
        }

        // Binary operation — recurse into both sides
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            scan_expression(binop.left.get(), source_file, result);
            scan_expression(binop.right.get(), source_file, result);
        }

        // Block expression — recurse
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            scan_block(block.statements, source_file, result);
        }

        // Nested function — recurse
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function(node.get(), source_file, result);
        }

        // Class definition — check field types
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            scan_class(node.get(), source_file, result);
        }

        // Struct definition — check field types
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            scan_struct(node.get(), source_file, result);
        }

        // Other nodes: no-op
        else {
            // Literals, identifiers, etc. — nothing to check
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

MigrationHintsResult MigrationHintsPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    MigrationHintsResult result;

    for (const auto& expr : expressions) {
        scan_expression(expr, source_file, result);
    }

    return result;
}

} // namespace meld::compiler
