#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// A source location result for navigation operations.
struct NavigationLocation {
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
    std::string name;       // Symbol name at this location
    std::string kind;       // "function", "variable", "type", etc.
};

/// A hierarchical document symbol for outline views.
struct DocumentSymbol {
    std::string name;
    std::string kind;       // "function", "variable", "type", "struct", "enum", "trait"
    std::string detail;     // Type info or signature
    uint32_t line{0};
    uint32_t column{0};
    std::vector<DocumentSymbol> children;
};

/// A flat workspace symbol for search.
struct WorkspaceSymbol {
    std::string name;
    std::string kind;
    std::string container;  // File or module containing the symbol
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
};

/// Hover information for a symbol.
struct HoverInfo {
    std::string name;
    std::string type_signature;   // Full type signature
    std::string documentation;    // Documentation string
    std::string kind;             // "function", "variable", "type", etc.
    std::vector<std::string> effects;  // Effect annotations if any
};

/// LSP code navigation provider reading from the SemanticModel.
/// Implements go-to-definition, find-references, document/workspace symbols, and hover.
class NavigationProvider {
public:
    explicit NavigationProvider(const SemanticModel& model);
    ~NavigationProvider() = default;

    // --- Go to definition (Req 18.1) ---

    /// Navigate to the declaration of a symbol at the given position.
    std::optional<NavigationLocation> go_to_definition(
        const std::filesystem::path& file,
        const std::string& symbol_name) const;

    /// Search a specific file's AST for a definition.
    std::optional<NavigationLocation> find_definition_in_file(
        const std::filesystem::path& file,
        const std::string& symbol_name) const;

    // --- Find references (Req 18.2) ---

    /// Find all usages of a symbol across the workspace.
    std::vector<NavigationLocation> find_references(
        const std::string& symbol_name) const;

    // --- Document symbols (Req 18.3) ---

    /// Provide a hierarchical outline of symbols in a file.
    std::vector<DocumentSymbol> get_document_symbols(
        const std::filesystem::path& file) const;

    // --- Workspace symbols (Req 18.4) ---

    /// Search for symbols across the entire workspace matching a query.
    std::vector<WorkspaceSymbol> get_workspace_symbols(
        const std::string& query) const;

    // --- Hover (Req 18.5) ---

    /// Get hover information for a symbol.
    std::optional<HoverInfo> get_hover(
        const std::filesystem::path& file,
        const std::string& symbol_name) const;

private:
    const SemanticModel& model_;

    /// Recursively find a symbol definition in an AST.
    std::optional<NavigationLocation> find_definition_in_ast(
        const std::shared_ptr<ASTNode>& node,
        const std::filesystem::path& file,
        const std::string& symbol_name) const;

    /// Recursively find all references to a symbol in an AST.
    void find_references_in_ast(
        const std::shared_ptr<ASTNode>& node,
        const std::filesystem::path& file,
        const std::string& symbol_name,
        std::vector<NavigationLocation>& results) const;

    /// Recursively build document symbols from an AST node.
    void build_document_symbols(
        const std::shared_ptr<ASTNode>& node,
        std::vector<DocumentSymbol>& symbols) const;

    /// Map AST node kind to a navigation kind string.
    static std::string classify_kind(const std::string& ast_kind);

    /// Check if a symbol name matches a query (case-insensitive substring).
    static bool matches_query(const std::string& name, const std::string& query);
};

}  // namespace meld::daemon
