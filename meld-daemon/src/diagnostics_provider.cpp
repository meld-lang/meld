#include "meld/daemon/diagnostics_provider.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace meld::daemon {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DiagnosticsProvider::DiagnosticsProvider(const SemanticModel& model)
    : model_(model) {}

// ---------------------------------------------------------------------------
// Grammar conformance (Req 17.1)
// ---------------------------------------------------------------------------

GrammarCheckResult DiagnosticsProvider::check_grammar(
    const std::filesystem::path& file,
    const std::string& source) const {

    GrammarCheckResult result;

    if (source.empty()) return result;

    // Validate basic structural grammar rules of Meld source code.
    // Check for balanced braces, brackets, and parentheses.
    int brace_depth = 0;
    int bracket_depth = 0;
    int paren_depth = 0;
    uint32_t line_num = 1;
    uint32_t col = 0;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        col++;

        if (c == '\n') {
            line_num++;
            col = 0;
            in_line_comment = false;
            continue;
        }
        if (in_line_comment) continue;
        if (in_block_comment) {
            if (c == '*' && i + 1 < source.size() && source[i + 1] == '/') {
                in_block_comment = false;
                ++i;
                col++;
            }
            continue;
        }

        // String handling
        if (c == '"' && !in_string) { in_string = true; continue; }
        if (c == '"' && in_string && (i == 0 || source[i - 1] != '\\')) {
            in_string = false; continue;
        }
        if (in_string) continue;

        // Comment handling
        if (c == '/' && i + 1 < source.size()) {
            if (source[i + 1] == '/') { in_line_comment = true; continue; }
            if (source[i + 1] == '*') { in_block_comment = true; ++i; col++; continue; }
        }

        if (c == '{') brace_depth++;
        else if (c == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                result.conforms = false;
                result.diagnostics.push_back({
                    file, line_num, col,
                    TypeDiagnosticSeverity::Error, "E1000",
                    "Unmatched closing brace '}'", "Remove extra '}'"
                });
            }
        }
        else if (c == '[') bracket_depth++;
        else if (c == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                result.conforms = false;
                result.diagnostics.push_back({
                    file, line_num, col,
                    TypeDiagnosticSeverity::Error, "E1000",
                    "Unmatched closing bracket ']'", "Remove extra ']'"
                });
            }
        }
        else if (c == '(') paren_depth++;
        else if (c == ')') {
            paren_depth--;
            if (paren_depth < 0) {
                result.conforms = false;
                result.diagnostics.push_back({
                    file, line_num, col,
                    TypeDiagnosticSeverity::Error, "E1000",
                    "Unmatched closing parenthesis ')'", "Remove extra ')'"
                });
            }
        }
    }

    if (brace_depth > 0) {
        result.conforms = false;
        result.diagnostics.push_back({
            file, line_num, col,
            TypeDiagnosticSeverity::Error, "E1000",
            "Unclosed brace '{' — expected '}'", "Add missing '}'"
        });
    }
    if (bracket_depth > 0) {
        result.conforms = false;
        result.diagnostics.push_back({
            file, line_num, col,
            TypeDiagnosticSeverity::Error, "E1000",
            "Unclosed bracket '[' — expected ']'", "Add missing ']'"
        });
    }
    if (paren_depth > 0) {
        result.conforms = false;
        result.diagnostics.push_back({
            file, line_num, col,
            TypeDiagnosticSeverity::Error, "E1000",
            "Unclosed parenthesis '(' — expected ')'", "Add missing ')'"
        });
    }
    if (in_string) {
        result.conforms = false;
        result.diagnostics.push_back({
            file, line_num, col,
            TypeDiagnosticSeverity::Error, "E1000",
            "Unterminated string literal", "Add closing '\"'"
        });
    }
    if (in_block_comment) {
        result.conforms = false;
        result.diagnostics.push_back({
            file, line_num, col,
            TypeDiagnosticSeverity::Error, "E1000",
            "Unterminated block comment", "Add closing '*/'"
        });
    }

    return result;
}

// ---------------------------------------------------------------------------
// Type error reporting (Req 17.2)
// ---------------------------------------------------------------------------

