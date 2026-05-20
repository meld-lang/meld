/// @file advisory_diagnostics_pass.cpp
/// @brief Semantic Analyzer — Advisory Diagnostics Pass implementation
///
/// Emits non-fatal advisory diagnostics:
///   W4002: "unnecessary retain/release — consider std.mem.move()"
///   I4001: "consider View[T] instead of Hold[T]"
///
/// Requirements: 7.5, 7.6

#include "meld/compiler/advisory_diagnostics_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

// ===========================================================================
// AdvisoryDiagnosticsPass
// ===========================================================================

AdvisoryDiagnosticsPass::AdvisoryDiagnosticsPass() = default;

const std::unordered_set<std::string>& AdvisoryDiagnosticsPass::own_type_names() {
    static const std::unordered_set<std::string> names = {
        "Hold",
        "std.mem.Hold",
        "mem.Hold"
    };
    return names;
}

const std::unordered_set<std::string>& AdvisoryDiagnosticsPass::back_reference_field_names() {
    static const std::unordered_set<std::string> names = {
        "parent",
        "owner",
        "container",
        "parent_ref",
        "owner_ref",
        "back_ref"
    };
    return names;
}

std::string AdvisoryDiagnosticsPass::extract_own_inner_type(
    const parser::ast::type_annotation& type
) {
    const std::string& outer_name = type.type_name.name;
    if (own_type_names().count(outer_name) > 0 &&
        type.has_type_arguments && type.type_arguments.size() == 1) {
        return type.type_arguments[0].get().type_name.name;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Class definition collection
// ---------------------------------------------------------------------------

void AdvisoryDiagnosticsPass::collect_class_definitions(
    const std::vector<parser::ast::expression>& expressions,
    std::unordered_map<std::string, const parser::ast::class_definition*>& classes
) {
    for (const auto& expr : expressions) {
        meld::compat::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T,
                    boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
                const auto& cls = node.get();
                classes[cls.name.name] = &cls;
            }
        }, expr);
    }
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

AdvisoryDiagnosticsResult AdvisoryDiagnosticsPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    AdvisoryDiagnosticsResult result;

    // Phase 1: Scan function bodies for W4002 (move suggestions)
    for (const auto& expr : expressions) {
        if (auto* func_fwd = boost::get<
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            scan_function_for_move_suggestions(func_fwd->get(), registry, source_file, result);
        }
    }

    // Phase 2: Scan class definitions for I4001 (back-reference suggestions)
    std::unordered_map<std::string, const parser::ast::class_definition*> classes;
    collect_class_definitions(expressions, classes);

    for (const auto& [name, cls_ptr] : classes) {
        scan_class_for_back_references(*cls_ptr, classes, source_file, result);
    }

    return result;
}

// ---------------------------------------------------------------------------
// W4002: Move suggestion scanning
// ---------------------------------------------------------------------------

void AdvisoryDiagnosticsPass::scan_function_for_move_suggestions(
    const parser::ast::function_definition& func,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    AdvisoryDiagnosticsResult& result
) {
    const auto& body = func.body.get();
    scan_block_for_move_suggestions(body.statements, registry, source_file, result);
}

void AdvisoryDiagnosticsPass::scan_block_for_move_suggestions(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    AdvisoryDiagnosticsResult& result
) {
    for (const auto& stmt : statements) {
        scan_expression_for_move_suggestions(stmt.get(), registry, source_file, result);
    }
}

