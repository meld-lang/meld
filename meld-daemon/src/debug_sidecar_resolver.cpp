#include "meld/daemon/debug_sidecar_resolver.hpp"

#include <filesystem>

namespace meld::daemon {

namespace fs = std::filesystem;

void DebugSidecarResolver::set_workspace_root(const fs::path& root) {
    workspace_root_ = root;
}

std::optional<fs::path> DebugSidecarResolver::resolve(
    const std::string& debug_id,
    const fs::path& binary_path) const {

    // Check cache first.
    {
        std::lock_guard lock(cache_mutex_);
        if (auto it = cache_.find(debug_id); it != cache_.end()) {
            if (fs::exists(it->second))
                return it->second;
            // Stale entry — remove and re-search.
            cache_.erase(it);
        }
    }

    // Search order: co-located → project cache → bazel-bin.
    std::optional<fs::path> result;

    if (!binary_path.empty())
        result = try_colocated(binary_path);

    if (!result)
        result = try_project_cache(debug_id);

    if (!result)
        result = try_bazel_bin(debug_id);

    // Cache the result if found.
    if (result) {
        std::lock_guard lock(cache_mutex_);
        cache_[debug_id] = *result;
    }

    return result;
}

void DebugSidecarResolver::invalidate() {
    std::lock_guard lock(cache_mutex_);
    cache_.clear();
}

void DebugSidecarResolver::invalidate(const std::string& debug_id) {
    std::lock_guard lock(cache_mutex_);
    cache_.erase(debug_id);
}

size_t DebugSidecarResolver::cache_size() const {
    std::lock_guard lock(cache_mutex_);
    return cache_.size();
}

std::optional<fs::path> DebugSidecarResolver::try_colocated(
    const fs::path& binary_path) const {
    auto mdebug = binary_path;
    mdebug.replace_extension(".mdebug");
    if (fs::exists(mdebug))
        return mdebug;
    return std::nullopt;
}

std::optional<fs::path> DebugSidecarResolver::try_project_cache(
    const std::string& debug_id) const {
    if (workspace_root_.empty()) return std::nullopt;
    auto candidate = workspace_root_ / ".meld" / "debug" / (debug_id + ".mdebug");
    if (fs::exists(candidate))
        return candidate;
    return std::nullopt;
}

std::optional<fs::path> DebugSidecarResolver::try_bazel_bin(
    const std::string& debug_id) const {
    if (workspace_root_.empty()) return std::nullopt;
    auto bazel_bin = workspace_root_ / "bazel-bin";
    if (!fs::exists(bazel_bin) || !fs::is_directory(bazel_bin))
        return std::nullopt;

    // Walk bazel-bin looking for <debug_id>.mdebug.
    // In production this would use an index; for now, shallow recursive search.
    std::error_code ec;
    std::string target_name = debug_id + ".mdebug";
    for (auto& entry : fs::recursive_directory_iterator(bazel_bin, ec)) {
        if (entry.is_regular_file() && entry.path().filename() == target_name)
            return entry.path();
    }
    return std::nullopt;
}

}  // namespace meld::daemon
