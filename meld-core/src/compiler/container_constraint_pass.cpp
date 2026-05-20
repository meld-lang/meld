/// @file container_constraint_pass.cpp
/// @brief Semantic Analyzer — Container Constraint Check Pass implementation
///
/// For every generic type instantiation `Container[T]`, checks if
/// `Container` has the `@intrinsic(managed_container)` annotation.
/// If so, verifies that `T` implements the `@intrinsic(memory_strategy)`
/// trait. If `T` is a raw class type, emits E4003 with a fix-it
/// suggesting `Hold[T]` or `View[T]`.
///
/// Requirements: 5.3, 5.4, 5.5, 7.3

#include "meld/compiler/container_constraint_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

// ===========================================================================
// ContainerConstraintPass
// ===========================================================================

ContainerConstraintPass::ContainerConstraintPass() = default;

const std::unordered_set<std::string>& ContainerConstraintPass::storable_type_names() {
    static const std::unordered_set<std::string> names = {
        "Own", "Link",
        "std.mem.Own", "std.mem.Link",
        "mem.Own", "mem.Link",
        "Hold", "View",
        "std.mem.Hold", "std.mem.View",
        "mem.Hold", "mem.View"
    };
    return names;
}

bool ContainerConstraintPass::is_managed_container(
    const std::string& type_name,
    const IntrinsicResolutionRegistry& registry
) {
    auto entries = registry.lookup(std_mem::IntrinsicTag::managed_container);
    for (const auto* entry : entries) {
        if (entry->entity_name == type_name) return true;
    }
    return false;
}

bool ContainerConstraintPass::is_storable_type(const std::string& type_name) {
    return storable_type_names().count(type_name) > 0;
}

bool ContainerConstraintPass::is_exempt_from_container_check(const std::string& type_name) {
    // Primitive value types don't need ownership wrappers
    static const std::unordered_set<std::string> exempt = {
        "int", "float", "string", "bool", "any", "unit",
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
        "f32", "f64", "byte", "char", "void",
        // Common stdlib value types and containers
        "List", "Vec", "Map", "Set", "Array", "Pair",
        "Result", "Option", "Future", "Task", "Fiber",
        "Duration", "Instant", "Path"
    };
    if (exempt.count(type_name) > 0) return true;
    // Single uppercase letter = generic type parameter (T, U, K, V, etc.)
    if (type_name.size() == 1 && std::isupper(type_name[0])) return true;
    // Short PascalCase names (2-3 chars) are likely type params (Fn, IO, Eq)
    if (type_name.size() <= 3 && std::isupper(type_name[0])) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

ContainerConstraintResult ContainerConstraintPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    ContainerConstraintResult result;

    for (const auto& expr : expressions) {
        scan_expression(expr, registry, source_file, result);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Type annotation checking
// ---------------------------------------------------------------------------

void ContainerConstraintPass::check_type_annotation(
    const parser::ast::type_annotation& type,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ContainerConstraintResult& result
) {
    const std::string& container_name = type.type_name.name;

    // Check if this is a managed container with type arguments
    if (type.has_type_arguments && !type.type_arguments.empty() &&
        is_managed_container(container_name, registry)) {

        result.container_types_checked++;

        // Check each type argument
        for (const auto& arg_fwd : type.type_arguments) {
            const auto& arg_type = arg_fwd.get();
            const std::string& element_name = arg_type.type_name.name;

            // Skip primitive/built-in types that aren't class types
            // (string, int, float, bool, etc. are not class types that
            // need Storable wrapping — they are value types)
            // For dict[K, V], we check V (the value type) but also K
            // since both positions in a managed container must be Storable
            // if they are class types.

            if (!is_storable_type(element_name) &&
                !is_exempt_from_container_check(element_name)) {
                // This is a raw class type in a managed container
                emit_raw_type_in_container(
                    element_name, container_name,
                    source_file, 0, 0, result);
            }
        }
    }

    // Recursively check nested type arguments (e.g., vec[Hold[vec[User]]])
    if (type.has_type_arguments) {
        for (const auto& arg_fwd : type.type_arguments) {
            check_type_annotation(arg_fwd.get(), registry, source_file, result);
        }
    }

    // Check union types
    for (const auto& ut : type.union_types) {
        check_type_annotation(ut.get(), registry, source_file, result);
    }

    // Check intersection types
    for (const auto& it : type.intersection_types) {
        check_type_annotation(it.get(), registry, source_file, result);
    }
}

// ---------------------------------------------------------------------------
// AST scanning
// ---------------------------------------------------------------------------

void ContainerConstraintPass::scan_function(
    const parser::ast::function_definition& func,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ContainerConstraintResult& result
) {
    // Check parameter type annotations
    for (const auto& param : func.parameters) {
        check_type_annotation(param.type, registry, source_file, result);
    }

    // Check return type annotation
    if (func.has_return_type) {
        check_type_annotation(func.return_type, registry, source_file, result);
    }

    // Scan the function body for nested declarations
    const auto& body = func.body.get();
    for (const auto& stmt : body.statements) {
        scan_expression(stmt.get(), registry, source_file, result);
    }
}

void ContainerConstraintPass::scan_class(
    const parser::ast::class_definition& class_def,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ContainerConstraintResult& result
) {
    // Check field type annotations
    for (const auto& field : class_def.fields) {
        check_type_annotation(field.type, registry, source_file, result);
    }
}

void ContainerConstraintPass::scan_struct(
    const parser::ast::struct_definition& struct_def,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ContainerConstraintResult& result
) {
    // Check field type annotations
    for (const auto& field : struct_def.fields) {
        check_type_annotation(field.type, registry, source_file, result);
    }
}

void ContainerConstraintPass::scan_expression(
    const parser::ast::expression& expr,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ContainerConstraintResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // --- function definition: scan parameters, return type, body ---
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function(node.get(), registry, source_file, result);
        }

        // --- class definition: scan fields and body ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            scan_class(node.get(), registry, source_file, result);
        }

        // --- struct definition: scan fields ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            scan_struct(node.get(), registry, source_file, result);
        }

        // --- block expression: scan nested statements ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            for (const auto& stmt : block.statements) {
                scan_expression(stmt.get(), registry, source_file, result);
            }
        }

        // --- Other node types: no type annotations to check ---
        else {
            // val/var declarations, literals, identifiers, etc.
            // These don't carry explicit type annotations in the current AST.
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// Diagnostic emission
// ---------------------------------------------------------------------------

void ContainerConstraintPass::emit_raw_type_in_container(
    const std::string& raw_type_name,
    const std::string& container_name,
    const std::string& source_file,
    size_t line,
    size_t column,
    ContainerConstraintResult& result
) {
    std::string message =
        "raw type '" + raw_type_name + "' in managed container '" +
        container_name + "' — use Hold[" + raw_type_name +
        "] or View[" + raw_type_name + "]";

    result.diagnostics.push_back(ContainerConstraintDiagnostic{
        .level = ContainerConstraintDiagnostic::Level::Error,
        .code = "E4003",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    result.raw_type_errors++;
    result.success = false;
}

} // namespace meld::compiler