void AdvisoryDiagnosticsPass::scan_expression_for_move_suggestions(
    const parser::ast::expression& expr,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    AdvisoryDiagnosticsResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // val x = some_hold_ref  →  suggest std.mem.move(some_hold_ref)
        // Detect: val declaration where the initializer is a plain identifier
        // (i.e., copying an existing Hold[T] binding rather than moving it)
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            // Check if the initializer is a simple identifier (not a function call,
            // not a constructor, not a literal). A plain identifier copy of a
            // Hold[T] binding triggers a retain/release that move() would elide.
            if (auto* id = boost::get<parser::ast::identifier>(&decl.value.get())) {
                // The identifier is being copied into a new val binding.
                // This is the pattern where move() would help.
                emit_move_suggestion(id->name, source_file, 0, 0, result);
            }
        }

        // var x = some_hold_ref  →  same suggestion
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            if (auto* id = boost::get<parser::ast::identifier>(&decl.value.get())) {
                emit_move_suggestion(id->name, source_file, 0, 0, result);
            }
        }

        // Assignment: x = some_hold_ref  →  suggest move
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            if (binop.op == "=") {
                if (auto* id = boost::get<parser::ast::identifier>(&binop.right.get())) {
                    emit_move_suggestion(id->name, source_file, 0, 0, result);
                }
            } else {
                // Recurse into non-assignment binary ops
                scan_expression_for_move_suggestions(binop.left.get(), registry, source_file, result);
                scan_expression_for_move_suggestions(binop.right.get(), registry, source_file, result);
            }
        }

        // Block expression: recurse
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            scan_block_for_move_suggestions(block.statements, registry, source_file, result);
        }

        // Nested function: recurse
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function_for_move_suggestions(node.get(), registry, source_file, result);
        }

        // Other nodes: no-op
        else {
            // Literals, function calls, etc. — not a plain copy pattern
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// I4001: Back-reference pattern detection
// ---------------------------------------------------------------------------

void AdvisoryDiagnosticsPass::scan_class_for_back_references(
    const parser::ast::class_definition& cls,
    const std::unordered_map<std::string, const parser::ast::class_definition*>& all_classes,
    const std::string& source_file,
    AdvisoryDiagnosticsResult& result
) {
    const std::string& class_name = cls.name.name;

    for (const auto& field : cls.fields) {
        // Check if this field is Hold[T] where T is another known class
        std::string inner_type = extract_own_inner_type(field.type);
        if (inner_type.empty()) continue;

        // Check if the target class exists in the module
        if (all_classes.count(inner_type) == 0) continue;

        // Heuristic: if the field name matches a back-reference pattern
        // (e.g., "parent", "owner"), suggest View[T] instead of Hold[T]
        const std::string& field_name = field.name.name;
        if (back_reference_field_names().count(field_name) > 0) {
            emit_back_reference_suggestion(
                field_name, class_name, inner_type,
                source_file, 0, 0, result);
            continue;
        }

        // Additional heuristic: if the target class has a Hold[this_class]
        // field, this field looks like a back-reference. The cycle detection
        // pass already warns about mutual Hold[T] (W4001), but I4001 is a
        // softer suggestion for the specific field that looks like the
        // back-reference direction.
        const auto* target_cls = all_classes.at(inner_type);
        for (const auto& target_field : target_cls->fields) {
            std::string target_inner = extract_own_inner_type(target_field.type);
            if (target_inner == class_name) {
                // Target class holds this class → this field is likely
                // the back-reference direction
                emit_back_reference_suggestion(
                    field_name, class_name, inner_type,
                    source_file, 0, 0, result);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Diagnostic emission
// ---------------------------------------------------------------------------

void AdvisoryDiagnosticsPass::emit_move_suggestion(
    const std::string& binding_name,
    const std::string& source_file,
    size_t line,
    size_t column,
    AdvisoryDiagnosticsResult& result
) {
    std::string message =
        "unnecessary retain/release — consider std.mem.move(" +
        binding_name + ")";

    result.diagnostics.push_back(AdvisoryDiagnostic{
        .level = AdvisoryDiagnostic::Level::Warning,
        .code = "W4002",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    result.move_suggestions++;
}

void AdvisoryDiagnosticsPass::emit_back_reference_suggestion(
    const std::string& field_name,
    const std::string& class_name,
    const std::string& target_class,
    const std::string& source_file,
    size_t line,
    size_t column,
    AdvisoryDiagnosticsResult& result
) {
    std::string message =
        "back-reference pattern detected — consider View[" + target_class +
        "] instead of Hold[" + target_class + "] for field '" +
        field_name + "' in class " + class_name;

    result.diagnostics.push_back(AdvisoryDiagnostic{
        .level = AdvisoryDiagnostic::Level::Info,
        .code = "I4001",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    result.back_reference_suggestions++;
}

} // namespace meld::compiler
