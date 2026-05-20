#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace meld::daemon {

/// Source location for diagnostics
struct SourceLocation {
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
};

/// Severity levels for diagnostics
enum class DiagnosticSeverity { Error, Warning, Info, Hint };

/// A single diagnostic message
struct Diagnostic {
    SourceLocation location;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    std::string rule_id;       // Stable identifier (e.g., "E0042-effect-leak")
    std::string ast_selector;  // JSONPath-like path to AST node
    std::string context_hash;  // Hash of surrounding code context
};

/// Minimal AST node representation for the SemanticModel.
/// In production this wraps the full meld-lang parser AST;
/// here we define the interface the daemon needs.
struct ASTNode {
    std::string kind;       // Node type (e.g., "function_definition", "val_declaration")
    std::string name;       // Symbol name if applicable
    std::string type_info;  // Inferred type string
    SourceLocation location;
    std::vector<std::string> effects;  // Effect annotations
    std::vector<std::shared_ptr<ASTNode>> children;
};

/// Per-file semantic information
struct FileSemantics {
    std::filesystem::path path;
    std::shared_ptr<ASTNode> ast;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> exports;   // Exported symbol names
    std::vector<std::string> imports;   // Import coordinates
};

/// Type information for a symbol
struct TypeInfo {
    std::string name;
    std::string qualified_type;
    std::vector<std::string> type_params;
};

/// Effect information for a statement
struct EffectInfo {
    std::vector<std::string> required_effects;
    std::vector<std::string> call_chain;  // How each effect was introduced
};

/// Ownership information for a reference
enum class OwnershipKind { Own, Link, Unknown };
enum class LifecycleState { Valid, PotentiallyDangling, Moved };

struct OwnershipInfo {
    OwnershipKind kind{OwnershipKind::Unknown};
    LifecycleState state{LifecycleState::Valid};
    std::string scope;  // Scope in which reference was created
};


/// The daemon's in-memory representation of the entire workspace.
/// Protected by a shared_mutex (AST_RWLock) for concurrent read / exclusive write.
class SemanticModel {
public:
    SemanticModel() = default;
    ~SemanticModel() = default;

    // Non-copyable, non-movable (shared state)
    SemanticModel(const SemanticModel&) = delete;
    SemanticModel& operator=(const SemanticModel&) = delete;

    // --- Write operations (exclusive lock) ---

    /// Update the AST and diagnostics for a file
    void update_file(const std::filesystem::path& path, FileSemantics semantics);

    /// Remove a file from the model
    void remove_file(const std::filesystem::path& path);

    /// Clear all files
    void clear();

    // --- Read operations (shared lock) ---

    /// Get the AST for a file, or nullptr if not indexed
    std::shared_ptr<ASTNode> get_ast(const std::filesystem::path& path) const;

    /// Get diagnostics for a specific file
    std::vector<Diagnostic> get_diagnostics(const std::filesystem::path& path) const;

    /// Get all diagnostics across the workspace
    std::vector<Diagnostic> get_all_diagnostics() const;

    /// Get all indexed file paths
    std::vector<std::filesystem::path> get_indexed_files() const;

    /// Check if a file is indexed
    bool has_file(const std::filesystem::path& path) const;

    /// Get the number of indexed files
    size_t file_count() const;

    /// Query type information for a symbol at a location
    std::optional<TypeInfo> query_type(const std::filesystem::path& path,
                                       const std::string& symbol) const;

    /// Query effect information for a statement
    std::optional<EffectInfo> query_effects(const std::filesystem::path& path,
                                             uint32_t line) const;

    /// Query ownership information for a reference
    std::optional<OwnershipInfo> query_ownership(const std::filesystem::path& path,
                                                  const std::string& symbol) const;

    /// Get the symbol name at a given line/column position in a file
    std::string get_symbol_at(const std::filesystem::path& path,
                              uint32_t line, uint32_t column) const;

    /// Format a file's content according to meld fmt conventions.
    /// Returns the formatted text, or nullopt if the file is not indexed.
    std::optional<std::string> format_file(const std::filesystem::path& path) const;

    // --- Symbol index for O(1) lookups ---

    /// A location entry in the symbol index.
    struct SymbolLocation {
        std::filesystem::path file;
        uint32_t line;
        uint32_t column;
        std::string kind;  // "function", "val_declaration", "reference", etc.
        std::string name;
    };

    /// Get all locations of a symbol (for find_references).
    std::vector<SymbolLocation> get_symbol_locations(const std::string& name) const;

    /// Get the definition location of a symbol (first non-reference match).
    std::optional<SymbolLocation> get_definition_location(const std::string& name) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, FileSemantics> files_;  // path string → semantics

    // Symbol index: name → list of locations across all files
    std::unordered_map<std::string, std::vector<SymbolLocation>> symbol_index_;
};

}  // namespace meld::daemon
