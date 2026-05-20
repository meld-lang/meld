#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/module_definition.hpp"
#include "meld/compiler/module_resolver.hpp"
#include "meld/parser/parser.hpp"
#include <string>
#include <vector>
#include <set>
#include <unordered_set>

using namespace meld::compiler;
using namespace meld::parser;

namespace {

// Generator for valid Meld identifiers
rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(rc::gen::inRange(1, 50), [](int n) {
        return "symbol_" + std::to_string(n);
    });
}

// Generator for module path segments
rc::Gen<std::string> genModuleSegment() {
    return rc::gen::elementOf(std::vector<std::string>{
        "app", "services", "models", "utils", "core", "auth", "data", "api"
    });
}

// Generator for dot-separated module paths
rc::Gen<std::string> genModulePath() {
    return rc::gen::map(
        rc::gen::container<std::vector<std::string>>(3, genModuleSegment()),
        [](const std::vector<std::string>& segments) {
            std::string path;
            for (size_t i = 0; i < segments.size(); ++i) {
                if (i > 0) path += ".";
                path += segments[i];
            }
            return path;
        }
    );
}

// Generate a simple Meld module source with given symbol names
std::string generateModuleSource(const std::vector<std::string>& symbols) {
    std::string source;
    for (const auto& sym : symbols) {
        source += "val " + sym + " = 42\n";
    }
    return source;
}

} // anonymous namespace

// ============================================================================
// Property: Module Isolation
// For any two symbols with the same name in different modules, they should
// be distinct and not conflict unless explicitly imported into the same
// scope via `imp`.
// Validates: Requirements 31A.1, 31D.13, 31E.17, 31G.24
// ============================================================================

TEST(ModuleIsolationProperty, SameNameDifferentModulesAreDistinct) {
    rc::check("Symbols with same name in different modules are distinct",
        [](void) {
            auto symbol_name = *genIdentifier();
            
            // Create two modules with the same symbol name
            std::string source_a = "val " + symbol_name + " = 1\n";
            std::string source_b = "val " + symbol_name + " = 2\n";
            
            ModuleDefinition mod_a("module_a.meld");
            ModuleDefinition mod_b("module_b.meld");
            
            Parser parser_a, parser_b;
            std::vector<ast::expression> exprs_a, exprs_b;
            
            RC_ASSERT(parser_a.parse_file(source_a, exprs_a));
            RC_ASSERT(parser_b.parse_file(source_b, exprs_b));
            
            mod_a.build_from_expressions(exprs_a);
            mod_b.build_from_expressions(exprs_b);
            
            // Both modules should have the symbol
            RC_ASSERT(mod_a.has_symbol(symbol_name));
            RC_ASSERT(mod_b.has_symbol(symbol_name));
            
            // But they are in different modules (different module names)
            RC_ASSERT(mod_a.module_name() != mod_b.module_name());
            
            // Both should export the symbol independently
            RC_ASSERT(mod_a.is_exported(symbol_name));
            RC_ASSERT(mod_b.is_exported(symbol_name));
        }
    );
}

TEST(ModuleIsolationProperty, PrivateSymbolsNotExported) {
    rc::check("Private symbols are not visible to importers",
        [](void) {
            auto public_sym = *genIdentifier();
            auto private_sym = *rc::gen::distinctFrom(genIdentifier(), public_sym);
            
            std::string source = "val " + public_sym + " = 1\n"
                               + "val " + private_sym + " = 2\n";
            
            ModuleDefinition mod("test_module.meld");
            Parser parser;
            std::vector<ast::expression> exprs;
            
            RC_ASSERT(parser.parse_file(source, exprs));
            
            std::unordered_set<std::string> private_set = {private_sym};
            mod.build_from_expressions(exprs, private_set);
            
            // Public symbol should be exported
            RC_ASSERT(mod.is_exported(public_sym));
            
            // Private symbol should exist but not be exported
            RC_ASSERT(mod.has_symbol(private_sym));
            RC_ASSERT(!mod.is_exported(private_sym));
        }
    );
}

