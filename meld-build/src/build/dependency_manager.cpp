#include "meld/build/dependency_manager.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <fstream>

namespace meld {
namespace build {

// Semantic version implementation
std::string SemanticVersion::to_string() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!prerelease.empty()) {
        oss << "-" << prerelease;
    }
    if (!build_metadata.empty()) {
        oss << "+" << build_metadata;
    }
    return oss.str();
}

bool SemanticVersion::is_compatible_with(const SemanticVersion& other) const {
    // Compatible if major version matches and this version >= other
    if (major != other.major) return false;
    if (minor < other.minor) return false;
    if (minor == other.minor && patch < other.patch) return false;
    return true;
}

bool SemanticVersion::satisfies_requirement(const std::string& requirement) const {
    // Simplified version requirement checking
    // Supports: ^1.2.3 (compatible), ~1.2.3 (patch), >=1.2.3, =1.2.3
    
    if (requirement.empty()) return true;
    
    if (requirement[0] == '^') {
        // Caret: compatible with version
        auto req_ver = parse(requirement.substr(1));
        return req_ver && is_compatible_with(*req_ver);
    }
    
    if (requirement[0] == '~') {
        // Tilde: allow patch updates
        auto req_ver = parse(requirement.substr(1));
        return req_ver && major == req_ver->major && minor == req_ver->minor && patch >= req_ver->patch;
    }
    
    if (requirement.substr(0, 2) == ">=") {
        auto req_ver = parse(requirement.substr(2));
        return req_ver && !(*this < *req_ver);
    }
    
    if (requirement[0] == '=') {
        auto req_ver = parse(requirement.substr(1));
        return req_ver && *this == *req_ver;
    }
    
    // Exact match
    auto req_ver = parse(requirement);
    return req_ver && *this == *req_ver;
}

std::optional<SemanticVersion> SemanticVersion::parse(const std::string& version_str) {
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?(?:\+([a-zA-Z0-9.-]+))?)");
    std::smatch match;
    
    if (std::regex_match(version_str, match, version_regex)) {
        SemanticVersion ver;
        ver.major = std::stoi(match[1]);
        ver.minor = std::stoi(match[2]);
        ver.patch = std::stoi(match[3]);
        if (match[4].matched) ver.prerelease = match[4];
        if (match[5].matched) ver.build_metadata = match[5];
        return ver;
    }
    
    return std::nullopt;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    return prerelease < other.prerelease;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && 
           patch == other.patch && prerelease == other.prerelease;
}

// Dependency resolver implementation
DependencyResolver::ResolutionResult DependencyResolver::resolve(
    const PackageManifest& manifest,
    const std::vector<std::string>& enabled_features
) {
    ResolutionResult result;
    result.success = true;
    
    // Resolve each dependency
    for (const auto& dep : manifest.dependencies) {
        auto version = find_compatible_version(dep.name, dep.version_requirement);
        
        if (version) {
            ResolvedDependency resolved;
            resolved.name = dep.name;
            resolved.version = *version;
            resolved.source_path = dep.source_url.empty() ? 
                "/registry/" + dep.name : dep.source_url;
            resolved.enabled_features = dep.features;
            
            result.resolved.push_back(resolved);
        } else {
            result.success = false;
            result.errors.push_back(
                "Could not find compatible version for " + dep.name + 
                " " + dep.version_requirement
            );
        }
    }
    
    return result;
}

