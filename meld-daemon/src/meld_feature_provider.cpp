#include "meld/daemon/meld_feature_provider.hpp"

#include <algorithm>
#include <functional>

namespace meld::daemon {

MeldFeatureProvider::MeldFeatureProvider(const SemanticModel& model)
    : model_(model) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::shared_ptr<ASTNode> MeldFeatureProvider::find_node_recursive(
    const std::shared_ptr<ASTNode>& root,
    const std::string& name) const {
    if (!root) return nullptr;
    if (root->name == name && !root->name.empty()) return root;
    for (const auto& child : root->children) {
        auto found = find_node_recursive(child, name);
        if (found) return found;
    }
    return nullptr;
}

std::shared_ptr<ASTNode> MeldFeatureProvider::find_node(
    const std::filesystem::path& file,
    const std::string& name) const {
    auto ast = model_.get_ast(file);
    if (!ast) return nullptr;
    return find_node_recursive(ast, name);
}

std::string MeldFeatureProvider::classify_runtime_type(const std::string& ast_kind) {
    if (ast_kind == "function_definition") return "Function";
    if (ast_kind == "val_declaration") return "Value";
    if (ast_kind == "struct") return "Struct";
    if (ast_kind == "enum") return "Enum";
    if (ast_kind == "trait") return "Trait";
    if (ast_kind == "macro_definition") return "Macro";
    if (ast_kind == "tree_init") return "TreeInit";
    if (ast_kind == "refinement_type") return "RefinementType";
    if (ast_kind == "dispatch") return "Dispatch";
    if (ast_kind == "literal") return "Literal";
    if (ast_kind == "module") return "Module";
    return "Unknown";
}

bool MeldFeatureProvider::is_valid_predicate(const std::string& predicate) {
    if (predicate.empty()) return false;
    // Basic validation: must contain at least one comparison or logical operator
    static const std::vector<std::string> operators = {
        ">", "<", ">=", "<=", "==", "!=", "&&", "||", "!"};
    for (const auto& op : operators) {
        if (predicate.find(op) != std::string::npos) return true;
    }
    // Also accept simple boolean identifiers
    if (predicate == "true" || predicate == "false") return true;
    return false;
}

std::vector<std::string> MeldFeatureProvider::extract_property_names(
    const std::shared_ptr<ASTNode>& node) {
    std::vector<std::string> names;
    if (!node) return names;
    for (const auto& child : node->children) {
        if (!child->name.empty()) {
            names.push_back(child->name);
        }
    }
    return names;
}

// ---------------------------------------------------------------------------
// Homoiconic analysis (Req 21.1)
// ---------------------------------------------------------------------------

HomoiconicAnalysis MeldFeatureProvider::analyze_homoiconic(
    const std::filesystem::path& file,
    const std::string& node_name) const {
    HomoiconicAnalysis result;
    result.node_kind = "";
    result.runtime_type = "Unknown";
    result.ast_runtime_match = false;

    auto node = find_node(file, node_name);
    if (!node) {
        result.description = "Node not found: " + node_name;
        return result;
    }

    result.node_kind = node->kind;
    result.runtime_type = classify_runtime_type(node->kind);
    // In homoiconic code, AST representation IS the runtime value
    result.ast_runtime_match = (result.runtime_type != "Unknown");
    result.description = "AST kind '" + node->kind + "' maps to runtime type '" +
                         result.runtime_type + "'";
    return result;
}

// ---------------------------------------------------------------------------
// Macro system support (Req 21.2)
// ---------------------------------------------------------------------------

MacroAnalysis MeldFeatureProvider::analyze_macro(
    const std::filesystem::path& file,
    const std::string& macro_name) const {
    MacroAnalysis result;
    result.macro_name = macro_name;
    result.macro_kind = "unknown";
    result.valid = false;

    auto node = find_node(file, macro_name);
    if (!node) {
        result.expansion_hint = "Macro not found: " + macro_name;
        return result;
    }

    // Determine macro kind from AST node kind
    if (node->kind == "macro_definition" || node->kind == "macro") {
        result.macro_kind = "syntax";
        result.valid = true;
    } else if (node->kind == "derive_macro") {
        result.macro_kind = "derive";
        result.valid = true;
    } else if (node->kind == "attribute_macro") {
        result.macro_kind = "attribute";
        result.valid = true;
    } else if (node->kind == "meta_macro") {
        result.macro_kind = "meta";
        result.valid = true;
    }

    // Extract parameters from children
    for (const auto& child : node->children) {
        if (!child->name.empty()) {
            result.parameters.push_back(child->name);
        }
    }

    if (result.valid) {
        result.expansion_hint = "Macro '" + macro_name + "' (" +
                                result.macro_kind + ") with " +
                                std::to_string(result.parameters.size()) +
                                " parameter(s)";
    }
    return result;
}

// ---------------------------------------------------------------------------
// Refinement type evaluation (Req 21.3)
// ---------------------------------------------------------------------------

RefinementResult MeldFeatureProvider::evaluate_refinement(
    const std::filesystem::path& file,
    const std::string& type_name) const {
    RefinementResult result;
    result.type_name = type_name;

    auto node = find_node(file, type_name);
    if (!node) {
        result.violation_message = "Type not found: " + type_name;
        return result;
    }

    result.base_type = node->type_info;

    // Look for a predicate in the node's effects (used as predicate storage)
    if (!node->effects.empty()) {
        result.predicate = node->effects[0];
        result.predicate_valid = is_valid_predicate(result.predicate);
        result.constraint_satisfied = result.predicate_valid;
        if (!result.predicate_valid) {
            result.violation_message = "Invalid predicate expression: " +
                                       result.predicate;
        }
    } else {
        // No predicate means it's a plain type, not a refinement
        result.predicate = "";
        result.predicate_valid = false;
        result.violation_message = "No refinement predicate found for type: " +
                                    type_name;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Multiple dispatch resolution (Req 21.4)
// ---------------------------------------------------------------------------

DispatchResolution MeldFeatureProvider::resolve_dispatch(
    const std::filesystem::path& file,
    const std::string& function_name,
    const std::vector<std::string>& argument_types) const {
    DispatchResolution result;
    result.function_name = function_name;
    result.argument_types = argument_types;
    result.resolved = false;
    result.ambiguous = false;

    // Collect all overloads of the function across indexed files
    std::vector<std::shared_ptr<ASTNode>> overloads;

    auto collect = [&](const std::filesystem::path& f) {
        auto ast = model_.get_ast(f);
        if (!ast) return;
        std::function<void(const std::shared_ptr<ASTNode>&)> walk;
        walk = [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            if (node->name == function_name &&
                (node->kind == "function_definition" || node->kind == "dispatch")) {
                overloads.push_back(node);
            }
            for (const auto& child : node->children) {
                walk(child);
            }
        };
        walk(ast);
    };

    // Search the specified file first, then all indexed files
    collect(file);
    for (const auto& f : model_.get_indexed_files()) {
        if (f != file) collect(f);
    }

    if (overloads.empty()) {
        return result;
    }

    // Build candidate list
    for (const auto& overload : overloads) {
        result.candidates.push_back(overload->type_info);
    }

    if (overloads.size() == 1) {
        result.resolved = true;
        result.resolved_overload = overloads[0]->type_info;
    } else {
        // Try to resolve by matching argument types
        std::vector<std::shared_ptr<ASTNode>> matches;
        for (const auto& overload : overloads) {
            // Simple matching: check if type_info contains all argument types
            bool all_match = true;
            for (const auto& arg_type : argument_types) {
                if (overload->type_info.find(arg_type) == std::string::npos) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) matches.push_back(overload);
        }

        if (matches.size() == 1) {
            result.resolved = true;
            result.resolved_overload = matches[0]->type_info;
        } else if (matches.size() > 1) {
            result.ambiguous = true;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Tree initialization validation (Req 21.5)
// ---------------------------------------------------------------------------

TreeInitValidation MeldFeatureProvider::validate_tree_init(
    const std::filesystem::path& file,
    const std::string& type_name) const {
    TreeInitValidation result;
    result.type_name = type_name;
    result.valid_syntax = false;
    result.valid_nesting = false;

    auto node = find_node(file, type_name);
    if (!node) {
        result.errors.push_back("Type not found: " + type_name);
        return result;
    }

    // Check if the node is a valid tree init construct
    if (node->kind == "tree_init" || node->kind == "struct" ||
        node->kind == "constructor") {
        result.valid_syntax = true;
    } else {
        result.errors.push_back("Node '" + type_name +
                                "' is not a tree initialization construct (kind: " +
                                node->kind + ")");
        return result;
    }

    // Extract and validate property names
    result.property_names = extract_property_names(node);

    // Check for duplicate property names
    std::vector<std::string> sorted_names = result.property_names;
    std::sort(sorted_names.begin(), sorted_names.end());
    auto dup = std::adjacent_find(sorted_names.begin(), sorted_names.end());
    if (dup != sorted_names.end()) {
        result.errors.push_back("Duplicate property: " + *dup);
    }

    // Validate nesting: children with children indicate nested properties
    result.valid_nesting = true;
    for (const auto& child : node->children) {
        if (!child->children.empty()) {
            // Nested property — validate recursively
            auto nested_names = extract_property_names(child);
            std::vector<std::string> sorted_nested = nested_names;
            std::sort(sorted_nested.begin(), sorted_nested.end());
            auto nested_dup = std::adjacent_find(sorted_nested.begin(),
                                                  sorted_nested.end());
            if (nested_dup != sorted_nested.end()) {
                result.valid_nesting = false;
                result.errors.push_back("Duplicate nested property in '" +
                                        child->name + "': " + *nested_dup);
            }
        }
    }

    return result;
}

}  // namespace meld::daemon
