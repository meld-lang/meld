/**
 * **Feature: meld-lsp-server, Property 19: Workspace symbol search completeness**
 *
 * For any workspace, symbol search should provide access to all symbols
 * across all files.
 *
 * **Validates: Requirements 4.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <set>

namespace {

using namespace meld::lsp::services;
using namespace meld::lsp::analysis;

/// Check if a symbol with the given name exists in the results
bool has_symbol_named(const std::vector<SymbolInfo>& symbols,
                      const std::string& name) {
    return std::any_of(symbols.begin(), symbols.end(),
        [&](const SymbolInfo& s) { return s.name == name; });
}

} // anonymous namespace

/**
 * Property 19.1: Workspace symbol search with empty query returns all
 * symbols from all analyzed files.
 */
TEST(WorkspaceSymbolSearchPropertyTest, EmptyQueryReturnsAllSymbols) {
    rc::check("Empty query returns all symbols across all analyzed files",
        []() {
            int num_files = *rc::gen::inRange(1, 4);
            int funcs_per_file = *rc::gen::inRange(1, 4);

            LanguageService service;
            std::vector<std::string> all_names;

            for (int f = 0; f < num_files; ++f) {
                std::ostringstream oss;
                std::string uri = "file:///workspace/file_" +
                                  std::to_string(f) + ".meld";
                for (int i = 0; i < funcs_per_file; ++i) {
                    std::string name = "f" + std::to_string(f) +
                                       "_func_" + std::to_string(i);
                    all_names.push_back(name);
                    oss << "fnc " << name << "() {\n";
                    oss << "    return " << i << "\n";
                    oss << "}\n\n";
                }
                // Analyze each file to populate the cache
                service.get_document_symbols(uri, oss.str());
            }

            // Search with empty query
            auto results = service.search_workspace_symbols("");

            // All function names from all files should be present
            for (const auto& name : all_names) {
                RC_ASSERT(has_symbol_named(results, name));
            }
        }
    );
}

/**
 * Property 19.2: Workspace symbol search with a specific query returns
 * only matching symbols (case-insensitive substring match).
 */
TEST(WorkspaceSymbolSearchPropertyTest, QueryFiltersSymbols) {
    rc::check("Query filters symbols by case-insensitive substring match",
        []() {
            LanguageService service;

            // File 1: functions with "alpha" in name
            std::string src1 =
                "fnc alpha_one() {\n    return 1\n}\n\n"
                "fnc alpha_two() {\n    return 2\n}\n\n";
            service.get_document_symbols("file:///a.meld", src1);

            // File 2: functions with "beta" in name
            std::string src2 =
                "fnc beta_one() {\n    return 1\n}\n\n"
                "fnc beta_two() {\n    return 2\n}\n\n";
            service.get_document_symbols("file:///b.meld", src2);

            // Search for "alpha"
            auto results = service.search_workspace_symbols("alpha");

            // Should find alpha_one and alpha_two
            RC_ASSERT(has_symbol_named(results, "alpha_one"));
            RC_ASSERT(has_symbol_named(results, "alpha_two"));

            // Should NOT find beta functions
            RC_ASSERT(!has_symbol_named(results, "beta_one"));
            RC_ASSERT(!has_symbol_named(results, "beta_two"));
        }
    );
}

/**
 * Property 19.3: Workspace symbol search is case-insensitive.
 */
TEST(WorkspaceSymbolSearchPropertyTest, CaseInsensitiveSearch) {
    rc::check("Workspace symbol search is case-insensitive",
        []() {
            LanguageService service;

            std::string src =
                "struct MyWidget {\n    x: Int\n}\n\n"
                "fnc create_widget() {\n    return 0\n}\n\n";
            service.get_document_symbols("file:///test.meld", src);

            // Search with different cases
            auto results_lower = service.search_workspace_symbols("widget");
            auto results_upper = service.search_workspace_symbols("WIDGET");
            auto results_mixed = service.search_workspace_symbols("Widget");

            // All should find MyWidget and create_widget
            RC_ASSERT(has_symbol_named(results_lower, "MyWidget"));
            RC_ASSERT(has_symbol_named(results_lower, "create_widget"));
            RC_ASSERT(has_symbol_named(results_upper, "MyWidget"));
            RC_ASSERT(has_symbol_named(results_upper, "create_widget"));
            RC_ASSERT(has_symbol_named(results_mixed, "MyWidget"));
            RC_ASSERT(has_symbol_named(results_mixed, "create_widget"));
        }
    );
}

/**
 * Property 19.4: Symbols from multiple files are all searchable.
 */
TEST(WorkspaceSymbolSearchPropertyTest, MultiFileSymbolsSearchable) {
    rc::check("Symbols from multiple analyzed files are all searchable",
        []() {
            int num_files = *rc::gen::inRange(2, 5);

            LanguageService service;
            std::set<std::string> all_struct_names;

            for (int f = 0; f < num_files; ++f) {
                std::string name = "Type" + std::to_string(f);
                all_struct_names.insert(name);
                std::string uri = "file:///ws/mod_" +
                                  std::to_string(f) + ".meld";
                std::string src = "struct " + name + " {\n    val: Int\n}\n";
                service.get_document_symbols(uri, src);
            }

            // Search for "Type" should find all structs
            auto results = service.search_workspace_symbols("Type");

            for (const auto& name : all_struct_names) {
                RC_ASSERT(has_symbol_named(results, name));
            }

            // Total matching results should be at least num_files
            RC_ASSERT(results.size() >= static_cast<size_t>(num_files));
        }
    );
}

/**
 * Property 19.5: Workspace symbol search returns symbols with correct
 * URI information from their originating file.
 */
TEST(WorkspaceSymbolSearchPropertyTest, SymbolsRetainFileOrigin) {
    rc::check("Workspace symbols retain their originating file URI",
        []() {
            LanguageService service;

            std::string uri1 = "file:///project/module_a.meld";
            std::string uri2 = "file:///project/module_b.meld";

            service.get_document_symbols(uri1,
                "fnc unique_alpha() {\n    return 1\n}\n");
            service.get_document_symbols(uri2,
                "fnc unique_beta() {\n    return 2\n}\n");

            auto results = service.search_workspace_symbols("unique");

            // Find each symbol and verify its URI
            for (const auto& sym : results) {
                if (sym.name == "unique_alpha") {
                    RC_ASSERT(sym.definition.uri == uri1);
                } else if (sym.name == "unique_beta") {
                    RC_ASSERT(sym.definition.uri == uri2);
                }
            }

            RC_ASSERT(has_symbol_named(results, "unique_alpha"));
            RC_ASSERT(has_symbol_named(results, "unique_beta"));
        }
    );
}
