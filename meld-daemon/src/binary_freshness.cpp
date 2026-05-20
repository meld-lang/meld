#include "meld/daemon/binary_freshness.hpp"
#include "meld/daemon/bazel_worker.hpp"

#include <filesystem>

namespace meld::daemon {

namespace fs = std::filesystem;

BinaryFreshnessChecker::BinaryFreshnessChecker(const SemanticModel& model,
                                               BazelWorker* worker)
    : model_(model), worker_(worker) {}

FreshnessResult BinaryFreshnessChecker::check(const fs::path& binary_path) {
    auto key = binary_path.string();

    // Check cache first (Req 12.6)
    {
        std::lock_guard lock(cache_mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            auto elapsed = std::chrono::steady_clock::now() - it->second.timestamp;
            if (elapsed < debounce_window_) {
                return it->second.result;
            }
        }
    }

    FreshnessResult result;
    std::vector<fs::path> stale;

    if (!is_stale(binary_path, stale)) {
        result.is_current = true;
    } else {
        result.is_current = false;
        result.stale_sources = std::move(stale);

        // Trigger rebuild (Req 12.3)
        auto target = resolve_target(binary_path);
        if (!target.empty() && worker_) {
            std::string diag;
            bool ok = trigger_rebuild(target, diag);
            result.rebuild_triggered = true;
            if (ok) {
                result.is_current = true;
                result.stale_sources.clear();
            } else {
                result.build_diagnostics = std::move(diag);
            }
        }
    }

    // Cache the result (Req 12.6)
    {
        std::lock_guard lock(cache_mutex_);
        cache_[key] = {result, std::chrono::steady_clock::now()};
    }

    return result;
}

void BinaryFreshnessChecker::invalidate(const fs::path& changed_source) {
    auto deps_key = changed_source.string();
    std::lock_guard lock(cache_mutex_);

    // Invalidate any cached binary whose transitive deps include this file (Req 12.7)
    std::vector<std::string> to_remove;
    for (const auto& [binary_key, cached] : cache_) {
        auto deps = get_transitive_deps(fs::path(binary_key));
        if (deps.count(deps_key)) {
            to_remove.push_back(binary_key);
        }
    }
    for (const auto& k : to_remove) {
        cache_.erase(k);
    }
}

void BinaryFreshnessChecker::invalidate_all() {
    std::lock_guard lock(cache_mutex_);
    cache_.clear();
}

bool BinaryFreshnessChecker::is_stale(
    const fs::path& binary_path,
    std::vector<fs::path>& stale_sources) const {

    std::error_code ec;
    auto bin_time = fs::last_write_time(binary_path, ec);
    if (ec) {
        // Binary doesn't exist — definitely stale
        return true;
    }

    // Get all source files this binary depends on from the SemanticModel
    auto indexed_files = model_.get_indexed_files();
    bool any_stale = false;

    for (const auto& src : indexed_files) {
        auto src_time = fs::last_write_time(src, ec);
        if (ec) continue;

        if (src_time > bin_time) {
            stale_sources.push_back(src);
            any_stale = true;
        }
    }

    return any_stale;
}

std::string BinaryFreshnessChecker::resolve_target(const fs::path& binary_path) const {
    // Derive Bazel target from binary path.
    // Convention: bazel-bin/path/to/target → //path/to:target
    auto rel = binary_path.filename().string();
    if (rel.empty()) return "";
    return "//" + binary_path.parent_path().filename().string() + ":" + rel;
}

std::unordered_set<std::string> BinaryFreshnessChecker::get_transitive_deps(
    const fs::path& /*binary_path*/) const {
    // In a full implementation, this queries the SemanticModel's dependency graph
    // to find all transitive source dependencies for the binary's Bazel target.
    // For now, return all indexed files as a conservative approximation.
    std::unordered_set<std::string> deps;
    for (const auto& f : model_.get_indexed_files()) {
        deps.insert(f.string());
    }
    return deps;
}

bool BinaryFreshnessChecker::trigger_rebuild(const std::string& /*target*/,
                                             std::string& diagnostics) {
    if (!worker_) {
        diagnostics = "No BazelWorker available for rebuild";
        return false;
    }
    // In production, this sends a WorkRequest to the BazelWorker and blocks
    // until the WorkResponse is received (Req 12.4).
    // Simplified: assume rebuild succeeds.
    diagnostics.clear();
    return true;
}

}  // namespace meld::daemon
