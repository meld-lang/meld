#pragma once

#include "meld/daemon/file_watcher.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// Incremental re-analysis pipeline.
/// Accepts a changed file path and the SemanticModel, re-parses only the changed
/// file, updates its AST node, and re-runs type checking and effect inference
/// for the changed file and its direct dependents.
class IncrementalAnalyzer {
public:
    explicit IncrementalAnalyzer(SemanticModel& model);
    ~IncrementalAnalyzer() = default;

    /// Analyze a single changed file and its dependents.
    /// Returns the set of files whose diagnostics were updated.
    std::vector<std::filesystem::path> analyze_change(const std::filesystem::path& changed_file);

    /// Analyze a file with in-memory content (for unsaved editor buffers).
    /// Returns the set of files whose diagnostics were updated.
    std::vector<std::filesystem::path> analyze_change(const std::filesystem::path& file,
                                                       const std::string& content);

    /// Analyze a batch of file changes (from debounced events).
    /// Returns the set of files whose diagnostics were updated.
    std::vector<std::filesystem::path> analyze_changes(const std::vector<FileChangeEvent>& events);

    /// Perform a full workspace analysis (initial indexing).
    /// Returns all files that were analyzed.
    std::vector<std::filesystem::path> analyze_workspace(const std::filesystem::path& root);

    /// Get the direct dependents of a file (files that import it).
    std::unordered_set<std::string> get_dependents(const std::filesystem::path& file) const;

    /// Resolve an import path (e.g. "std.async.core") to a file path.
    std::optional<std::filesystem::path> resolve_import(const std::string& import_path) const;

private:
    /// Parse a single .meld file and update the SemanticModel
    FileSemantics parse_file(const std::filesystem::path& path);

    /// Parse content for a file path (used for unsaved editor buffers)
    FileSemantics parse_content(const std::filesystem::path& path, const std::string& content);

    /// Re-run type checking for a file using current SemanticModel state
    std::vector<Diagnostic> type_check(const std::filesystem::path& path);

    /// Re-run effect inference for a file
    std::vector<Diagnostic> infer_effects(const std::filesystem::path& path);

    SemanticModel& model_;
    // Reverse dependency map: file → set of files that depend on it
    mutable std::mutex deps_mutex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> reverse_deps_;
    // Import path → file path index (e.g. "std.async.core" → "/path/to/std/async/core.meld")
    std::unordered_map<std::string, std::filesystem::path> import_index_;
};

}  // namespace meld::daemon
