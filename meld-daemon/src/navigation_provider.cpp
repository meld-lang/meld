#include "meld/daemon/navigation_provider.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <unordered_set>

namespace meld::daemon {

NavigationProvider::NavigationProvider(const SemanticModel& model)
    : model_(model) {}

// ---------------------------------------------------------------------------
// Kind classification
// ---------------------------------------------------------------------------

std::string NavigationProvider::classify_kind(const std::string& ast_kind) {
    if (ast_kind == "function_definition" || ast_kind == "fnc")
        return "function";
    if (ast_kind == "struct")
        return "struct";
    if (ast_kind == "enum")
        return "enum";
    if (ast_kind == "trait")
        return "trait";
    if (ast_kind == "type_alias")
        return "type";
    if (ast_kind == "val_declaration" || ast_kind == "var_declaration" ||
        ast_kind == "let_declaration")
        return "variable";
    if (ast_kind == "parameter" || ast_kind == "param")
        return "parameter";
    return "variable";
}

// ---------------------------------------------------------------------------
// Query matching
// ---------------------------------------------------------------------------

bool NavigationProvider::matches_query(const std::string& name,
                                       const std::string& query) {
    if (query.empty()) return true;
    std::string lower_name, lower_query;
    lower_name.reserve(name.size());
    lower_query.reserve(query.size());
    for (char c : name)
        lower_name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char c : query)
        lower_query += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower_name.find(lower_query) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Go to definition (Req 18.1)
// ---------------------------------------------------------------------------

std::optional<NavigationLocation> NavigationProvider::find_definition_in_ast(
    const std::shared_ptr<ASTNode>& node,
    const std::filesystem::path& file,
    const std::string& symbol_name) const {
    if (!node) return std::nullopt;

    // A definition is a named node whose kind is a declaration/definition type
    if (node->name == symbol_name && !node->name.empty()) {
        const auto& k = node->kind;
        if (k == "function" || k == "function_definition" || k == "fnc" ||
            k == "struct" || k == "class" || k == "enum" || k == "trait" ||
            k == "type" || k == "type_alias" ||
            k == "val_declaration" || k == "var_declaration") {
            return NavigationLocation{
                file, node->location.line, node->location.column,
                node->name, classify_kind(k)};
        }
    }

    for (const auto& child : node->children) {
        if (auto result = find_definition_in_ast(child, file, symbol_name))
            return result;
    }
    return std::nullopt;
}

std::optional<NavigationLocation> NavigationProvider::go_to_definition(
    const std::filesystem::path& file,
    const std::string& symbol_name) const {
    if (symbol_name.empty()) return std::nullopt;

    // Use the symbol index for O(1) definition lookup
    auto def = model_.get_definition_location(symbol_name);
    if (def) {
        return NavigationLocation{
            def->file, def->line, def->column,
            def->name, classify_kind(def->kind)};
    }

    return std::nullopt;
}

std::optional<NavigationLocation> NavigationProvider::find_definition_in_file(
    const std::filesystem::path& file,
    const std::string& symbol_name) const {
    auto ast = model_.get_ast(file);
    return find_definition_in_ast(ast, file, symbol_name);
}

// ---------------------------------------------------------------------------
// Find references (Req 18.2)
// ---------------------------------------------------------------------------

void NavigationProvider::find_references_in_ast(
    const std::shared_ptr<ASTNode>& node,
    const std::filesystem::path& file,
    const std::string& symbol_name,
    std::vector<NavigationLocation>& results) const {
    if (!node) return;

    if (node->name == symbol_name && !node->name.empty()) {
        results.push_back(NavigationLocation{
            file, node->location.line, node->location.column,
            node->name, classify_kind(node->kind)});
    }

    for (const auto& child : node->children) {
        find_references_in_ast(child, file, symbol_name, results);
    }
}

std::vector<NavigationLocation> NavigationProvider::find_references(
    const std::string& symbol_name) const {
    std::vector<NavigationLocation> results;
    if (symbol_name.empty()) return results;

    // Use the symbol index for O(1) lookup
    auto locs = model_.get_symbol_locations(symbol_name);
    for (const auto& loc : locs) {
        results.push_back(NavigationLocation{
            loc.file, loc.line, loc.column,
            loc.name, classify_kind(loc.kind)});
    }

    // Deduplicate by (file, line, column)
    std::sort(results.begin(), results.end(), [](const NavigationLocation& a, const NavigationLocation& b) {
        if (a.file != b.file) return a.file < b.file;
        if (a.line != b.line) return a.line < b.line;
        return a.column < b.column;
    });
    results.erase(std::unique(results.begin(), results.end(), [](const NavigationLocation& a, const NavigationLocation& b) {
        return a.file == b.file && a.line == b.line && a.column == b.column;
    }), results.end());

    return results;
}

// ---------------------------------------------------------------------------
// Document symbols (Req 18.3)
// ---------------------------------------------------------------------------

void NavigationProvider::build_document_symbols(
    const std::shared_ptr<ASTNode>& node,
    std::vector<DocumentSymbol>& symbols) const {
    if (!node) return;

    // Only include declaration nodes in document symbols, not references
    static const std::unordered_set<std::string> declaration_kinds = {
        "function", "val_declaration", "var_declaration", "struct", "class",
        "enum", "trait", "type", "module", "import", "field", "parameter",
        "enum_variant"
    };

    if (!node->name.empty() && declaration_kinds.count(node->kind)) {
        DocumentSymbol sym;
        sym.name = node->name;
        sym.kind = classify_kind(node->kind);
        sym.detail = node->type_info;
        sym.line = node->location.line;
        sym.column = node->location.column;

        // Recursively collect children symbols (only declarations)
        for (const auto& child : node->children) {
            build_document_symbols(child, sym.children);
        }
        symbols.push_back(std::move(sym));
    } else {
        // Unnamed node or reference — recurse into children directly
        for (const auto& child : node->children) {
            build_document_symbols(child, symbols);
        }
    }
}

std::vector<DocumentSymbol> NavigationProvider::get_document_symbols(
    const std::filesystem::path& file) const {
    std::vector<DocumentSymbol> symbols;
    auto ast = model_.get_ast(file);
    build_document_symbols(ast, symbols);
    return symbols;
}

// ---------------------------------------------------------------------------
// Workspace symbols (Req 18.4)
// ---------------------------------------------------------------------------

std::vector<WorkspaceSymbol> NavigationProvider::get_workspace_symbols(
    const std::string& query) const {
    std::vector<WorkspaceSymbol> results;

    for (const auto& f : model_.get_indexed_files()) {
        auto ast = model_.get_ast(f);
        if (!ast) continue;

        // Collect top-level symbols and their children
        std::function<void(const std::shared_ptr<ASTNode>&)> collect;
        collect = [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            if (!node->name.empty() && matches_query(node->name, query)) {
                results.push_back(WorkspaceSymbol{
                    node->name, classify_kind(node->kind),
                    f.filename().string(), f,
                    node->location.line, node->location.column});
            }
            for (const auto& child : node->children) {
                collect(child);
            }
        };
        collect(ast);
    }
    return results;
}

// ---------------------------------------------------------------------------
// Hover (Req 18.5)
// ---------------------------------------------------------------------------

std::optional<HoverInfo> NavigationProvider::get_hover(
    const std::filesystem::path& file,
    const std::string& symbol_name) const {
    if (symbol_name.empty()) return std::nullopt;

    // Recursive search helper
    std::function<std::optional<HoverInfo>(const std::shared_ptr<ASTNode>&)> search;
    search = [&](const std::shared_ptr<ASTNode>& n) -> std::optional<HoverInfo> {
        if (!n) return std::nullopt;
        if (n->name == symbol_name && !n->name.empty()) {
            HoverInfo info;
            info.name = n->name;
            info.kind = classify_kind(n->kind);
            info.type_signature = n->type_info;
            info.effects = n->effects;

            // Build documentation from kind + type
            if (n->kind == "function_definition" || n->kind == "fnc") {
                info.documentation = "fnc " + n->name;
                if (!n->type_info.empty())
                    info.documentation += " -> " + n->type_info;
            } else if (n->kind == "struct" || n->kind == "enum" ||
                       n->kind == "trait" || n->kind == "type_alias") {
                info.documentation = n->kind + " " + n->name;
            } else {
                info.documentation = n->name;
                if (!n->type_info.empty())
                    info.documentation += ": " + n->type_info;
            }
            return info;
        }
        for (const auto& child : n->children) {
            if (auto result = search(child)) return result;
        }
        return std::nullopt;
    };

    // Search current file first
    auto ast = model_.get_ast(file);
    if (auto result = search(ast)) return result;

    // Then search other files
    for (const auto& f : model_.get_indexed_files()) {
        if (f == file) continue;
        auto other_ast = model_.get_ast(f);
        if (auto result = search(other_ast)) return result;
    }

    return std::nullopt;
}

}  // namespace meld::daemon