TypeDiagnostic DiagnosticsProvider::make_type_error(
    const std::filesystem::path& file,
    uint32_t line, uint32_t column,
    const std::string& expected_type,
    const std::string& actual_type,
    const std::string& context) {

    std::ostringstream msg;
    msg << "Type mismatch: expected '" << expected_type
        << "' but found '" << actual_type << "'";
    if (!context.empty()) msg << " in " << context;

    std::ostringstream fix;
    fix << "Change type to '" << expected_type << "'";

    return TypeDiagnostic{
        file, line, column,
        TypeDiagnosticSeverity::Error, "E1001",
        msg.str(), fix.str()
    };
}

TypeCheckResult DiagnosticsProvider::check_types(
    const std::filesystem::path& file) const {

    TypeCheckResult result;
    auto ast = model_.get_ast(file);
    if (!ast) return result;

    // Walk the AST looking for type annotations and checking consistency.
    // For each node with type_info, verify children's types are compatible.
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;

        // Check function return type vs body type
        if ((node->kind == "function_definition" || node->kind == "fnc") &&
            !node->type_info.empty()) {
            for (const auto& child : node->children) {
                if (child && child->kind == "return_statement" &&
                    !child->type_info.empty() &&
                    child->type_info != node->type_info) {
                    result.errors.push_back(make_type_error(
                        file, child->location.line, child->location.column,
                        node->type_info, child->type_info,
                        "return statement of '" + node->name + "'"));
                }
            }
        }

        // Check variable declarations with explicit type annotations
        if ((node->kind == "val_declaration" || node->kind == "var_declaration" ||
             node->kind == "let_declaration") &&
            !node->type_info.empty()) {
            for (const auto& child : node->children) {
                if (child && child->kind == "initializer" &&
                    !child->type_info.empty() &&
                    child->type_info != node->type_info) {
                    result.errors.push_back(make_type_error(
                        file, node->location.line, node->location.column,
                        node->type_info, child->type_info,
                        "initialization of '" + node->name + "'"));
                }
            }
        }

        // Check function call argument types
        if (node->kind == "call_expression" && !node->children.empty()) {
            auto callee = node->children[0];
            if (callee && (callee->kind == "function_definition" || callee->kind == "fnc")) {
                // Match argument types against parameter types
                size_t param_idx = 0;
                for (size_t i = 1; i < node->children.size(); ++i) {
                    auto arg = node->children[i];
                    if (!arg) continue;
                    // Find corresponding parameter
                    if (param_idx < callee->children.size()) {
                        auto param = callee->children[param_idx];
                        if (param && !param->type_info.empty() &&
                            !arg->type_info.empty() &&
                            param->type_info != arg->type_info) {
                            result.errors.push_back(make_type_error(
                                file, arg->location.line, arg->location.column,
                                param->type_info, arg->type_info,
                                "argument " + std::to_string(param_idx + 1) +
                                " of call to '" + callee->name + "'"));
                        }
                        param_idx++;
                    }
                }
            }
        }

        for (const auto& child : node->children) walk(child);
    };

    walk(ast);
    return result;
}

// ---------------------------------------------------------------------------
// Undefined symbol detection (Req 17.3)
// ---------------------------------------------------------------------------

int DiagnosticsProvider::edit_distance(const std::string& a, const std::string& b) {
    size_t m = a.size(), n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                                 dp[i - 1][j - 1] + cost});
        }
    }
    return dp[m][n];
}

std::vector<SymbolSuggestion> DiagnosticsProvider::find_similar(
    const std::string& name,
    const std::vector<std::string>& available,
    int max_distance) {

    std::vector<SymbolSuggestion> suggestions;
    for (const auto& sym : available) {
        int dist = edit_distance(name, sym);
        if (dist > 0 && dist <= max_distance) {
            suggestions.push_back({sym, dist});
        }
    }
    std::sort(suggestions.begin(), suggestions.end(),
              [](const SymbolSuggestion& a, const SymbolSuggestion& b) {
                  return a.edit_distance < b.edit_distance;
              });
    return suggestions;
}

void DiagnosticsProvider::collect_defined_symbols_impl(
    const std::shared_ptr<ASTNode>& node,
    std::vector<std::string>& out) {
    if (!node) return;
    if (!node->name.empty() &&
        (node->kind == "function_definition" || node->kind == "fnc" ||
         node->kind == "val_declaration" || node->kind == "var_declaration" ||
         node->kind == "let_declaration" || node->kind == "struct" ||
         node->kind == "enum" || node->kind == "trait" ||
         node->kind == "type_alias" || node->kind == "parameter")) {
        out.push_back(node->name);
    }
    for (const auto& child : node->children) {
        collect_defined_symbols_impl(child, out);
    }
}

