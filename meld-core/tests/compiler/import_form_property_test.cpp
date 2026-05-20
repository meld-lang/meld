#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/module_resolver.hpp"
#include "meld/parser/parser.hpp"
#include <string>
#include <vector>
#include <set>

using namespace meld::compiler;
using namespace meld::parser;
namespace x3 = boost::spirit::x3;

namespace {

// Generator for symbol names that are valid Meld identifiers
rc::Gen<std::string> genSymbolName() {
    return rc::gen::elementOf(std::vector<std::string>{
        "sin", "cos", "tan", "sqrt", "abs", "floor", "ceil",
        "PI", "E", "TAU", "round", "pow", "log", "exp"
    });
}

// Generator for module alias names
rc::Gen<std::string> genAliasName() {
    return rc::gen::elementOf(std::vector<std::string>{
        "m", "math", "io", "fs", "net", "util", "lib", "pkg"
    });
}

// Helper: parse a single imp statement and return the import_declaration
std::optional<ast::import_declaration> parseImp(const std::string& code) {
    Parser parser;
    std::vector<ast::expression> result;
    if (!parser.parse_file(code, result)) return std::nullopt;
    if (result.empty()) return std::nullopt;
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    if (!imp) return std::nullopt;
    return imp->get();
}

} // anonymous namespace

// ============================================================================
// Property: Import Form Equivalence
// For any module M with exported symbol S, all four import forms (basic,
// aliased, destructured, destructured-with-alias) should provide equivalent
// access to S.
// Validates: Requirements 31B.4, 31B.5, 31B.6, 31B.7
// ============================================================================

TEST(ImportFormProperty, BasicImportParsesCorrectly) {
    rc::check("Basic import form always produces IMP_BASIC with correct path",
        [](void) {
            auto segments = *rc::gen::map(
                rc::gen::inRange(1, 4),
                [](int n) {
                    std::vector<std::string> pool = {"std", "app", "lib", "math", "io"};
                    std::vector<std::string> result;
                    for (int i = 0; i < n; ++i)
                        result.push_back(pool[i % pool.size()]);
                    return result;
                }
            );
            
            // Build imp statement
            std::string code = "imp ";
            for (size_t i = 0; i < segments.size(); ++i) {
                if (i > 0) code += ".";
                code += segments[i];
            }
            
            auto decl = parseImp(code);
            RC_ASSERT(decl.has_value());
            RC_ASSERT(decl->import_type == ast::ImportType::IMP_BASIC);
            RC_ASSERT(decl->is_imp);
            RC_ASSERT(decl->namespace_path.size() == segments.size());
            
            for (size_t i = 0; i < segments.size(); ++i) {
                RC_ASSERT(decl->namespace_path[i] == segments[i]);
            }
        }
    );
}

TEST(ImportFormProperty, AliasedImportParsesCorrectly) {
    rc::check("Aliased import form always produces IMP_ALIASED with alias",
        [](void) {
            auto alias = *genAliasName();
            
            std::string code = "imp " + alias + " = std.math";
            
            auto decl = parseImp(code);
            RC_ASSERT(decl.has_value());
            RC_ASSERT(decl->import_type == ast::ImportType::IMP_ALIASED);
            RC_ASSERT(decl->is_imp);
            RC_ASSERT(decl->has_alias);
            RC_ASSERT(decl->alias == alias);
            RC_ASSERT(decl->namespace_path.size() == 2);
            RC_ASSERT(decl->namespace_path[0] == "std");
            RC_ASSERT(decl->namespace_path[1] == "math");
        }
    );
}

TEST(ImportFormProperty, DestructuredImportParsesCorrectly) {
    rc::check("Destructured import form extracts correct symbols",
        [](void) {
            auto count = *rc::gen::inRange(1, 5);
            std::vector<std::string> symbols;
            for (int i = 0; i < count; ++i) {
                symbols.push_back(*genSymbolName());
            }
            // Deduplicate
            std::sort(symbols.begin(), symbols.end());
            symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
            if (symbols.empty()) symbols.push_back("x");
            
            // Build imp { sym1, sym2 } = std.math
            std::string code = "imp { ";
            for (size_t i = 0; i < symbols.size(); ++i) {
                if (i > 0) code += ", ";
                code += symbols[i];
            }
            code += " } = std.math";
            
            auto decl = parseImp(code);
            RC_ASSERT(decl.has_value());
            RC_ASSERT(decl->import_type == ast::ImportType::IMP_DESTRUCTURED);
            RC_ASSERT(decl->is_imp);
            RC_ASSERT(decl->symbols.size() == symbols.size());
            
            for (size_t i = 0; i < symbols.size(); ++i) {
                RC_ASSERT(decl->symbols[i].original_name == symbols[i]);
                RC_ASSERT(decl->symbols[i].local_name == symbols[i]);
                RC_ASSERT(!decl->symbols[i].has_alias);
            }
        }
    );
}

