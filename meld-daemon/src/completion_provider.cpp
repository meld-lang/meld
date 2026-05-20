#include "meld/daemon/completion_provider.hpp"

#include <algorithm>
#include <cctype>
#include <functional>

namespace meld::daemon {

CompletionProvider::CompletionProvider(const SemanticModel& model)
    : model_(model) {}

std::string CompletionProvider::get_prefix(const std::string& line_text, uint32_t column) {
    if (column == 0 || line_text.empty()) return "";
    uint32_t end = std::min(column, static_cast<uint32_t>(line_text.size()));
    uint32_t start = end;
    while (start > 0) {
        char c = line_text[start - 1];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') --start;
        else break;
    }
    return line_text.substr(start, end - start);
}

int CompletionProvider::count_commas_before(const std::string& line_text, uint32_t column) {
    int commas = 0;
    int depth = 0;
    uint32_t end = std::min(column, static_cast<uint32_t>(line_text.size()));
    // Scan backwards from cursor to find the opening paren
    for (uint32_t i = 0; i < end; ++i) {
        char c = line_text[i];
        if (c == '(') depth++;
        else if (c == ')') depth--;
        else if (c == ',' && depth == 1) commas++;
    }
    return commas;
}

CompletionContext CompletionProvider::determine_context(
    const std::filesystem::path& /*file*/,
    uint32_t /*line*/, uint32_t column,
    const std::string& line_text) const {

    if (column == 0 || line_text.empty()) return CompletionContext::General;

    uint32_t pos = std::min(column, static_cast<uint32_t>(line_text.size()));

    // Check for type annotation context: preceded by ':'
    for (uint32_t i = pos; i > 0; --i) {
        char c = line_text[i - 1];
        if (c == ':') return CompletionContext::TypeAnnotation;
        if (c == ' ' || c == '\t') continue;
        break;
    }

    // Check for member access: preceded by '.'
    if (pos > 0 && line_text[pos - 1] == '.') return CompletionContext::MemberAccess;

    // Check for function call context: inside parentheses
    int paren_depth = 0;
    for (uint32_t i = 0; i < pos; ++i) {
        if (line_text[i] == '(') paren_depth++;
        else if (line_text[i] == ')') paren_depth--;
    }
    if (paren_depth > 0) return CompletionContext::FunctionCall;

    // Check for import context
    std::string trimmed = line_text.substr(0, pos);
    // Remove leading whitespace
    size_t first = trimmed.find_first_not_of(" \t");
    if (first != std::string::npos) {
        std::string start = trimmed.substr(first);
        if (start.rfind("import ", 0) == 0 || start.rfind("imp ", 0) == 0) {
            return CompletionContext::Import;
        }
    }

    // Check for effect context
    if (first != std::string::npos) {
        std::string start = trimmed.substr(first);
        if (start.rfind("effect ", 0) == 0 || start.rfind("handle ", 0) == 0 ||
            start.rfind("perform ", 0) == 0) {
            return CompletionContext::Effect;
        }
    }

    return CompletionContext::General;
}

void CompletionProvider::collect_symbols_from_ast(
    const std::shared_ptr<ASTNode>& node,
    std::vector<CompletionItem>& items) const {
    if (!node) return;

    if (!node->name.empty()) {
        CompletionItem item;
        item.label = node->name;
        item.detail = node->type_info;

        if (node->kind == "function_definition" || node->kind == "fnc") {
            item.kind = "function";
            item.insert_text = node->name + "(";
            item.sort_priority = 1;
        } else if (node->kind == "struct" || node->kind == "enum" ||
                   node->kind == "trait" || node->kind == "type_alias") {
            item.kind = "type";
            item.sort_priority = 2;
        } else if (node->kind == "val_declaration" || node->kind == "var_declaration" ||
                   node->kind == "let_declaration") {
            item.kind = "variable";
            item.sort_priority = 0;
        } else {
            item.kind = "variable";
            item.sort_priority = 3;
        }

        items.push_back(std::move(item));
    }

    for (const auto& child : node->children) {
        collect_symbols_from_ast(child, items);
    }
}

std::vector<CompletionItem> CompletionProvider::collect_scope_symbols(
    const std::filesystem::path& file) const {
    std::vector<CompletionItem> items;

    // Collect symbols from the current file's AST
    auto ast = model_.get_ast(file);
    collect_symbols_from_ast(ast, items);

    // Collect exported symbols from imported files
    auto indexed_files = model_.get_indexed_files();
    for (const auto& f : indexed_files) {
        if (f == file) continue;
        auto other_ast = model_.get_ast(f);
        if (!other_ast) continue;
        // Only collect top-level exported symbols from other files
        for (const auto& child : other_ast->children) {
            if (child && !child->name.empty()) {
                CompletionItem item;
                item.label = child->name;
                item.detail = child->type_info;
                item.documentation = "from " + f.filename().string();
                if (child->kind == "function_definition" || child->kind == "fnc") {
                    item.kind = "function";
                    item.insert_text = child->name + "(";
                } else if (child->kind == "struct" || child->kind == "enum" ||
                           child->kind == "trait" || child->kind == "type_alias") {
                    item.kind = "type";
                } else {
                    item.kind = "import";
                }
                item.sort_priority = 5;
                items.push_back(std::move(item));
            }
        }
    }

    return items;
}

std::vector<CompletionItem> CompletionProvider::collect_type_completions(
    const std::filesystem::path& file) const {
    std::vector<CompletionItem> items;

    // Add built-in types
    for (const auto& t : builtin_types()) {
        items.push_back({t, "type", "built-in type", "", t, 0});
    }

    // Add user-defined types from the SemanticModel
    auto ast = model_.get_ast(file);
    if (ast) {
        for (const auto& child : ast->children) {
            if (child && (child->kind == "struct" || child->kind == "enum" ||
                          child->kind == "trait" || child->kind == "type_alias")) {
                items.push_back({child->name, "type", child->type_info,
                                 "", child->name, 1});
            }
        }
    }

    // Add types from other indexed files
    auto indexed_files = model_.get_indexed_files();
    for (const auto& f : indexed_files) {
        if (f == file) continue;
        auto other_ast = model_.get_ast(f);
        if (!other_ast) continue;
        for (const auto& child : other_ast->children) {
            if (child && (child->kind == "struct" || child->kind == "enum" ||
                          child->kind == "trait" || child->kind == "type_alias")) {
                items.push_back({child->name, "type",
                                 child->type_info,
                                 "from " + f.filename().string(),
                                 child->name, 2});
            }
        }
    }

    return items;
}

std::vector<CompletionItem> CompletionProvider::collect_import_completions() const {
    std::vector<CompletionItem> items;
    auto indexed_files = model_.get_indexed_files();
    for (const auto& f : indexed_files) {
        std::string module_name = f.stem().string();
        items.push_back({module_name, "import", f.string(), "", module_name, 0});
    }
    return items;
}

std::vector<CompletionItem> CompletionProvider::filter_by_prefix(
    const std::vector<CompletionItem>& items,
    const std::string& prefix) {
    if (prefix.empty()) return items;

    std::vector<CompletionItem> filtered;
    std::string lower_prefix;
    lower_prefix.reserve(prefix.size());
    for (char c : prefix)
        lower_prefix += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    for (const auto& item : items) {
        std::string lower_label;
        lower_label.reserve(item.label.size());
        for (char c : item.label)
            lower_label += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower_label.find(lower_prefix) == 0) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

std::vector<CompletionItem> CompletionProvider::filter_for_context(
    const std::vector<CompletionItem>& items,
    CompletionContext context) {
    std::vector<CompletionItem> filtered;
    for (const auto& item : items) {
        switch (context) {
            case CompletionContext::TypeAnnotation:
                // Only types in type annotation context
                if (item.kind == "type") filtered.push_back(item);
                break;
            case CompletionContext::Import:
                // Only importable modules
                if (item.kind == "import") filtered.push_back(item);
                break;
            case CompletionContext::FunctionCall:
                // Variables and functions are valid in function call args
                filtered.push_back(item);
                break;
            case CompletionContext::General:
            case CompletionContext::MemberAccess:
            case CompletionContext::Effect:
            default:
                filtered.push_back(item);
                break;
        }
    }
    return filtered;
}

std::optional<SignatureInfo> CompletionProvider::extract_signature(
    const std::shared_ptr<ASTNode>& node) const {
    if (!node) return std::nullopt;
    if (node->kind != "function_definition" && node->kind != "fnc")
        return std::nullopt;

    SignatureInfo sig;
    sig.label = node->name + "(";
    sig.documentation = node->type_info;

    // Extract parameters from children
    for (const auto& child : node->children) {
        if (child && (child->kind == "parameter" || child->kind == "param")) {
            ParameterInfo param;
            param.name = child->name;
            param.type = child->type_info;
            param.documentation = "";
            sig.parameters.push_back(std::move(param));
        }
    }

    // Build the label string
    for (size_t i = 0; i < sig.parameters.size(); ++i) {
        if (i > 0) sig.label += ", ";
        sig.label += sig.parameters[i].name;
        if (!sig.parameters[i].type.empty())
            sig.label += ": " + sig.parameters[i].type;
    }
    sig.label += ")";
    if (!node->type_info.empty()) sig.label += " -> " + node->type_info;

    return sig;
}

SignatureHelpResult CompletionProvider::get_signature_help(
    const std::filesystem::path& file,
    uint32_t /*line*/, uint32_t column,
    const std::string& line_text) const {

    SignatureHelpResult result;

    // Find the function name before the opening paren
    uint32_t pos = std::min(column, static_cast<uint32_t>(line_text.size()));
    // Walk back to find '('
    int paren_depth = 0;
    uint32_t paren_pos = pos;
    for (uint32_t i = pos; i > 0; --i) {
        char c = line_text[i - 1];
        if (c == ')') paren_depth++;
        else if (c == '(') {
            if (paren_depth == 0) { paren_pos = i - 1; break; }
            paren_depth--;
        }
    }

    // Extract function name before the paren
    std::string func_name = get_prefix(line_text, paren_pos);
    if (func_name.empty()) return result;

    // Search for the function in the SemanticModel
    auto ast = model_.get_ast(file);
    if (!ast) return result;

    // Search recursively for the function
    std::function<std::optional<SignatureInfo>(const std::shared_ptr<ASTNode>&)> find_func;
    find_func = [&](const std::shared_ptr<ASTNode>& node) -> std::optional<SignatureInfo> {
        if (!node) return std::nullopt;
        if (node->name == func_name &&
            (node->kind == "function_definition" || node->kind == "fnc")) {
            return extract_signature(node);
        }
        for (const auto& child : node->children) {
            if (auto sig = find_func(child)) return sig;
        }
        return std::nullopt;
    };

    if (auto sig = find_func(ast)) {
        sig->active_parameter = count_commas_before(line_text, column);
        result.signatures.push_back(std::move(*sig));
    }

    // Also search other indexed files
    if (result.signatures.empty()) {
        auto indexed_files = model_.get_indexed_files();
        for (const auto& f : indexed_files) {
            if (f == file) continue;
            auto other_ast = model_.get_ast(f);
            if (auto sig = find_func(other_ast)) {
                sig->active_parameter = count_commas_before(line_text, column);
                result.signatures.push_back(std::move(*sig));
                break;
            }
        }
    }

    return result;
}

CompletionResult CompletionProvider::get_completions(
    const std::filesystem::path& file,
    uint32_t line, uint32_t column,
    const std::string& line_text) const {

    CompletionResult result;
    auto context = determine_context(file, line, column, line_text);
    std::string prefix = get_prefix(line_text, column);

    std::vector<CompletionItem> all_items;

    switch (context) {
        case CompletionContext::TypeAnnotation:
            all_items = collect_type_completions(file);
            break;
        case CompletionContext::Import:
            all_items = collect_import_completions();
            break;
        default: {
            // Collect scope symbols
            all_items = collect_scope_symbols(file);
            // Add keywords in general context
            if (context == CompletionContext::General) {
                for (const auto& kw : completion_keywords()) {
                    all_items.push_back({kw, "keyword", "keyword", "", kw, 10});
                }
            }
            break;
        }
    }

    // Apply context-sensitive filtering
    all_items = filter_for_context(all_items, context);

    // Apply prefix filtering
    if (!prefix.empty()) {
        all_items = filter_by_prefix(all_items, prefix);
    }

    // Sort by priority then alphabetically
    std::sort(all_items.begin(), all_items.end(),
              [](const CompletionItem& a, const CompletionItem& b) {
                  if (a.sort_priority != b.sort_priority)
                      return a.sort_priority < b.sort_priority;
                  return a.label < b.label;
              });

    result.items = std::move(all_items);
    return result;
}

}  // namespace meld::daemon
