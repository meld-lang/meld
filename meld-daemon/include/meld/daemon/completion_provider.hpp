#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// The syntactic context at a cursor position, used to filter completions.
enum class CompletionContext {
    General,        // Top-level or statement position
    TypeAnnotation, // After ':' in a type annotation position
    FunctionCall,   // Inside function call parentheses
    Import,         // After 'import' keyword
    MemberAccess,   // After '.' operator
    Effect          // Inside effect/handle context
};

/// A single completion item returned to the LSP client.
struct CompletionItem {
    std::string label;           // Display text
    std::string kind;            // "variable", "function", "type", "keyword", "import"
    std::string detail;          // Type signature or short description
    std::string documentation;   // Longer documentation string
    std::string insert_text;     // Text to insert (may include snippet placeholders)
    int sort_priority{0};        // Lower = higher priority
};

/// Parameter information for function signature help.
struct ParameterInfo {
    std::string name;
    std::string type;
    std::string documentation;
};

/// Signature help for a function call.
struct SignatureInfo {
    std::string label;                       // Full signature string
    std::string documentation;               // Function documentation
    std::vector<ParameterInfo> parameters;
    int active_parameter{0};                 // Currently active parameter index
};

/// Result of a completion request.
struct CompletionResult {
    std::vector<CompletionItem> items;
    bool is_incomplete{false};
};

/// Result of a signature help request.
struct SignatureHelpResult {
    std::vector<SignatureInfo> signatures;
    int active_signature{0};
};

/// Built-in Meld types available for type annotation completions.
inline const std::vector<std::string>& builtin_types() {
    static const std::vector<std::string> types = {
        "Int", "Float", "String", "Bool", "Unit", "Byte",
        "List", "Map", "Set", "Option", "Result", "Future",
        "Array", "Tuple", "Char", "Any", "Nothing"
    };
    return types;
}

/// Meld keywords that can appear as completions in general context.
inline const std::vector<std::string>& completion_keywords() {
    static const std::vector<std::string> kws = {
        "fnc", "let", "val", "var", "if", "else", "match", "return",
        "import", "struct", "enum", "trait", "impl",
        "effect", "handle", "perform", "async", "await",
        "for", "while", "true", "false", "type"
    };
    return kws;
}

/// Context-aware code completion and signature help provider.
/// Reads from the SemanticModel to provide intelligent completions.
class CompletionProvider {
public:
    explicit CompletionProvider(const SemanticModel& model);
    ~CompletionProvider() = default;

    /// Determine the completion context at a cursor position.
    CompletionContext determine_context(const std::filesystem::path& file,
                                       uint32_t line, uint32_t column,
                                       const std::string& line_text) const;

    /// Get completions for a position in a file.
    CompletionResult get_completions(const std::filesystem::path& file,
                                     uint32_t line, uint32_t column,
                                     const std::string& line_text) const;

    /// Get signature help for a function call at a position.
    SignatureHelpResult get_signature_help(const std::filesystem::path& file,
                                           uint32_t line, uint32_t column,
                                           const std::string& line_text) const;

    /// Collect all symbols visible in a file's scope from the SemanticModel.
    std::vector<CompletionItem> collect_scope_symbols(
        const std::filesystem::path& file) const;

    /// Get type annotation completions (built-in + user-defined types).
    std::vector<CompletionItem> collect_type_completions(
        const std::filesystem::path& file) const;

    /// Get import completions from indexed files.
    std::vector<CompletionItem> collect_import_completions() const;

    /// Filter completion items by prefix.
    static std::vector<CompletionItem> filter_by_prefix(
        const std::vector<CompletionItem>& items,
        const std::string& prefix);

    /// Filter completions to exclude items inappropriate for the context.
    static std::vector<CompletionItem> filter_for_context(
        const std::vector<CompletionItem>& items,
        CompletionContext context);

private:
    const SemanticModel& model_;

    /// Recursively collect symbols from an AST node.
    void collect_symbols_from_ast(const std::shared_ptr<ASTNode>& node,
                                  std::vector<CompletionItem>& items) const;

    /// Extract function signature info from an AST node.
    std::optional<SignatureInfo> extract_signature(
        const std::shared_ptr<ASTNode>& node) const;

    /// Get the word prefix at the cursor position.
    static std::string get_prefix(const std::string& line_text, uint32_t column);

    /// Count commas before cursor in a function call to determine active param.
    static int count_commas_before(const std::string& line_text, uint32_t column);
};

}  // namespace meld::daemon