TEST(ImportFormProperty, DestructuredWithAliasParsesCorrectly) {
    rc::check("Destructured import with alias maps symbols correctly",
        [](void) {
            auto original = *genSymbolName();
            auto alias = *rc::gen::distinctFrom(genAliasName(), original);
            
            std::string code = "imp { " + original + " -> " + alias + " } = std.math";
            
            auto decl = parseImp(code);
            RC_ASSERT(decl.has_value());
            RC_ASSERT(decl->import_type == ast::ImportType::IMP_DESTRUCTURED);
            RC_ASSERT(decl->is_imp);
            RC_ASSERT(decl->symbols.size() == 1);
            RC_ASSERT(decl->symbols[0].original_name == original);
            RC_ASSERT(decl->symbols[0].local_name == alias);
            RC_ASSERT(decl->symbols[0].has_alias);
        }
    );
}

TEST(ImportFormProperty, AllFormsPreserveModulePath) {
    rc::check("All four import forms preserve the same module path",
        [](void) {
            auto symbol = *genSymbolName();
            auto alias = *genAliasName();
            
            // Form 1: basic
            auto basic = parseImp("imp std.math");
            // Form 2: aliased
            auto aliased = parseImp("imp " + alias + " = std.math");
            // Form 3: destructured
            auto destructured = parseImp("imp { " + symbol + " } = std.math");
            // Form 4: destructured with alias
            auto destr_alias = parseImp("imp { " + symbol + " -> " + alias + " } = std.math");
            
            RC_ASSERT(basic.has_value());
            RC_ASSERT(aliased.has_value());
            RC_ASSERT(destructured.has_value());
            RC_ASSERT(destr_alias.has_value());
            
            // All forms should have the same module path
            RC_ASSERT(basic->namespace_path == aliased->namespace_path);
            RC_ASSERT(basic->namespace_path == destructured->namespace_path);
            RC_ASSERT(basic->namespace_path == destr_alias->namespace_path);
            
            // All forms should be marked as imp
            RC_ASSERT(basic->is_imp);
            RC_ASSERT(aliased->is_imp);
            RC_ASSERT(destructured->is_imp);
            RC_ASSERT(destr_alias->is_imp);
        }
    );
}

// ============================================================================
// Property: Circular Import Detection
// For any set of modules with a cyclic dependency, the compiler should
// reject the cycle at compile time.
// Validates: Requirements 31E.17
// ============================================================================

TEST(ImportFormProperty, CircularImportAlwaysDetected) {
    rc::check("Any circular dependency chain is detected regardless of length",
        [](void) {
            auto chain_len = *rc::gen::inRange(2, 10);
            
            ModuleDependencyGraph graph;
            std::vector<std::string> chain;
            for (int i = 0; i < chain_len; ++i) {
                chain.push_back("module_" + std::to_string(i));
            }
            
            // Build a chain: m0 → m1 → m2 → ... → m(n-1)
            for (size_t i = 0; i + 1 < chain.size(); ++i) {
                graph.add_dependency(chain[i], chain[i + 1]);
            }
            
            // Closing the cycle: m(n-1) → m0 should be detected
            auto cycle = graph.would_create_cycle(chain.back(), chain.front());
            RC_ASSERT(!cycle.empty());
        }
    );
}

TEST(ImportFormProperty, SelfImportDetected) {
    rc::check("A module importing itself is always detected as circular",
        [](void) {
            auto module_name = *rc::gen::elementOf(std::vector<std::string>{
                "app.main", "std.math", "lib.utils", "core.types"
            });
            
            ModuleDependencyGraph graph;
            auto cycle = graph.would_create_cycle(module_name, module_name);
            RC_ASSERT(!cycle.empty());
        }
    );
}

TEST(ImportFormProperty, NonCircularDependenciesAllowed) {
    rc::check("Non-circular dependency chains are always allowed",
        [](void) {
            auto chain_len = *rc::gen::inRange(2, 10);
            
            ModuleDependencyGraph graph;
            
            // Build a strict chain (no back-edges)
            for (int i = 0; i + 1 < chain_len; ++i) {
                std::string from = "mod_" + std::to_string(i);
                std::string to = "mod_" + std::to_string(i + 1);
                
                auto cycle = graph.would_create_cycle(from, to);
                RC_ASSERT(cycle.empty());
                graph.add_dependency(from, to);
            }
            
            // The overall graph should be acyclic
            auto cycle = graph.find_cycle();
            RC_ASSERT(cycle.empty());
        }
    );
}

TEST(ImportFormProperty, CAPCompatibleErrorFormat) {
    // Verify that circular import errors produce valid CAP-compatible JSON
    ModuleResolutionError error{
        ModuleResolutionError::Kind::CIRCULAR_IMPORT,
        "Circular import detected",
        "app.main",
        {"app.main", "app.services", "app.models", "app.main"}
    };
    
    std::string json = error.to_cap_json();
    
    // Should contain required fields
    EXPECT_TRUE(json.find("circular_import") != std::string::npos);
    EXPECT_TRUE(json.find("app.main") != std::string::npos);
    EXPECT_TRUE(json.find("cycle") != std::string::npos);
    EXPECT_TRUE(json.find("app.services") != std::string::npos);
    EXPECT_TRUE(json.find("app.models") != std::string::npos);
}

TEST(ImportFormProperty, ImportKeywordProducesHelpfulError) {
    // Verify the old 'import' keyword is rejected with a helpful message
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_FALSE(parser.parse_file("import std.math", result));
    
    std::string error = parser.error_message();
    EXPECT_TRUE(error.find("imp") != std::string::npos)
        << "Error should suggest using 'imp': " << error;
}
