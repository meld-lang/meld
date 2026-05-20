#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace meld {
namespace build {

/**
 * A dependency declared in meld.toml [dependencies] section.
 */
struct Dependency {
    std::string name;
    std::string git_url;
    std::string version_spec;  // tag, branch, or commit SHA
};

/**
 * A dependency after resolution — has a concrete commit and cache path.
 */
struct ResolvedDependency {
    std::string name;
    std::string git_url;
    std::string resolved_commit;
    std::filesystem::path cache_path;
};

/**
 * A single entry in meld.lock pinning a dependency to an exact commit.
 */
struct LockedPackage {
    std::string name;
    std::string git_url;
    std::string resolved_commit;
    std::string tag;
    std::string integrity;  // SHA-256 of fetched content
};

/**
 * Lock file model (meld.lock) for reproducible builds.
 */
struct LockFile {
    int version = 1;
    std::vector<LockedPackage> packages;

    /** Find a locked package by name, or return nullopt. */
    std::optional<LockedPackage> find(const std::string& name) const;
};

/**
 * Git-first package resolver.
 *
 * Fetches dependencies declared in meld.toml as Git repositories,
 * caches them under ~/.meld/cache/<name>/<commit>/, and manages
 * meld.lock for reproducible builds.
 */
class PackageResolver {
public:
    explicit PackageResolver(const std::filesystem::path& cache_dir = default_cache_dir());

    /**
     * Fetch all dependencies, respecting the lock file for pinned commits.
     * Clones or fetches each Git repo into the cache directory.
     * Returns the list of resolved dependencies on success.
     * Throws std::runtime_error on network error, invalid URL, or missing tag.
     */
    std::vector<ResolvedDependency> fetch(const std::vector<Dependency>& deps,
                                          const LockFile& lock);

    /**
     * Resolve latest allowed tags/commits for each dependency and
     * produce an updated lock file.
     */
    LockFile update(const std::vector<Dependency>& deps);

    /**
     * Remove all contents of the cache directory.
     */
    void clean();

    /** Default cache directory: ~/.meld/cache/ */
    static std::filesystem::path default_cache_dir();

private:
    std::filesystem::path cache_dir_;

    /**
     * Clone or fetch a single dependency into the cache.
     * Returns the resolved commit hash.
     */
    std::string fetch_dependency(const Dependency& dep,
                                 const std::optional<LockedPackage>& locked);

    /**
     * Resolve the version spec to a concrete commit hash by querying
     * the remote Git repository.
     */
    std::string resolve_version(const Dependency& dep);

    /**
     * Run a git command via std::system(). Returns the exit code.
     */
    static int run_git(const std::string& command);
};

} // namespace build
} // namespace meld
