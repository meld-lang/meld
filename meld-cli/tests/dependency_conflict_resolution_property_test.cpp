#include <gtest/gtest.h>
#include "meld/cli/package_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <filesystem>
#include <random>
#include <set>
#include <map>

/**
 * Feature: meld-cli, Property 30: Dependency Conflict Resolution
 * Validates: Requirements 9.4
 * 
 * Property: For any set of package dependencies with version conflicts, the CLI should 
 * resolve conflicts using a consistent algorithm
 */

namespace meld::cli::test {

class DependencyConflictResolutionPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        package_module_ = std::make_unique<PackageModule>(error_handler_);
    }
    
    std::shared_ptr<ErrorHandler> error_handler_;
    std::unique_ptr<PackageModule> package_module_;
};

// Generator for package dependencies
class DependencyGenerator {
public:
    static PackageDependency generate(std::mt19937& rng, const std::string& name = "") {
        PackageDependency dep;
        
        if (name.empty()) {
            // Generate random package name
            std::vector<std::string> package_names = {
                "json-lib", "http-client", "test-utils", "crypto-lib", "string-utils",
                "math-lib", "network-tools", "file-io", "async-lib", "cache-lib"
            };
            std::uniform_int_distribution<size_t> name_dist(0, package_names.size() - 1);
            dep.name = package_names[name_dist(rng)];
        } else {
            dep.name = name;
        }
        
        // Generate version constraint
        std::uniform_int_distribution<int> version_dist(1, 10);
        int major = version_dist(rng);
        int minor = version_dist(rng);
        int patch = version_dist(rng);
        
        std::vector<std::string> constraint_types = {"", ">=", "~", "^"};
        std::uniform_int_distribution<size_t> constraint_dist(0, constraint_types.size() - 1);
        
        std::string constraint_prefix = constraint_types[constraint_dist(rng)];
        dep.version_constraint = constraint_prefix + std::to_string(major) + "." + 
                                std::to_string(minor) + "." + std::to_string(patch);
        
        // Random development dependency flag
        std::uniform_int_distribution<int> bool_dist(0, 1);
        dep.is_dev_dependency = bool_dist(rng) == 1;
        
        return dep;
    }
    
    static std::vector<PackageDependency> generate_conflicting_set(std::mt19937& rng) {
        std::vector<PackageDependency> deps;
        
        // Generate dependencies with same name but different version constraints
        std::string package_name = "conflicting-package";
        
        std::uniform_int_distribution<int> num_conflicts_dist(2, 5);
        int num_conflicts = num_conflicts_dist(rng);
        
        for (int i = 0; i < num_conflicts; ++i) {
            auto dep = generate(rng, package_name);
            // Ensure different version constraints
            dep.version_constraint = std::to_string(i + 1) + ".0.0";
            deps.push_back(dep);
        }
        
        return deps;
    }
};

// Helper function to check if dependencies have conflicts
bool has_conflicts(const std::vector<PackageDependency>& dependencies) {
    std::map<std::string, std::set<std::string>> package_versions;
    
    for (const auto& dep : dependencies) {
        package_versions[dep.name].insert(dep.version_constraint);
    }
    
    for (const auto& pair : package_versions) {
        if (pair.second.size() > 1) {
            return true;
        }
    }
    
    return false;
}

// Property test: Conflict detection accuracy
TEST_F(DependencyConflictResolutionPropertyTest, ConflictDetectionAccuracy) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& rng) {
        // Generate dependencies with potential conflicts
        std::vector<PackageDependency> dependencies;
        
        std::uniform_int_distribution<int> num_deps_dist(2, 10);
        int num_deps = num_deps_dist(rng);
        
        for (int i = 0; i < num_deps; ++i) {
            dependencies.push_back(DependencyGenerator::generate(rng));
        }
        
        // Add some intentional conflicts
        std::uniform_int_distribution<int> add_conflict_dist(0, 1);
        if (add_conflict_dist(rng) == 1) {
            auto conflicting_deps = DependencyGenerator::generate_conflicting_set(rng);
            dependencies.insert(dependencies.end(), conflicting_deps.begin(), conflicting_deps.end());
        }
        
        // Detect conflicts
        auto detected_conflicts = package_module_->detect_conflicts(dependencies);
        bool has_actual_conflicts = has_conflicts(dependencies);
        
        // Property: Conflict detection should be accurate
        if (has_actual_conflicts) {
            EXPECT_FALSE(detected_conflicts.empty()) 
                << "Should detect conflicts when they exist";
        }
        
        // Property: All detected conflicts should be real conflicts
        for (const auto& conflict : detected_conflicts) {
            EXPECT_GE(conflict.conflicting_versions.size(), 2u) 
                << "Conflict should involve at least 2 versions";
            
            EXPECT_FALSE(conflict.package_name.empty()) 
                << "Conflict should specify package name";
            
            // Verify the conflict actually exists in the input
            std::set<std::string> versions_for_package;
            for (const auto& dep : dependencies) {
                if (dep.name == conflict.package_name) {
                    versions_for_package.insert(dep.version_constraint);
                }
            }
            
            EXPECT_GE(versions_for_package.size(), 2u) 
                << "Detected conflict should correspond to actual version differences";
        }
        
        return true;
    });
}

