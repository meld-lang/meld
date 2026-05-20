#pragma once

#include "meld/daemon/dependency_graph.hpp"
#include "meld/daemon/file_watcher.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// Result of a cross-file reference resolution.
struct CrossFileReference {
    std::string symbol_name;
    std::filesystem::path source_file;   // File containing the reference
    std::filesystem::path target_file;   // File containing the definition
    uint32_t target_line{0};
    uint32_t target_column{0};
    bool resolved{false};
};

/// Workspace indexing statistics for performance monitoring.
struct WorkspaceStats {
    size_t total_files{0};
    size_t indexed_files{0};
    std::chrono::milliseconds last_index_duration{0};
    bool incremental{false};
};

/// LSP workspace management provider (Req 20).
/// Handles file discovery, incremental indexing, cross-file resolution,
/// and configuration change handling. Reads from the shared SemanticModel.
class WorkspaceProvider {
public:
    WorkspaceProvider(SemanticModel& model,
                      DependencyGraph& dep_graph);
    ~WorkspaceProvider() = default;

    // --- File discovery (Req 20.1) ---

    /// Discover and index all .meld files recursively under a root.
    /// Returns the set of discovered file paths.
    std::vector<std::filesystem::path> discover_and_index(
        const std::filesystem::path& workspace_root);

    // --- Incremental index updates (Req 20.2) ---

    /// Handle a file system change event and update the index incrementally.
    void handle_file_change(const std::filesystem::path& file,
                            FileChangeType change_type);

    // --- Cross-file resolution (Req 20.3) ---

    /// Resolve a cross-file reference (import/export) using the DependencyGraph.
    CrossFileReference resolve_cross_file_reference(
        const std::filesystem::path& from_file,
        const std::string& symbol_name) const;

    /// Resolve all imports in a file.
    std::vector<CrossFileReference> resolve_all_imports(
        const std::filesystem::path& file) const;

    // --- Large workspace performance (Req 20.4) ---

    /// Get workspace statistics for performance monitoring.
    WorkspaceStats get_stats() const;

    /// Check if a file operation completes within a time budget.
    /// Returns true if the operation is within the budget.
    bool is_within_performance_budget(
        std::chrono::milliseconds budget) const;

    // --- Configuration change handling (Req 20.5) ---

    /// Reload and reindex files affected by a configuration change.
    /// Returns the set of reindexed file paths.
    std::vector<std::filesystem::path> handle_config_change(
        const std::filesystem::path& config_file);

    /// Get all currently indexed files.
    std::vector<std::filesystem::path> get_indexed_files() const;

    /// Check if a file is indexed.
    bool is_indexed(const std::filesystem::path& file) const;

    /// Get the number of indexed files.
    size_t indexed_file_count() const;

private:
    SemanticModel& model_;
    DependencyGraph& dep_graph_;
    WorkspaceStats stats_;

    /// Build a FileSemantics entry for a file path (stub parse).
    FileSemantics build_semantics(const std::filesystem::path& file) const;

    /// Recursively discover .meld files under a directory.
    std::vector<std::filesystem::path> discover_meld_files(
        const std::filesystem::path& root) const;

    /// Find the definition of a symbol across all indexed files.
    std::optional<std::pair<std::filesystem::path, SourceLocation>>
    find_symbol_definition(const std::string& symbol_name) const;

    /// Determine which files are affected by a config change.
    std::vector<std::filesystem::path> find_affected_files(
        const std::filesystem::path& config_file) const;
};

}  // namespace meld::daemon
