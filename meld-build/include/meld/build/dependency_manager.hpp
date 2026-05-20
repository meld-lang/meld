#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace meld {
namespace build {

// Semantic version representation
struct SemanticVersion {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    std::string build_metadata;
    
    SemanticVersion(int maj = 0, int min = 0, int p = 0)
        : major(maj), minor(min), patch(p) {}
    
    std::string to_string() const;
    bool is_compatible_with(const SemanticVersion& other) const;
    bool satisfies_requirement(const std::string& requirement) const;
    
    static std::optional<SemanticVersion> parse(const std::string& version_str);
    
    bool operator<(const SemanticVersion& other) const;
    bool operator==(const SemanticVersion& other) const;
};

// Dependency specification
struct Dependency {
    std::string name;
    std::string version_requirement;  // e.g., "^1.2.3", ">=2.0.0", "~1.5"
    bool optional;
    std::vector<std::string> features;
    std::string source;  // "registry", "git", "path"
    std::string source_url;
    
    Dependency(const std::string& n, const std::string& ver)
        : name(n), version_requirement(ver), optional(false), source("registry") {}
};

// Feature flag configuration
struct FeatureFlag {
    std::string name;
    std::string description;
    bool enabled_by_default;
    std::vector<std::string> dependencies;  // Other features this depends on
    std::vector<std::string> enables_dependencies;  // Package dependencies enabled by this feature
    
    FeatureFlag(const std::string& n, bool enabled = false)
        : name(n), enabled_by_default(enabled) {}
};

// Package manifest (similar to Cargo.toml)
struct PackageManifest {
    std::string name;
    SemanticVersion version;
    std::vector<std::string> authors;
    std::string description;
    std::string license;
    std::string repository;
    
    std::vector<Dependency> dependencies;
    std::vector<Dependency> dev_dependencies;
    std::vector<Dependency> build_dependencies;
    
    std::map<std::string, FeatureFlag> features;
    std::vector<std::string> default_features;
    
    // Rust-inspired feature configuration
    struct RustFeatures {
        bool ownership_checking;
        bool borrow_checking;
        bool lifetime_inference;
        bool trait_system;
        bool pattern_matching;
        
        RustFeatures() : ownership_checking(false), borrow_checking(false),
                        lifetime_inference(false), trait_system(false),
                        pattern_matching(false) {}
    };
    
    RustFeatures rust_features;
    
    static std::optional<PackageManifest> load_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;
};

// Dependency resolver
class DependencyResolver {
public:
    struct ResolvedDependency {
        std::string name;
        SemanticVersion version;
        std::string source_path;
        std::vector<std::string> enabled_features;
    };
    
    struct ResolutionResult {
        bool success;
        std::vector<ResolvedDependency> resolved;
        std::vector<std::string> errors;
        std::map<std::string, std::vector<SemanticVersion>> conflicts;
    };
    
    // Resolve dependencies
    ResolutionResult resolve(
        const PackageManifest& manifest,
        const std::vector<std::string>& enabled_features = {}
    );
    
    // Check for version conflicts
    bool has_conflicts(const std::vector<Dependency>& deps);
    
    // Find compatible version
    std::optional<SemanticVersion> find_compatible_version(
        const std::string& package_name,
        const std::string& version_requirement
    );
    
private:
    std::map<std::string, std::vector<SemanticVersion>> available_versions_;
};

// Lock file management (similar to Cargo.lock)
class LockFile {
public:
    struct LockedDependency {
        std::string name;
        SemanticVersion version;
        std::string checksum;
        std::vector<std::string> dependencies;
    };
    
    std::vector<LockedDependency> locked_dependencies;
    
    static std::optional<LockFile> load(const std::string& path);
    bool save(const std::string& path) const;
    
    // Check if lock file is up to date with manifest
    bool is_up_to_date(const PackageManifest& manifest) const;
    
    // Update lock file with new resolutions
    void update(const DependencyResolver::ResolutionResult& resolution);
};

// Feature flag manager
class FeatureFlagManager {
public:
    // Enable/disable features
    void enable_feature(const std::string& feature_name);
    void disable_feature(const std::string& feature_name);
    
    // Check if feature is enabled
    bool is_enabled(const std::string& feature_name) const;
    
    // Get all enabled features
    std::vector<std::string> get_enabled_features() const;
    
    // Resolve feature dependencies
    std::vector<std::string> resolve_feature_dependencies(
        const std::string& feature_name,
        const std::map<std::string, FeatureFlag>& all_features
    );
    
    // Validate feature configuration
    struct ValidationResult {
        bool valid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validate_features(
        const std::map<std::string, FeatureFlag>& features
    );
    
private:
    std::map<std::string, bool> enabled_features_;
};

// Build configuration
struct BuildConfig {
    std::string profile;  // "debug", "release", "test"
    std::vector<std::string> enabled_features;
    bool incremental_compilation;
    int optimization_level;  // 0-3
    bool debug_info;
    
    // Rust-inspired feature flags
    bool enable_ownership_checking;
    bool enable_borrow_checking;
    bool enable_lifetime_inference;
    
    BuildConfig() : profile("debug"), incremental_compilation(true),
                   optimization_level(0), debug_info(true),
                   enable_ownership_checking(false),
                   enable_borrow_checking(false),
                   enable_lifetime_inference(false) {}
};

// Build system integration
class BuildSystem {
public:
    // Initialize build system
    bool initialize(const std::string& project_root);
    
    // Load project configuration
    std::optional<PackageManifest> load_manifest();
    
    // Resolve and fetch dependencies
    bool resolve_dependencies(const std::vector<std::string>& features = {});
    
    // Build project
    struct BuildResult {
        bool success;
        std::vector<std::string> artifacts;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::chrono::milliseconds build_time;
    };
    
    BuildResult build(const BuildConfig& config);
    
    // Run tests
    struct TestResult {
        bool success;
        size_t tests_run;
        size_t tests_passed;
        size_t tests_failed;
        std::vector<std::string> failed_tests;
    };
    
    TestResult run_tests(const BuildConfig& config);
    
    // Generate documentation
    bool generate_docs(const std::string& output_dir);
    
    // Clean build artifacts
    void clean();
    
private:
    std::string project_root_;
    std::optional<PackageManifest> manifest_;
    std::optional<LockFile> lock_file_;
    DependencyResolver resolver_;
    FeatureFlagManager feature_manager_;
};

// Gradual adoption helper
class GradualAdoptionHelper {
public:
    // Migration strategies
    enum class MigrationStrategy {
        PerModule,      // Enable features per module
        PerFile,        // Enable features per file
        PerFunction,    // Enable features per function
        WholeProject    // Enable for entire project
    };
    
    struct MigrationPlan {
        MigrationStrategy strategy;
        std::vector<std::string> features_to_enable;
        std::vector<std::string> modules_to_migrate;
        bool generate_warnings;
        bool strict_mode;
    };
    
    // Create migration plan
    static MigrationPlan create_migration_plan(
        const PackageManifest& manifest,
        MigrationStrategy strategy
    );
    
    // Apply migration incrementally
    struct MigrationResult {
        bool success;
        size_t files_migrated;
        std::vector<std::string> errors;
        std::vector<std::string> suggestions;
    };
    
    static MigrationResult apply_migration(
        const MigrationPlan& plan,
        const std::string& project_root
    );
    
    // Generate migration report
    static std::string generate_migration_report(
        const MigrationResult& result
    );
};

} // namespace build
} // namespace meld
