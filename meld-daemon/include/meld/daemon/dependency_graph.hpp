#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// A single dependency node in the graph
struct DependencyNode {
    std::string name;           // Package name (e.g., "meld-lang")
    std::string version;        // Version string
    std::string source;         // "registry", "git", "local"
    std::string url;            // Source URL or path
    std::vector<std::string> transitive_deps;  // Names of transitive dependencies
};

/// Diff result when comparing dependency graphs
struct DependencyDiff {
    std::vector<std::string> added;
    std::vector<std::string> removed;
    std::vector<std::string> changed;  // Version or source changed

    bool empty() const { return added.empty() && removed.empty() && changed.empty(); }
};

/// In-memory dependency graph synchronized with meld.toml and Bazel.
class DependencyGraph {
public:
    DependencyGraph() = default;
    ~DependencyGraph() = default;

    /// Add or update a dependency node
    void upsert(DependencyNode node);

    /// Remove a dependency by name
    bool remove(const std::string& name);

    /// Get a dependency by name
    std::optional<DependencyNode> get(const std::string& name) const;

    /// Get all dependency names
    std::vector<std::string> all_names() const;

    /// Get the full transitive closure for a dependency
    std::unordered_set<std::string> transitive_closure(const std::string& name) const;

    /// Compute the diff between this graph and a new set of dependencies
    DependencyDiff diff(const std::vector<DependencyNode>& new_deps) const;

    /// Replace the entire graph with new dependencies
    void replace_all(std::vector<DependencyNode> deps);

    /// Parse dependency declarations from meld.toml content
    static std::vector<DependencyNode> parse_meld_toml(const std::string& content);

    /// Trigger Bazel sync and query to refresh the graph
    /// Returns diagnostics (empty on success)
    std::vector<Diagnostic> sync_with_bazel(const std::filesystem::path& workspace);

    /// Get the number of dependencies
    size_t size() const { return nodes_.size(); }

    /// Check if empty
    bool empty() const { return nodes_.empty(); }

private:
    std::unordered_map<std::string, DependencyNode> nodes_;
};

}  // namespace meld::daemon
