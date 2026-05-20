#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace meld::daemon {

/// Resolves .mdebug sidecar files by debug_id.
///
/// Search order:
///   1. Co-located with the binary (<binary>.mdebug)
///   2. Project debug cache (.meld/debug/<debug_id>.mdebug)
///   3. Bazel output tree (bazel-bin/**/<debug_id>.mdebug)
///
/// Results are cached; call invalidate() when binaries are rebuilt.
class DebugSidecarResolver {
public:
    DebugSidecarResolver() = default;
    ~DebugSidecarResolver() = default;

    /// Set the workspace root for .meld/debug/ and bazel-bin/ searches.
    void set_workspace_root(const std::filesystem::path& root);

    /// Resolve a .mdebug file by debug_id.
    /// Returns the path to the sidecar, or nullopt if not found.
    std::optional<std::filesystem::path> resolve(
        const std::string& debug_id,
        const std::filesystem::path& binary_path = {}) const;

    /// Invalidate the cache (e.g., after a rebuild).
    void invalidate();

    /// Invalidate a single entry.
    void invalidate(const std::string& debug_id);

    /// Number of cached entries (for diagnostics).
    size_t cache_size() const;

private:
    /// Try co-located: <binary_path>.mdebug
    std::optional<std::filesystem::path> try_colocated(
        const std::filesystem::path& binary_path) const;

    /// Try project debug cache: <workspace>/.meld/debug/<debug_id>.mdebug
    std::optional<std::filesystem::path> try_project_cache(
        const std::string& debug_id) const;

    /// Try bazel output tree: <workspace>/bazel-bin/**/<debug_id>.mdebug
    std::optional<std::filesystem::path> try_bazel_bin(
        const std::string& debug_id) const;

    std::filesystem::path workspace_root_;
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, std::filesystem::path> cache_;
};

}  // namespace meld::daemon
