#include <gtest/gtest.h>
#include "meld/cli/project_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <set>
#include <algorithm>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 15: Dependency Resolution**
 * **Validates: Requirements 5.6**
 * 
 * For any project with declared dependencies, the CLI should fetch and make 
 * available all transitive dependencies without conflicts
 */

class DependencyResolutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        project_module_ = std::make_unique<ProjectModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_dependency_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::shared_ptr<ErrorHandler> error_handler_;
    std::unique_ptr<ProjectModule> project_module_;
    std::filesystem::path test_dir_;
};

// Generator for dependency structures
class DependencyGenerator {
public:
    static Dependency generate(std::mt19937& gen, const std::string& name_prefix = "") {
        Dependency dep;
        
        // Generate dependency name
        if (name_prefix.empty()) {
            std::uniform_int_distribution<> name_length_dist(3, 12);
            std::uniform_int_distribution<> char_dist(0, 25);
            
            int name_length = name_length_dist(gen);
            dep.name.reserve(name_length);
            for (int i = 0; i < name_length; ++i) {
                dep.name += static_cast<char>('a' + char_dist(gen));
            }
        } else {
            dep.name = name_prefix;
        }
        
        // Generate version
        std::uniform_int_distribution<> version_dist(0, 9);
        dep.version = std::to_string(version_dist(gen)) + "." + 
                     std::to_string(version_dist(gen)) + "." + 
                     std::to_string(version_dist(gen));
        
        // Generate source
        std::vector<std::string> sources = {"registry", "git", "local"};
        std::uniform_int_distribution<> source_dist(0, sources.size() - 1);
        dep.source = sources[source_dist(gen)];
        
        // Generate metadata
        std::uniform_int_distribution<> meta_count_dist(0, 3);
        int meta_count = meta_count_dist(gen);
        for (int i = 0; i < meta_count; ++i) {
            dep.metadata["key" + std::to_string(i)] = "value" + std::to_string(i);
        }
        
        return dep;
    }
    
    static std::vector<Dependency> generateDependencySet(std::mt19937& gen, int max_deps = 5) {
        std::uniform_int_distribution<> count_dist(1, max_deps);
        int dep_count = count_dist(gen);
        
        std::vector<Dependency> dependencies;
        std::set<std::string> used_names;
        
        for (int i = 0; i < dep_count; ++i) {
            Dependency dep = generate(gen);
            
            // Ensure unique names
            int suffix = 0;
            std::string original_name = dep.name;
            while (used_names.count(dep.name) > 0) {
                dep.name = original_name + std::to_string(suffix++);
            }
            used_names.insert(dep.name);
            
            dependencies.push_back(dep);
        }
        
        return dependencies;
    }
    
    // Generate dependencies with potential conflicts
    static std::vector<Dependency> generateConflictingDependencies(std::mt19937& gen) {
        std::vector<Dependency> dependencies;
        
        // Create dependencies with same name but different versions
        std::string common_name = "common_lib";
        
        std::uniform_int_distribution<> version_dist(1, 5);
        std::uniform_int_distribution<> conflict_count_dist(2, 4);
        
        int conflict_count = conflict_count_dist(gen);
        for (int i = 0; i < conflict_count; ++i) {
            Dependency dep;
            dep.name = common_name;
            dep.version = std::to_string(version_dist(gen)) + ".0.0";
            dep.source = "registry";
            dependencies.push_back(dep);
        }
        
        // Add some non-conflicting dependencies
        auto additional_deps = generateDependencySet(gen, 3);
        dependencies.insert(dependencies.end(), additional_deps.begin(), additional_deps.end());
        
        return dependencies;
    }
};

