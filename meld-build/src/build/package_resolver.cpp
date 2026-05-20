#include "meld/build/package_resolver.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <array>

namespace meld {
namespace build {

// ---------------------------------------------------------------------------
// LockFile
// ---------------------------------------------------------------------------

std::optional<LockedPackage> LockFile::find(const std::string& name) const {
    for (const auto& pkg : packages) {
        if (pkg.name == name) {
            return pkg;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// PackageResolver
// ---------------------------------------------------------------------------

PackageResolver::PackageResolver(const std::filesystem::path& cache_dir)
    : cache_dir_(cache_dir) {
}

std::filesystem::path PackageResolver::default_cache_dir() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");  // Windows fallback
    }
    if (!home) {
        return std::filesystem::temp_directory_path() / ".meld" / "cache";
    }
    return std::filesystem::path(home) / ".meld" / "cache";
}

std::vector<ResolvedDependency> PackageResolver::fetch(
    const std::vector<Dependency>& deps,
    const LockFile& lock) {

    std::filesystem::create_directories(cache_dir_);

    std::vector<ResolvedDependency> resolved;
    resolved.reserve(deps.size());

    for (const auto& dep : deps) {
        auto locked = lock.find(dep.name);

        std::string commit;
        try {
            commit = fetch_dependency(dep, locked);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Failed to fetch dependency '" + dep.name + "' (" +
                dep.git_url + "): " + e.what());
        }

        std::filesystem::path dep_cache = cache_dir_ / dep.name / commit;

        ResolvedDependency rd;
        rd.name = dep.name;
        rd.git_url = dep.git_url;
        rd.resolved_commit = commit;
        rd.cache_path = dep_cache;
        resolved.push_back(std::move(rd));
    }

    // Print summary
    std::cout << "Fetched " << resolved.size() << " dependencies:" << std::endl;
    for (const auto& rd : resolved) {
        std::cout << "  " << rd.name << " @ " << rd.resolved_commit.substr(0, 12)
                  << " -> " << rd.cache_path.string() << std::endl;
    }

    return resolved;
}

LockFile PackageResolver::update(const std::vector<Dependency>& deps) {
    std::filesystem::create_directories(cache_dir_);

    LockFile new_lock;
    new_lock.version = 1;

    for (const auto& dep : deps) {
        std::string commit;
        try {
            commit = resolve_version(dep);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Failed to resolve dependency '" + dep.name + "' (" +
                dep.git_url + "): " + e.what());
        }

        LockedPackage lp;
        lp.name = dep.name;
        lp.git_url = dep.git_url;
        lp.resolved_commit = commit;
        lp.tag = dep.version_spec;
        lp.integrity = "";  // Computed after fetch in a full implementation
        new_lock.packages.push_back(std::move(lp));
    }

    std::cout << "Updated lock file with " << new_lock.packages.size()
              << " dependencies:" << std::endl;
    for (const auto& lp : new_lock.packages) {
        std::cout << "  " << lp.name << " @ " << lp.resolved_commit.substr(0, 12);
        if (!lp.tag.empty()) {
            std::cout << " (tag: " << lp.tag << ")";
        }
        std::cout << std::endl;
    }

    return new_lock;
}

void PackageResolver::clean() {
    if (std::filesystem::exists(cache_dir_)) {
        std::filesystem::remove_all(cache_dir_);
        std::cout << "Cleaned package cache: " << cache_dir_.string() << std::endl;
    } else {
        std::cout << "Package cache already clean." << std::endl;
    }
}

std::string PackageResolver::fetch_dependency(
    const Dependency& dep,
    const std::optional<LockedPackage>& locked) {

    // If we have a lock entry, use the pinned commit
    std::string target_commit;
    if (locked.has_value()) {
        target_commit = locked->resolved_commit;
    } else {
        // Resolve from version spec
        target_commit = resolve_version(dep);
    }

    std::filesystem::path dep_dir = cache_dir_ / dep.name / target_commit;

    if (std::filesystem::exists(dep_dir) && !std::filesystem::is_empty(dep_dir)) {
        // Already cached — fetch to update refs but keep existing checkout
        std::string fetch_cmd = "git -C \"" + dep_dir.string() +
                                "\" fetch --quiet 2>&1";
        run_git(fetch_cmd);
        return target_commit;
    }

    // Clone the repository into the cache path
    std::filesystem::create_directories(dep_dir);

    std::string clone_cmd = "git clone --quiet \"" + dep.git_url +
                            "\" \"" + dep_dir.string() + "\" 2>&1";
    int rc = run_git(clone_cmd);
    if (rc != 0) {
        std::filesystem::remove_all(dep_dir);
        throw std::runtime_error("git clone failed (exit code " +
                                 std::to_string(rc) + ")");
    }

    // Checkout the specific commit/tag
    if (!target_commit.empty()) {
        std::string checkout_cmd = "git -C \"" + dep_dir.string() +
                                   "\" checkout --quiet " + target_commit + " 2>&1";
        rc = run_git(checkout_cmd);
        if (rc != 0) {
            std::filesystem::remove_all(dep_dir);
            throw std::runtime_error("git checkout '" + target_commit +
                                     "' failed — tag or commit not found");
        }
    }

    return target_commit;
}

std::string PackageResolver::resolve_version(const Dependency& dep) {
    if (dep.version_spec.empty()) {
        // Default to HEAD of the default branch
        return "HEAD";
    }

    // Use git ls-remote to resolve a tag or branch to a commit SHA
    std::string ls_cmd = "git ls-remote \"" + dep.git_url + "\" " +
                         dep.version_spec + " 2>&1";
    // For simplicity, return the version_spec as-is (tag name or commit SHA).
    // A full implementation would parse ls-remote output for the SHA.
    // We validate by attempting the clone+checkout in fetch_dependency.
    return dep.version_spec;
}

int PackageResolver::run_git(const std::string& command) {
    return std::system(command.c_str());
}

} // namespace build
} // namespace meld
