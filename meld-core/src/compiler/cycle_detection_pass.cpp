/// @file cycle_detection_pass.cpp
/// @brief Semantic Analyzer — Cycle Detection Pass implementation
///
/// For every pair of class declarations A and B, checks if A has a field
/// of type Hold[B] and B has a field of type Hold[A]. If so, emits warning
/// W4001: "potential reference cycle: {A} and {B} have mutual Hold[T]
/// references — consider using View[T] for one direction".
///
/// Requirements: 7.4, 10.4

#include "meld/compiler/cycle_detection_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <set>

namespace meld::compiler {

// ===========================================================================
// CycleDetectionPass
// ===========================================================================

CycleDetectionPass::CycleDetectionPass() = default;

const std::unordered_set<std::string>& CycleDetectionPass::own_type_names() {
    static const std::unordered_set<std::string> names = {
        "Own",
        "std.mem.Own",
        "mem.Own"
    };
    return names;
}

// ---------------------------------------------------------------------------
// Hold[T] extraction helpers
// ---------------------------------------------------------------------------

std::string CycleDetectionPass::extract_own_inner_type(
    const parser::ast::type_annotation& type
) {
    const std::string& outer_name = type.type_name.name;

    // Check if the outer type is an Own variant with exactly one type argument
    if (own_type_names().count(outer_name) > 0 &&
        type.has_type_arguments && type.type_arguments.size() == 1) {
        return type.type_arguments[0].get().type_name.name;
    }

    return {};
}

std::unordered_set<std::string> CycleDetectionPass::extract_own_targets(
    const std::vector<parser::ast::field_declaration>& fields
) {
    std::unordered_set<std::string> targets;
    for (const auto& field : fields) {
        std::string inner = extract_own_inner_type(field.type);
        if (!inner.empty()) {
            targets.insert(inner);
        }
    }
    return targets;
}

// ---------------------------------------------------------------------------
// AST collection
// ---------------------------------------------------------------------------

void CycleDetectionPass::collect_class_definitions(
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

CycleDetectionResult CycleDetectionPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    CycleDetectionResult result;

    // Step 1: Collect all class definitions
    std::unordered_map<std::string, const parser::ast::class_definition*> classes;
    collect_class_definitions(expressions, classes);
    result.classes_scanned = classes.size();

    // Step 2: Build a map of class_name → set of Hold[T] target class names
    std::unordered_map<std::string, std::unordered_set<std::string>> own_targets;
    for (const auto& [name, cls_ptr] : classes) {
        own_targets[name] = extract_own_targets(cls_ptr->fields);
    }

    // Step 3: For each ordered pair (A, B) where A < B lexicographically,
    // check if A owns B and B owns A. Using ordered pairs avoids
    // reporting the same cycle twice.
    std::set<std::string> sorted_names;
    for (const auto& [name, _] : classes) {
        sorted_names.insert(name);
    }

    for (auto it_a = sorted_names.begin(); it_a != sorted_names.end(); ++it_a) {
        for (auto it_b = std::next(it_a); it_b != sorted_names.end(); ++it_b) {
            const auto& a = *it_a;
            const auto& b = *it_b;

            const auto& a_targets = own_targets[a];
            const auto& b_targets = own_targets[b];

            if (a_targets.count(b) > 0 && b_targets.count(a) > 0) {
                emit_mutual_cycle_warning(a, b, source_file, result);
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Diagnostic emission
// ---------------------------------------------------------------------------

void CycleDetectionPass::emit_mutual_cycle_warning(
    const std::string& class_a,
    const std::string& class_b,
    const std::string& source_file,
    CycleDetectionResult& result
) {
    std::string message =
        "potential reference cycle: " + class_a + " and " + class_b +
        " have mutual Hold[T] references — consider using View[T] for one direction";

    result.diagnostics.push_back(CycleDetectionDiagnostic{
        .level = CycleDetectionDiagnostic::Level::Warning,
        .code = "W4001",
        .message = message,
        .source_file = source_file,
        .line = 0,
        .column = 0
    });

    result.mutual_cycles_detected++;
}

} // namespace meld::compiler