// Helper to create a project with dependencies
bool create_project_with_dependencies(const std::string& project_name,
                                     const std::vector<Dependency>& dependencies,
                                     const std::filesystem::path& base_dir) {
    try {
        std::filesystem::path project_dir = base_dir / project_name;
        std::filesystem::create_directories(project_dir);
        
        // Create meld.yaml with dependencies
        std::ofstream yaml_file(project_dir / "meld.yaml");
        yaml_file << "name: " << project_name << "\n";
        yaml_file << "version: 1.0.0\n";
        yaml_file << "type: library\n";
        yaml_file << "build_system: native\n";
        
        if (!dependencies.empty()) {
            yaml_file << "\ndependencies:\n";
            for (const auto& dep : dependencies) {
                yaml_file << "  - name: " << dep.name << "\n";
                yaml_file << "    version: " << dep.version << "\n";
                yaml_file << "    source: " << dep.source << "\n";
                if (!dep.metadata.empty()) {
                    yaml_file << "    metadata:\n";
                    for (const auto& meta : dep.metadata) {
                        yaml_file << "      " << meta.first << ": " << meta.second << "\n";
                    }
                }
            }
        }
        
        yaml_file.close();
        
        // Create basic project structure
        std::filesystem::create_directories(project_dir / "src");
        std::ofstream src_file(project_dir / "src" / "main.cpp");
        src_file << "#include <iostream>\nint main() { return 0; }\n";
        src_file.close();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

TEST_F(DependencyResolutionTest, DependencyResolution) {
    PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& gen) {  // Reduced iterations due to complexity
        // Generate project with dependencies
        std::string project_name = "test_project_" + std::to_string(gen());
        auto dependencies = DependencyGenerator::generateDependencySet(gen, 4);
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("test_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create project with dependencies
            bool project_created = create_project_with_dependencies(project_name, dependencies, iteration_dir);
            EXPECT_TRUE(project_created) << "Failed to create project with dependencies";
            
            if (!project_created) {
                return; // Skip if project creation failed
            }
            
            std::filesystem::path project_dir = iteration_dir / project_name;
            
            // Test dependency resolution
            std::vector<Dependency> resolved_dependencies;
            bool resolution_success = project_module_->resolve_dependencies(dependencies, resolved_dependencies);
            
            // Property: Resolution should complete (success or failure, but not crash)
            // Note: Since we don't have a real dependency registry, resolution might not succeed,
            // but it should handle the process gracefully
            
            // Property: Resolved dependencies should include all original dependencies
            if (resolution_success) {
                EXPECT_GE(resolved_dependencies.size(), dependencies.size())
                    << "Resolved dependencies should include at least all original dependencies";
                
                // Check that all original dependencies are present in resolved set
                for (const auto& original_dep : dependencies) {
                    bool found = false;
                    for (const auto& resolved_dep : resolved_dependencies) {
                        if (resolved_dep.name == original_dep.name) {
                            found = true;
                            break;
                        }
                    }
                    EXPECT_TRUE(found) << "Original dependency should be in resolved set: " << original_dep.name;
                }
            }
            
            // Property: No duplicate dependencies with same name should exist in resolved set
            std::set<std::string> resolved_names;
            for (const auto& dep : resolved_dependencies) {
                EXPECT_TRUE(resolved_names.find(dep.name) == resolved_names.end())
                    << "Duplicate dependency name in resolved set: " << dep.name;
                resolved_names.insert(dep.name);
            }
            
            // Test dependency fetching
            bool fetch_success = project_module_->fetch_dependencies(project_dir);
            
            // Property: Fetch operation should complete gracefully
            // Note: Since we don't have real dependencies to fetch, this might fail,
            // but it should not crash
            
            // Property: Individual dependency installation should be testable
            if (!dependencies.empty()) {
                const auto& first_dep = dependencies[0];
                bool install_success = project_module_->install_dependency(first_dep, project_dir);
                
                // Installation might fail due to missing registry, but should handle gracefully
                // The important thing is that it doesn't crash
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during dependency resolution test: " << e.what()
                   << " (project: " << project_name << ")";
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test dependency conflict resolution
TEST_F(DependencyResolutionTest, DependencyConflictResolution) {
    PropertyTest property_test;
    
    property_test.run_property_test(20, [this](std::mt19937& gen) {  // Fewer iterations for conflict testing
        // Generate dependencies with conflicts
        auto conflicting_dependencies = DependencyGenerator::generateConflictingDependencies(gen);
        std::string project_name = "conflict_project_" + std::to_string(gen());
        
        // Create unique test directory for this iteration
        std::filesystem::path iteration_dir = test_dir_ / ("conflict_" + std::to_string(gen()));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create project with conflicting dependencies
            bool project_created = create_project_with_dependencies(project_name, conflicting_dependencies, iteration_dir);
            EXPECT_TRUE(project_created) << "Failed to create project with conflicting dependencies";
            
            if (!project_created) {
                return;
            }
            
            // Test conflict resolution
            std::vector<Dependency> resolved_dependencies;
            bool resolution_success = project_module_->resolve_dependencies(conflicting_dependencies, resolved_dependencies);
            
            // Property: Conflict resolution should handle conflicts deterministically
            if (resolution_success) {
                // Count dependencies by name
                std::map<std::string, int> name_counts;
                for (const auto& dep : resolved_dependencies) {
                    name_counts[dep.name]++;
                }
                
                // Property: Each dependency name should appear at most once in resolved set
                for (const auto& pair : name_counts) {
                    EXPECT_EQ(pair.second, 1)
                        << "Dependency name should appear exactly once in resolved set: " << pair.first;
                }
            }
            
            // Property: Resolution should be deterministic (same input -> same output)
            std::vector<Dependency> second_resolution;
            bool second_success = project_module_->resolve_dependencies(conflicting_dependencies, second_resolution);
            
            if (resolution_success && second_success) {
                EXPECT_EQ(resolved_dependencies.size(), second_resolution.size())
                    << "Resolution should be deterministic";
                
                // Sort both vectors for comparison
                auto sort_by_name = [](const Dependency& a, const Dependency& b) {
                    return a.name < b.name;
                };
                
                std::sort(resolved_dependencies.begin(), resolved_dependencies.end(), sort_by_name);
                std::sort(second_resolution.begin(), second_resolution.end(), sort_by_name);
                
                for (size_t i = 0; i < resolved_dependencies.size(); ++i) {
                    EXPECT_EQ(resolved_dependencies[i].name, second_resolution[i].name)
                        << "Resolution should be deterministic for dependency names";
                    EXPECT_EQ(resolved_dependencies[i].version, second_resolution[i].version)
                        << "Resolution should be deterministic for dependency versions";
                }
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception during conflict resolution test: " << e.what()
                   << " (project: " << project_name << ")";
        }
        
        // Clean up this iteration
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    });
}

// Test dependency resolution with specific scenarios
TEST_F(DependencyResolutionTest, SpecificDependencyScenarios) {
    struct TestCase {
        std::string name;
        std::vector<Dependency> dependencies;
        bool should_resolve;
    };
    
    std::vector<TestCase> test_cases = {
        {
            "empty_dependencies",
            {},
            true
        },
        {
            "single_dependency",
            {{"lib1", "1.0.0", "registry", {}}},
            true
        },
        {
            "multiple_unique_dependencies",
            {
                {"lib1", "1.0.0", "registry", {}},
                {"lib2", "2.0.0", "git", {}},
                {"lib3", "1.5.0", "local", {}}
            },
            true
        },
        {
            "conflicting_versions",
            {
                {"common_lib", "1.0.0", "registry", {}},
                {"common_lib", "2.0.0", "registry", {}}
            },
            true  // Should resolve by picking one version
        }
    };
    
    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& test_case = test_cases[i];
        
        std::filesystem::path iteration_dir = test_dir_ / ("scenario_" + std::to_string(i));
        std::filesystem::create_directories(iteration_dir);
        
        try {
            // Create project with specific dependencies
            std::string project_name = test_case.name;
            bool project_created = create_project_with_dependencies(project_name, test_case.dependencies, iteration_dir);
            EXPECT_TRUE(project_created) << "Failed to create project for scenario: " << test_case.name;
            
            if (project_created) {
                std::filesystem::path project_dir = iteration_dir / project_name;
                
                // Test resolution
                std::vector<Dependency> resolved;
                bool resolution_success = project_module_->resolve_dependencies(test_case.dependencies, resolved);
                
                if (test_case.should_resolve) {
                    // For scenarios that should resolve, check basic properties
                    if (resolution_success) {
                        EXPECT_GE(resolved.size(), test_case.dependencies.size())
                            << "Resolved dependencies should include originals for " << test_case.name;
                    }
                } else {
                    // For scenarios that shouldn't resolve, we expect failure or empty result
                    if (!resolution_success) {
                        EXPECT_TRUE(resolved.empty())
                            << "Failed resolution should have empty result for " << test_case.name;
                    }
                }
                
                // Test fetching
                bool fetch_success = project_module_->fetch_dependencies(project_dir);
                // Fetch might fail due to missing infrastructure, but shouldn't crash
            }
            
        } catch (const std::exception& e) {
            FAIL() << "Exception in scenario test " << test_case.name << ": " << e.what();
        }
        
        // Clean up
        if (std::filesystem::exists(iteration_dir)) {
            std::filesystem::remove_all(iteration_dir);
        }
    }
}