// Property test: Conflict resolution consistency
TEST_F(DependencyConflictResolutionPropertyTest, ConflictResolutionConsistency) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& rng) {
        // Generate conflicting dependencies
        auto conflicting_deps = DependencyGenerator::generate_conflicting_set(rng);
        
        // Detect conflicts
        auto conflicts = package_module_->detect_conflicts(conflicting_deps);
        
        if (!conflicts.empty()) {
            // Resolve conflicts
            auto resolved_deps = package_module_->resolve_conflicts(conflicts);
            
            // Property: Resolution should provide exactly one dependency per conflicted package
            std::map<std::string, int> resolved_counts;
            for (const auto& dep : resolved_deps) {
                resolved_counts[dep.name]++;
            }
            
            for (const auto& conflict : conflicts) {
                EXPECT_EQ(resolved_counts[conflict.package_name], 1) 
                    << "Should resolve to exactly one version per conflicted package";
            }
            
            // Property: Resolved version should be one of the conflicting versions
            for (const auto& resolved_dep : resolved_deps) {
                bool found_in_conflicts = false;
                
                for (const auto& conflict : conflicts) {
                    if (conflict.package_name == resolved_dep.name) {
                        for (const auto& version : conflict.conflicting_versions) {
                            if (version == resolved_dep.version_constraint) {
                                found_in_conflicts = true;
                                break;
                            }
                        }
                        break;
                    }
                }
                
                EXPECT_TRUE(found_in_conflicts) 
                    << "Resolved version should be one of the conflicting versions";
            }
            
            // Property: Resolution should be deterministic (same input -> same output)
            auto resolved_deps2 = package_module_->resolve_conflicts(conflicts);
            
            EXPECT_EQ(resolved_deps.size(), resolved_deps2.size()) 
                << "Resolution should be deterministic";
            
            for (size_t i = 0; i < resolved_deps.size() && i < resolved_deps2.size(); ++i) {
                EXPECT_EQ(resolved_deps[i].name, resolved_deps2[i].name) 
                    << "Resolution should be deterministic";
                EXPECT_EQ(resolved_deps[i].version_constraint, resolved_deps2[i].version_constraint) 
                    << "Resolution should be deterministic";
            }
        }
        
        return true;
    });
}

// Property test: No conflicts after resolution
TEST_F(DependencyConflictResolutionPropertyTest, NoConflictsAfterResolution) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& rng) {
        // Generate dependencies with conflicts
        std::vector<PackageDependency> dependencies;
        
        // Add some regular dependencies
        std::uniform_int_distribution<int> num_regular_dist(1, 5);
        int num_regular = num_regular_dist(rng);
        
        for (int i = 0; i < num_regular; ++i) {
            dependencies.push_back(DependencyGenerator::generate(rng));
        }
        
        // Add conflicting dependencies
        auto conflicting_deps = DependencyGenerator::generate_conflicting_set(rng);
        dependencies.insert(dependencies.end(), conflicting_deps.begin(), conflicting_deps.end());
        
        // Resolve all dependencies
        auto resolved_deps = package_module_->resolve_dependencies(dependencies);
        
        // Property: Resolved dependencies should have no conflicts
        auto remaining_conflicts = package_module_->detect_conflicts(resolved_deps);
        
        EXPECT_TRUE(remaining_conflicts.empty()) 
            << "No conflicts should remain after resolution";
        
        // Property: Each package should appear at most once in resolved dependencies
        std::map<std::string, int> package_counts;
        for (const auto& dep : resolved_deps) {
            package_counts[dep.name]++;
        }
        
        for (const auto& pair : package_counts) {
            EXPECT_EQ(pair.second, 1) 
                << "Each package should appear exactly once in resolved dependencies";
        }
        
        return true;
    });
}