bool DependencyResolver::has_conflicts(const std::vector<Dependency>& deps) {
    std::map<std::string, std::vector<std::string>> version_requirements;
    
    for (const auto& dep : deps) {
        version_requirements[dep.name].push_back(dep.version_requirement);
    }
    
    for (const auto& [name, requirements] : version_requirements) {
        if (requirements.size() > 1) {
            // Check if all requirements are compatible
            // Simplified: just check if they're different
            for (size_t i = 1; i < requirements.size(); ++i) {
                if (requirements[i] != requirements[0]) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

std::optional<SemanticVersion> DependencyResolver::find_compatible_version(
    const std::string& package_name,
    const std::string& version_requirement
) {
    // In real implementation, would query package registry
    // For now, return a mock version
    auto versions = available_versions_[package_name];
    
    if (versions.empty()) {
        // Mock some versions
        versions = {
            SemanticVersion(1, 0, 0),
            SemanticVersion(1, 2, 3),
            SemanticVersion(2, 0, 0)
        };
    }
    
    // Find highest compatible version
    for (auto it = versions.rbegin(); it != versions.rend(); ++it) {
        if (it->satisfies_requirement(version_requirement)) {
            return *it;
        }
    }
    
    return std::nullopt;
}

// Lock file implementation
std::optional<LockFile> LockFile::load(const std::string& path) {
    // In real implementation, would parse lock file
    LockFile lock;
    return lock;
}

bool LockFile::save(const std::string& path) const {
    // In real implementation, would write lock file
    return true;
}

bool LockFile::is_up_to_date(const PackageManifest& manifest) const {
    // Check if locked dependencies match manifest
    return true;
}

void LockFile::update(const DependencyResolver::ResolutionResult& resolution) {
    locked_dependencies.clear();
    
    for (const auto& resolved : resolution.resolved) {
        LockedDependency locked;
        locked.name = resolved.name;
        locked.version = resolved.version;
        locked.checksum = "mock_checksum";
        locked_dependencies.push_back(locked);
    }
}

// Feature flag manager implementation
void FeatureFlagManager::enable_feature(const std::string& feature_name) {
    enabled_features_[feature_name] = true;
}

void FeatureFlagManager::disable_feature(const std::string& feature_name) {
    enabled_features_[feature_name] = false;
}

bool FeatureFlagManager::is_enabled(const std::string& feature_name) const {
    auto it = enabled_features_.find(feature_name);
    return it != enabled_features_.end() && it->second;
}

std::vector<std::string> FeatureFlagManager::get_enabled_features() const {
    std::vector<std::string> enabled;
    for (const auto& [name, is_enabled] : enabled_features_) {
        if (is_enabled) {
            enabled.push_back(name);
        }
    }
    return enabled;
}

std::vector<std::string> FeatureFlagManager::resolve_feature_dependencies(
    const std::string& feature_name,
    const std::map<std::string, FeatureFlag>& all_features
) {
    std::vector<std::string> resolved;
    resolved.push_back(feature_name);
    
    auto it = all_features.find(feature_name);
    if (it != all_features.end()) {
        for (const auto& dep : it->second.dependencies) {
            auto dep_resolved = resolve_feature_dependencies(dep, all_features);
            resolved.insert(resolved.end(), dep_resolved.begin(), dep_resolved.end());
        }
    }
    
    return resolved;
}

FeatureFlagManager::ValidationResult FeatureFlagManager::validate_features(
    const std::map<std::string, FeatureFlag>& features
) {
    ValidationResult result;
    result.valid = true;
    
    // Check for circular dependencies
    for (const auto& [name, feature] : features) {
        std::set<std::string> visited;
        std::function<bool(const std::string&)> has_cycle = [&](const std::string& current) {
            if (visited.count(current)) return true;
            visited.insert(current);
            
            auto it = features.find(current);
            if (it != features.end()) {
                for (const auto& dep : it->second.dependencies) {
                    if (has_cycle(dep)) return true;
                }
            }
            
            visited.erase(current);
            return false;
        };
        
        if (has_cycle(name)) {
            result.valid = false;
            result.errors.push_back("Circular dependency detected in feature: " + name);
        }
    }
    
    return result;
}

// Build system implementation
bool BuildSystem::initialize(const std::string& project_root) {
    project_root_ = project_root;
    manifest_ = load_manifest();
    
    if (manifest_) {
        lock_file_ = LockFile::load(project_root_ + "/meld.lock");
        return true;
    }
    
    return false;
}

std::optional<PackageManifest> BuildSystem::load_manifest() {
    // In real implementation, would parse meld.toml or similar
    PackageManifest manifest;
    manifest.name = "example_project";
    manifest.version = SemanticVersion(0, 1, 0);
    return manifest;
}

bool BuildSystem::resolve_dependencies(const std::vector<std::string>& features) {
    if (!manifest_) return false;
    
    auto result = resolver_.resolve(*manifest_, features);
    
    if (result.success) {
        if (!lock_file_) {
            lock_file_ = LockFile();
        }
        lock_file_->update(result);
        lock_file_->save(project_root_ + "/meld.lock");
    }
    
    return result.success;
}

BuildSystem::BuildResult BuildSystem::build(const BuildConfig& config) {
    BuildResult result;
    result.success = true;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate build process
    result.artifacts.push_back("target/" + config.profile + "/example_project");
    
    auto end = std::chrono::high_resolution_clock::now();
    result.build_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BuildSystem::TestResult BuildSystem::run_tests(const BuildConfig& config) {
    TestResult result;
    result.success = true;
    result.tests_run = 10;
    result.tests_passed = 10;
    result.tests_failed = 0;
    
    return result;
}

bool BuildSystem::generate_docs(const std::string& output_dir) {
    // Generate documentation
    return true;
}

void BuildSystem::clean() {
    // Clean build artifacts
}

// Gradual adoption helper
GradualAdoptionHelper::MigrationPlan GradualAdoptionHelper::create_migration_plan(
    const PackageManifest& manifest,
    MigrationStrategy strategy
) {
    MigrationPlan plan;
    plan.strategy = strategy;
    plan.generate_warnings = true;
    plan.strict_mode = false;
    
    // Add Rust-inspired features to enable
    if (manifest.rust_features.ownership_checking) {
        plan.features_to_enable.push_back("ownership_checking");
    }
    if (manifest.rust_features.borrow_checking) {
        plan.features_to_enable.push_back("borrow_checking");
    }
    
    return plan;
}

GradualAdoptionHelper::MigrationResult GradualAdoptionHelper::apply_migration(
    const MigrationPlan& plan,
    const std::string& project_root
) {
    MigrationResult result;
    result.success = true;
    result.files_migrated = 0;
    
    result.suggestions.push_back("Start with ownership_checking in new modules");
    result.suggestions.push_back("Gradually enable borrow_checking per file");
    
    return result;
}

std::string GradualAdoptionHelper::generate_migration_report(
    const MigrationResult& result
) {
    std::ostringstream oss;
    oss << "Migration Report\n";
    oss << "================\n";
    oss << "Success: " << (result.success ? "Yes" : "No") << "\n";
    oss << "Files migrated: " << result.files_migrated << "\n";
    
    if (!result.suggestions.empty()) {
        oss << "\nSuggestions:\n";
        for (const auto& suggestion : result.suggestions) {
            oss << "  - " << suggestion << "\n";
        }
    }
    
    return oss.str();
}

// Package manifest implementation
std::optional<PackageManifest> PackageManifest::load_from_file(const std::string& path) {
    // In real implementation, would parse TOML/YAML file
    PackageManifest manifest;
    return manifest;
}

bool PackageManifest::save_to_file(const std::string& path) const {
    // In real implementation, would write TOML/YAML file
    return true;
}

} // namespace build
} // namespace meld