std::vector<std::string> DiagnosticsProvider::collect_defined_symbols(
    const std::shared_ptr<ASTNode>& node) const {
    std::vector<std::string> symbols;
    collect_defined_symbols_impl(node, symbols);
    return symbols;
}

void DiagnosticsProvider::collect_referenced_symbols_impl(
    const std::shared_ptr<ASTNode>& node,
    std::vector<std::string>& out) {
    if (!node) return;
    if (!node->name.empty() &&
        (node->kind == "reference" || node->kind == "identifier" ||
         node->kind == "call_expression" || node->kind == "type_reference")) {
        out.push_back(node->name);
    }
    for (const auto& child : node->children) {
        collect_referenced_symbols_impl(child, out);
    }
}

std::vector<std::string> DiagnosticsProvider::collect_referenced_symbols(
    const std::shared_ptr<ASTNode>& node) const {
    std::vector<std::string> refs;
    collect_referenced_symbols_impl(node, refs);
    return refs;
}

std::vector<UndefinedSymbolResult> DiagnosticsProvider::detect_undefined_symbols(
    const std::filesystem::path& file) const {

    std::vector<UndefinedSymbolResult> results;
    auto ast = model_.get_ast(file);
    if (!ast) return results;

    // Collect all defined symbols (from this file + all indexed files)
    std::unordered_set<std::string> defined;
    auto local_defs = collect_defined_symbols(ast);
    defined.insert(local_defs.begin(), local_defs.end());

    // Add symbols from other indexed files
    auto indexed_files = model_.get_indexed_files();
    for (const auto& f : indexed_files) {
        if (f == file) continue;
        auto other_ast = model_.get_ast(f);
        if (!other_ast) continue;
        auto other_defs = collect_defined_symbols(other_ast);
        defined.insert(other_defs.begin(), other_defs.end());
    }

    // Add Meld built-in symbols
    static const std::vector<std::string> builtins = {
        "Int", "Float", "String", "Bool", "Unit", "Byte",
        "List", "Map", "Set", "Option", "Result", "Future",
        "Array", "Tuple", "Char", "Any", "Nothing",
        "true", "false", "println", "print", "assert"
    };
    defined.insert(builtins.begin(), builtins.end());

    // Find all references and check if they're defined
    std::vector<std::string> all_defined(defined.begin(), defined.end());

    std::function<void(const std::shared_ptr<ASTNode>&)> find_undefined;
    find_undefined = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (!node->name.empty() &&
            (node->kind == "reference" || node->kind == "identifier" ||
             node->kind == "type_reference") &&
            defined.find(node->name) == defined.end()) {
            UndefinedSymbolResult undef;
            undef.undefined_name = node->name;
            undef.file = file;
            undef.line = node->location.line;
            undef.column = node->location.column;
            undef.suggestions = find_similar(node->name, all_defined);
            results.push_back(std::move(undef));
        }
        for (const auto& child : node->children) find_undefined(child);
    };

    find_undefined(ast);
    return results;
}

// ---------------------------------------------------------------------------
// Refinement constraint validation (Req 17.4)
// ---------------------------------------------------------------------------

bool DiagnosticsProvider::satisfies_predicate(
    const std::string& predicate,
    const std::string& value) {

    // Parse simple predicates of the form "x > N", "x < N", "x >= N", "x <= N", "x == N"
    // where x is the variable and N is a numeric literal.
    if (predicate.empty() || value.empty()) return true;

    // Try to parse the value as a number
    double num_value = 0;
    try {
        num_value = std::stod(value);
    } catch (...) {
        return true;  // Non-numeric values pass by default
    }

    // Extract operator and threshold from predicate
    static const std::regex pred_re(R"(\w+\s*(>=|<=|!=|==|>|<)\s*(-?\d+\.?\d*))");
    std::smatch match;
    if (!std::regex_search(predicate, match, pred_re)) return true;

    std::string op = match[1].str();
    double threshold = std::stod(match[2].str());

    if (op == ">")  return num_value > threshold;
    if (op == "<")  return num_value < threshold;
    if (op == ">=") return num_value >= threshold;
    if (op == "<=") return num_value <= threshold;
    if (op == "==") return num_value == threshold;
    if (op == "!=") return num_value != threshold;

    return true;
}

