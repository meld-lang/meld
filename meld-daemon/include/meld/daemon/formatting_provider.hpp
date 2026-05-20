#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// A text edit representing a formatting change.
struct TextEdit {
    uint32_t start_line{0};
    uint32_t start_column{0};
    uint32_t end_line{0};
    uint32_t end_column{0};
    std::string new_text;
};

/// A range within a document.
struct Range {
    uint32_t start_line{0};
    uint32_t start_column{0};
    uint32_t end_line{0};
    uint32_t end_column{0};
};

/// Result of a rename operation.
struct RenameResult {
    bool success{false};
    std::string error_message;
    /// Map of file path -> edits for that file.
    std::vector<std::pair<std::filesystem::path, std::vector<TextEdit>>> file_edits;
};

/// LSP code formatting and refactoring provider reading from the SemanticModel.
/// Implements document formatting, range formatting, and symbol renaming.
class FormattingProvider {
public:
    explicit FormattingProvider(const SemanticModel& model);
    ~FormattingProvider() = default;

    // --- Document formatting (Req 19.1) ---

    /// Format an entire document according to Meld style conventions.
    /// Returns a list of text edits to apply.
    std::vector<TextEdit> format_document(const std::filesystem::path& file) const;

    // --- Range formatting (Req 19.2) ---

    /// Format only the selected range, preserving surrounding formatting.
    std::vector<TextEdit> format_range(const std::filesystem::path& file,
                                       const Range& range) const;

    // --- Symbol renaming (Req 19.3) ---

    /// Rename a symbol across all references in the workspace.
    RenameResult rename_symbol(const std::string& old_name,
                               const std::string& new_name) const;

    // --- Rename conflict detection (Req 19.4) ---

    /// Check if renaming would cause a naming conflict.
    bool has_naming_conflict(const std::string& old_name,
                             const std::string& new_name) const;

    // --- Semantic preservation (Req 19.5) ---

    /// Format a line of Meld code according to style conventions.
    /// Preserves semantic meaning while improving readability.
    static std::string format_line(const std::string& line);

private:
    const SemanticModel& model_;

    /// Collect all references to a symbol across the workspace.
    struct SymbolReference {
        std::filesystem::path file;
        uint32_t line{0};
        uint32_t column{0};
        std::string context_kind;  // "definition" or "reference"
    };

    std::vector<SymbolReference> find_all_references(
        const std::string& symbol_name) const;

    /// Recursively find references in an AST.
    void find_refs_in_ast(const std::shared_ptr<ASTNode>& node,
                          const std::filesystem::path& file,
                          const std::string& symbol_name,
                          std::vector<SymbolReference>& results) const;

    /// Check if a name exists as a symbol in the workspace.
    bool symbol_exists(const std::string& name) const;

    /// Check if a name exists in the same scope as the old symbol.
    bool name_conflicts_in_scope(const std::string& old_name,
                                  const std::string& new_name) const;

    /// Apply Meld style conventions to a source line.
    static std::string apply_style_conventions(const std::string& line);

    /// Normalize whitespace: collapse multiple spaces, ensure spacing around operators.
    static std::string normalize_whitespace(const std::string& line);

    /// Ensure consistent indentation (spaces, not tabs).
    static std::string normalize_indentation(const std::string& line);
};

}  // namespace meld::daemon