// Property test: Resolution strategy consistency
TEST_F(DependencyConflictResolutionPropertyTest, ResolutionStrategyConsistency) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(30, [this](std::mt19937& rng) {
        // Create multiple conflict scenarios with same package
        std::string package_name = "test-package";
        
        // Scenario 1: versions 1.0.0, 2.0.0, 3.0.0
        std::vector<PackageDependency> deps1;
        for (int i = 1; i <= 3; ++i) {
            PackageDependency dep;
            dep.name = package_name;
            dep.version_constraint = std::to_string(i) + ".0.0";
            deps1.push_back(dep);
        }
        
        // Scenario 2: versions 1.0.0, 2.0.0, 3.0.0 (same as scenario 1)
        std::vector<PackageDependency> deps2 = deps1;
        
        // Resolve both scenarios
        auto conflicts1 = package_module_->detect_conflicts(deps1);
        auto conflicts2 = package_module_->detect_conflicts(deps2);
        
        if (!conflicts1.empty() && !conflicts2.empty()) {
            auto resolved1 = package_module_->resolve_conflicts(conflicts1);
            auto resolved2 = package_module_->resolve_conflicts(conflicts2);
            
            // Property: Same conflict scenario should resolve to same result
            EXPECT_EQ(resolved1.size(), resolved2.size()) 
                << "Same conflict should resolve consistently";
            
            if (resolved1.size() == resolved2.size()) {
                for (size_t i = 0; i < resolved1.size(); ++i) {
                    EXPECT_EQ(resolved1[i].name, resolved2[i].name) 
                        << "Same conflict should resolve to same package";
                    EXPECT_EQ(resolved1[i].version_constraint, resolved2[i].version_constraint) 
                        << "Same conflict should resolve to same version";
                }
            }
        }
        
        return true;
    });
}

// Property test: Complex dependency graph resolution
TEST_F(DependencyConflictResolutionPropertyTest, ComplexDependencyGraphResolution) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(20, [this](std::mt19937& rng) {
        // Create a complex dependency graph with multiple conflicts
        std::vector<PackageDependency> dependencies;
        
        // Package A with multiple versions
        std::vector<std::string> versions_a = {"1.0.0", "1.1.0", "2.0.0"};
        for (const auto& version : versions_a) {
            PackageDependency dep;
            dep.name = "package-a";
            dep.version_constraint = version;
            dependencies.push_back(dep);
        }
        
        // Package B with multiple versions
        std::vector<std::string> versions_b = {"0.5.0", "1.0.0"};
        for (const auto& version : versions_b) {
            PackageDependency dep;
            dep.name = "package-b";
            dep.version_constraint = version;
            dependencies.push_back(dep);
        }
        
        // Package C with single version (no conflict)
        PackageDependency dep_c;
        dep_c.name = "package-c";
        dep_c.version_constraint = "1.0.0";
        dependencies.push_back(dep_c);
        
        // Resolve the complex graph
        auto resolved_deps = package_module_->resolve_dependencies(dependencies);
        
        // Property: All packages should be present in resolution
        std::set<std::string> expected_packages = {"package-a", "package-b", "package-c"};
        std::set<std::string> resolved_packages;
        
        for (const auto& dep : resolved_deps) {
            resolved_packages.insert(dep.name);
        }
        
        EXPECT_EQ(resolved_packages, expected_packages) 
            << "All packages should be present in resolution";
        
        // Property: No conflicts should remain
        auto remaining_conflicts = package_module_->detect_conflicts(resolved_deps);
        EXPECT_TRUE(remaining_conflicts.empty()) 
            << "Complex graph should resolve without remaining conflicts";
        
        return true;
    });
}

// Property test: Development dependency conflict handling
TEST_F(DependencyConflictResolutionPropertyTest, DevDependencyConflictHandling) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(30, [this](std::mt19937& rng) {
        // Create dependencies with mix of regular and dev dependencies
        std::vector<PackageDependency> dependencies;
        
        // Regular dependency
        PackageDependency regular_dep;
        regular_dep.name = "shared-package";
        regular_dep.version_constraint = "1.0.0";
        regular_dep.is_dev_dependency = false;
        dependencies.push_back(regular_dep);
        
        // Dev dependency with same package, different version
        PackageDependency dev_dep;
        dev_dep.name = "shared-package";
        dev_dep.version_constraint = "2.0.0";
        dev_dep.is_dev_dependency = true;
        dependencies.push_back(dev_dep);
        
        // Detect and resolve conflicts
        auto conflicts = package_module_->detect_conflicts(dependencies);
        
        if (!conflicts.empty()) {
            auto resolved_deps = package_module_->resolve_conflicts(conflicts);
            
            // Property: Resolution should handle dev vs regular dependency conflicts
            bool found_shared_package = false;
            for (const auto& dep : resolved_deps) {
                if (dep.name == "shared-package") {
                    found_shared_package = true;
                    
                    // Property: Resolved version should be one of the conflicting versions
                    EXPECT_TRUE(dep.version_constraint == "1.0.0" || dep.version_constraint == "2.0.0")
                        << "Resolved version should be one of the conflicting versions";
                    
                    break;
                }
            }
            
            EXPECT_TRUE(found_shared_package) 
                << "Shared package should be present in resolution";
        }
        
        return true;
    });
}

} // namespace meld::cli::test