std::vector<RefinementViolation> DiagnosticsProvider::check_refinement_constraints(
    const std::filesystem::path& file) const {

    std::vector<RefinementViolation> violations;
    auto ast = model_.get_ast(file);
    if (!ast) return violations;

    // Walk the AST looking for refinement type annotations.
    // Refinement types in Meld look like: val x: Int{x > 0} = -5
    // The type_info field encodes the constraint, e.g. "Int{x > 0}"
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;

        if ((node->kind == "val_declaration" || node->kind == "var_declaration" ||
             node->kind == "let_declaration") &&
            !node->type_info.empty()) {

            // Check if type_info contains a refinement predicate: Type{predicate}
            auto brace_start = node->type_info.find('{');
            auto brace_end = node->type_info.rfind('}');
            if (brace_start != std::string::npos && brace_end != std::string::npos &&
                brace_end > brace_start) {
                std::string predicate = node->type_info.substr(
                    brace_start + 1, brace_end - brace_start - 1);

                // Check if there's an initializer child with a value
                for (const auto& child : node->children) {
                    if (child && (child->kind == "initializer" || child->kind == "literal") &&
                        !child->name.empty()) {
                        if (!satisfies_predicate(predicate, child->name)) {
                            violations.push_back({
                                node->type_info.substr(0, brace_start),
                                predicate,
                                child->name,
                                file,
                                node->location.line,
                                node->location.column
                            });
                        }
                    }
                }
            }
        }

        for (const auto& child : node->children) walk(child);
    };

    walk(ast);
    return violations;
}

// ---------------------------------------------------------------------------
// Dispatch ambiguity detection (Req 17.5)
// ---------------------------------------------------------------------------

bool DiagnosticsProvider::signatures_ambiguous(
    const std::string& sig_a,
    const std::string& sig_b,
    const std::vector<std::string>& arg_types) {

    // Parse parameter types from signatures of the form "(Type1, Type2) -> RetType"
    auto parse_params = [](const std::string& sig) -> std::vector<std::string> {
        std::vector<std::string> params;
        auto paren_start = sig.find('(');
        auto paren_end = sig.find(')');
        if (paren_start == std::string::npos || paren_end == std::string::npos)
            return params;
        std::string inner = sig.substr(paren_start + 1, paren_end - paren_start - 1);
        std::istringstream ss(inner);
        std::string token;
        while (std::getline(ss, token, ',')) {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos)
                params.push_back(token.substr(start, end - start + 1));
        }
        return params;
    };

    auto params_a = parse_params(sig_a);
    auto params_b = parse_params(sig_b);

    // Different arity — not ambiguous
    if (params_a.size() != params_b.size()) return false;
    if (params_a.size() != arg_types.size()) return false;

    // Check if both signatures could accept the given argument types.
    // A signature accepts an arg if the param type matches or is a supertype.
    auto type_matches = [](const std::string& param_type, const std::string& arg_type) -> bool {
        if (param_type == arg_type) return true;
        if (param_type == "Any") return true;
        // Number hierarchy: Int <: Float <: Number
        if (param_type == "Number" && (arg_type == "Int" || arg_type == "Float")) return true;
        if (param_type == "Float" && arg_type == "Int") return true;
        return false;
    };

    bool a_matches = true, b_matches = true;
    for (size_t i = 0; i < arg_types.size(); ++i) {
        if (!type_matches(params_a[i], arg_types[i])) a_matches = false;
        if (!type_matches(params_b[i], arg_types[i])) b_matches = false;
    }

    // Ambiguous if both match and neither is strictly more specific
    if (!a_matches || !b_matches) return false;

    // Check if one is strictly more specific than the other
    bool a_more_specific = false, b_more_specific = false;
    for (size_t i = 0; i < params_a.size(); ++i) {
        if (params_a[i] != params_b[i]) {
            if (type_matches(params_b[i], params_a[i]) &&
                !type_matches(params_a[i], params_b[i]))
                a_more_specific = true;
            if (type_matches(params_a[i], params_b[i]) &&
                !type_matches(params_b[i], params_a[i]))
                b_more_specific = true;
        }
    }

    // If one is strictly more specific, not ambiguous
    if (a_more_specific && !b_more_specific) return false;
    if (b_more_specific && !a_more_specific) return false;

    return true;  // Both match, neither more specific — ambiguous
}