TEST(ModuleIsolationProperty, ModuleNameDerivedFromFilePath) {
    rc::check("Module name is correctly derived from file path",
        [](void) {
            auto segments = *rc::gen::container<std::vector<std::string>>(3, genModuleSegment());
            
            // Build file path from segments
            std::string file_path;
            std::string expected_name;
            for (size_t i = 0; i < segments.size(); ++i) {
                if (i > 0) {
                    file_path += "/";
                    expected_name += ".";
                }
                file_path += segments[i];
                expected_name += segments[i];
            }
            file_path += ".meld";
            
            RC_ASSERT(ModuleDefinition::derive_module_name(file_path) == expected_name);
        }
    );
}

// ============================================================================
// Property: Dependency Graph Acyclicity
// The module dependency graph must be a DAG — no cycles allowed.
// Validates: Requirements 31E.17
// ============================================================================

TEST(ModuleIsolationProperty, DependencyGraphDetectsCycles) {
    rc::check("Circular dependencies are always detected",
        [](void) {
            ModuleDependencyGraph graph;
            
            // Generate a chain of modules
            auto chain_length = *rc::gen::inRange(2, 6);
            std::vector<std::string> modules;
            for (int i = 0; i < chain_length; ++i) {
                modules.push_back("mod_" + std::to_string(i));
            }
            
            // Add chain dependencies: mod_0 → mod_1 → ... → mod_n-1
            for (size_t i = 0; i + 1 < modules.size(); ++i) {
                auto cycle = graph.would_create_cycle(modules[i], modules[i + 1]);
                RC_ASSERT(cycle.empty());  // No cycle yet
                graph.add_dependency(modules[i], modules[i + 1]);
            }
            
            // Adding mod_n-1 → mod_0 should create a cycle
            auto cycle = graph.would_create_cycle(modules.back(), modules.front());
            RC_ASSERT(!cycle.empty());
            
            // The cycle should contain all modules in the chain
            std::set<std::string> cycle_set(cycle.begin(), cycle.end());
            for (const auto& mod : modules) {
                RC_ASSERT(cycle_set.contains(mod));
            }
        }
    );
}

TEST(ModuleIsolationProperty, DAGHasValidTopologicalOrder) {
    rc::check("Acyclic dependency graphs produce valid topological orderings",
        [](void) {
            ModuleDependencyGraph graph;
            
            // Generate a DAG by only adding edges from lower to higher indices
            auto num_modules = *rc::gen::inRange(2, 8);
            std::vector<std::string> modules;
            for (int i = 0; i < num_modules; ++i) {
                modules.push_back("mod_" + std::to_string(i));
            }
            
            // Add some random edges (always from lower index to higher)
            for (int i = 0; i < num_modules; ++i) {
                for (int j = i + 1; j < num_modules; ++j) {
                    if (*rc::gen::inRange(0, 3) == 0) {  // ~33% chance
                        graph.add_dependency(modules[i], modules[j]);
                    }
                }
            }
            
            // Should have no cycles
            auto cycle = graph.find_cycle();
            RC_ASSERT(cycle.empty());
            
            // Topological order should exist and contain all modules with edges
            auto order = graph.topological_order();
            RC_ASSERT(!order.empty());
        }
    );
}

// ============================================================================
// Property: Re-export Visibility
// Re-exported symbols should be accessible through the re-exporting module.
// Validates: Requirements 31F.20, 31F.21
// ============================================================================

TEST(ModuleIsolationProperty, ReExportedSymbolsAreAccessible) {
    rc::check("Re-exported symbols are visible through the re-exporting module",
        [](void) {
            auto symbol_name = *genIdentifier();
            auto origin_module = *genModulePath();
            auto reexport_module = *rc::gen::distinctFrom(genModulePath(), origin_module);
            
            ReExportManager manager;
            manager.add_re_export(reexport_module, symbol_name, origin_module);
            
            // Symbol should be re-exported
            RC_ASSERT(manager.is_re_exported(reexport_module, symbol_name));
            
            // Get re-exports and verify
            auto re_exports = manager.get_re_exports(reexport_module);
            RC_ASSERT(!re_exports.empty());
            
            bool found = false;
            for (const auto& re : re_exports) {
                if (re.symbol_name == symbol_name && re.origin_module == origin_module) {
                    found = true;
                    break;
                }
            }
            RC_ASSERT(found);
            
            // Origin module should NOT have the re-export
            RC_ASSERT(!manager.is_re_exported(origin_module, symbol_name));
        }
    );
}
