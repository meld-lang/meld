/// @file own_type_inference_pass.cpp
/// @brief Semantic Analyzer — Hold[T] / View[T] Default Type Inference Pass
///
/// Creator Rule: When a class instance is assigned to a variable without
/// an explicit wrapper (e.g., `val x = MyClass()`), infers the variable's
/// type as `Hold[MyClass]` rather than raw `MyClass`.
///
/// Guest Rule: When a function parameter uses a bare class type without
/// an explicit Hold[T] or View[T] wrapper, infers `View[T]` for that
/// parameter.
///
/// Requirements: 119.5, 120.7

#include "meld/compiler/hold_type_inference_pass.hpp"
#include "meld/compat/visit.hpp"

#include <unordered_set>

namespace meld::compiler {

// ===========================================================================
// HoldTypeInferencePass
// ===========================================================================

HoldTypeInferencePass::HoldTypeInferencePass() = default;

bool HoldTypeInferencePass::is_constructor_call(
    const std::string& call_name,
    const std::unordered_set<std::string>& known_classes
) {
    return known_classes.count(call_name) > 0;
}

// ---------------------------------------------------------------------------
// Class name collection
// ---------------------------------------------------------------------------

void HoldTypeInferencePass::collect_class_names(
    const std::vector<parser::ast::expression>& expressions,
    std::unordered_set<std::string>& class_names
) {
    for (const auto& expr : expressions) {
        meld::compat::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T,
                    boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
                class_names.insert(node.get().name.name);
            }
        }, expr);
    }
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

HoldTypeInferenceResult HoldTypeInferencePass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    HoldTypeInferenceResult result;
    inferred_types_.clear();

    // Step 1: Collect all class names in the module
    std::unordered_set<std::string> known_classes;
    collect_class_names(expressions, known_classes);

    // Step 2: Scan all function bodies for val/var declarations
    for (const auto& expr : expressions) {
        scan_expression(expr, known_classes, source_file, result);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Function scanning
// ---------------------------------------------------------------------------

void HoldTypeInferencePass::scan_function(
    const parser::ast::function_definition& func,
    const std::unordered_set<std::string>& known_classes,
    const std::string& source_file,
    HoldTypeInferenceResult& result
) {
    // Guest Rule: infer View[T] for bare class-typed parameters
    infer_guest_rule_parameters(func, known_classes, source_file, result);

    // Creator Rule: scan body for val/var with constructor calls
    const auto& body = func.body.get();
    scan_block(body.statements, known_classes, source_file, result);
}

void HoldTypeInferencePass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::unordered_set<std::string>& known_classes,
    const std::string& source_file,
    HoldTypeInferenceResult& result
) {
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), known_classes, source_file, result);
    }
}

// ---------------------------------------------------------------------------
// Expression scanning
// ---------------------------------------------------------------------------

void HoldTypeInferencePass::scan_expression(
    const parser::ast::expression& expr,
    const std::unordered_set<std::string>& known_classes,
    const std::string& source_file,
    HoldTypeInferenceResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // val x = MyClass()  →  infer Hold[MyClass]
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            result.bindings_scanned++;

            // Check if the initializer is a constructor call
            if (auto* call_fwd = boost::get<
                    boost::spirit::x3::forward_ast<parser::ast::function_call>>(
                        &decl.value.get())) {
                const auto& call = call_fwd->get();
                if (is_constructor_call(call.function_name.name, known_classes)) {
                    record_inference(decl.name.name, call.function_name.name,
                                     source_file, 0, 0, result);
                }
            }
        }

        // var x = MyClass()  →  infer Hold[MyClass]
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            result.bindings_scanned++;

            if (auto* call_fwd = boost::get<
                    boost::spirit::x3::forward_ast<parser::ast::function_call>>(
                        &decl.value.get())) {
                const auto& call = call_fwd->get();
                if (is_constructor_call(call.function_name.name, known_classes)) {
                    record_inference(decl.name.name, call.function_name.name,
                                     source_file, 0, 0, result);
                }
            }
        }

        // Recurse into function definitions
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function(node.get(), known_classes, source_file, result);
        }

        // Recurse into block expressions
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            scan_block(block.statements, known_classes, source_file, result);
        }

        // Other nodes: no-op
        else {
            // Literals, identifiers, class defs, etc.
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// Inference recording
// ---------------------------------------------------------------------------

void HoldTypeInferencePass::record_inference(
    const std::string& binding_name,
    const std::string& class_name,
    const std::string& source_file,
    size_t line,
    size_t column,
    HoldTypeInferenceResult& result
) {
    std::string inferred = "Hold[" + class_name + "]";

    result.inferences.push_back(HoldTypeInference{
        .binding_name = binding_name,
        .class_name = class_name,
        .inferred_type = inferred,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    inferred_types_[binding_name] = inferred;
    result.own_types_inferred++;
}

// ---------------------------------------------------------------------------
// Primitive type check
// ---------------------------------------------------------------------------

bool HoldTypeInferencePass::is_primitive_type(const std::string& type_name) {
    static const std::unordered_set<std::string> primitives = {
        "int", "float", "bool", "string", "nil", "byte", "char", "unit"
    };
    return primitives.count(type_name) > 0;
}

// ---------------------------------------------------------------------------
// Guest Rule: View[T] inference for function parameters
// Requirements: 120.7
// ---------------------------------------------------------------------------

void HoldTypeInferencePass::infer_guest_rule_parameters(
    const parser::ast::function_definition& func,
    const std::unordered_set<std::string>& known_classes,
    const std::string& source_file,
    HoldTypeInferenceResult& result
) {
    for (const auto& param : func.parameters) {
        const auto& type_name = param.type.type_name.name;

        // Skip empty type annotations
        if (type_name.empty()) continue;

        // Skip primitive types
        if (is_primitive_type(type_name)) continue;

        // Skip if already wrapped with Hold or View (including qualified names)
        if (type_name == "Hold" || type_name == "View" ||
            type_name == "std.mem.Hold" || type_name == "std.mem.View") {
            continue;
        }

        // Skip if the type is not a known class in this module
        if (known_classes.count(type_name) == 0) continue;

        // Bare class type → infer View[T]
        std::string inferred = "View[" + type_name + "]";

        result.guest_rule_inferences.push_back(GuestRuleInference{
            .parameter_name = param.name.name,
            .class_name = type_name,
            .inferred_type = inferred,
            .function_name = func.name.name,
            .source_file = source_file,
            .line = 0,
            .column = 0
        });

        inferred_types_[param.name.name] = inferred;
        result.guest_types_inferred++;
    }
}

} // namespace meld::compiler