std::vector<std::string> DiagnosticsProvider::collect_overloads(
    const std::string& function_name,
    const std::filesystem::path& file) const {

    std::vector<std::string> overloads;
    auto ast = model_.get_ast(file);
    if (!ast) return overloads;

    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (node->name == function_name &&
            (node->kind == "function_definition" || node->kind == "fnc") &&
            !node->type_info.empty()) {
            overloads.push_back(node->type_info);
        }
        for (const auto& child : node->children) walk(child);
    };

    walk(ast);
    return overloads;
}

std::vector<DispatchAmbiguity> DiagnosticsProvider::detect_dispatch_ambiguities(
    const std::filesystem::path& file) const {

    std::vector<DispatchAmbiguity> ambiguities;
    auto ast = model_.get_ast(file);
    if (!ast) return ambiguities;

    // Find all call expressions and check if the callee has ambiguous overloads
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;

        if (node->kind == "call_expression" && !node->name.empty()) {
            auto overloads = collect_overloads(node->name, file);
            if (overloads.size() < 2) {
                for (const auto& child : node->children) walk(child);
                return;
            }

            // Extract argument types from call children
            std::vector<std::string> arg_types;
            for (const auto& child : node->children) {
                if (child && !child->type_info.empty() &&
                    child->kind != "function_definition" && child->kind != "fnc") {
                    arg_types.push_back(child->type_info);
                }
            }

            // Check all pairs of overloads for ambiguity
            for (size_t i = 0; i < overloads.size(); ++i) {
                for (size_t j = i + 1; j < overloads.size(); ++j) {
                    if (signatures_ambiguous(overloads[i], overloads[j], arg_types)) {
                        ambiguities.push_back({
                            node->name,
                            {overloads[i], overloads[j]},
                            arg_types,
                            file,
                            node->location.line,
                            node->location.column
                        });
                    }
                }
            }
        }

        for (const auto& child : node->children) walk(child);
    };

    walk(ast);
    return ambiguities;
}

// ---------------------------------------------------------------------------
// Aggregate diagnostics
// ---------------------------------------------------------------------------

std::vector<TypeDiagnostic> DiagnosticsProvider::diagnose_file(
    const std::filesystem::path& file,
    const std::string& source) const {

    std::vector<TypeDiagnostic> all_diags;

    // Grammar check
    auto grammar = check_grammar(file, source);
    all_diags.insert(all_diags.end(),
                     grammar.diagnostics.begin(), grammar.diagnostics.end());

    // Type check
    auto types = check_types(file);
    all_diags.insert(all_diags.end(), types.errors.begin(), types.errors.end());
    all_diags.insert(all_diags.end(), types.warnings.begin(), types.warnings.end());

    // Undefined symbols
    auto undefs = detect_undefined_symbols(file);
    for (const auto& undef : undefs) {
        std::ostringstream msg;
        msg << "Undefined symbol '" << undef.undefined_name << "'";
        std::string suggestion;
        if (!undef.suggestions.empty()) {
            msg << "; did you mean '" << undef.suggestions[0].name << "'?";
            suggestion = "Replace with '" + undef.suggestions[0].name + "'";
        }
        all_diags.push_back({
            undef.file, undef.line, undef.column,
            TypeDiagnosticSeverity::Error, "E1003",
            msg.str(), suggestion
        });
    }

    // Refinement violations
    auto refinements = check_refinement_constraints(file);
    for (const auto& viol : refinements) {
        std::ostringstream msg;
        msg << "Refinement constraint violation: value " << viol.violating_value
            << " does not satisfy predicate '" << viol.predicate
            << "' for type " << viol.type_name;
        all_diags.push_back({
            viol.file, viol.line, viol.column,
            TypeDiagnosticSeverity::Error, "E1004",
            msg.str(), "Provide a value satisfying '" + viol.predicate + "'"
        });
    }

    // Dispatch ambiguities
    auto dispatch = detect_dispatch_ambiguities(file);
    for (const auto& amb : dispatch) {
        std::ostringstream msg;
        msg << "Ambiguous dispatch for '" << amb.function_name
            << "' — multiple overloads match: ";
        for (size_t i = 0; i < amb.candidate_signatures.size(); ++i) {
            if (i > 0) msg << ", ";
            msg << amb.candidate_signatures[i];
        }
        all_diags.push_back({
            amb.file, amb.line, amb.column,
            TypeDiagnosticSeverity::Error, "E1005",
            msg.str(), "Add explicit type annotations to disambiguate"
        });
    }

    return all_diags;
}

}  // namespace meld::daemon
