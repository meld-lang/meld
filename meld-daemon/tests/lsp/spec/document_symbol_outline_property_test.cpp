/**
 * **Feature: meld-lsp-server, Property 18: Document symbol outline accuracy**
 *
 * For any Meld file, the document symbols should provide a complete
 * hierarchical outline of all functions, types, and declarations.
 *
 * **Validates: Requirements 4.3**
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

/// Check if a symbol with the given name and kind exists in the list
bool has_symbol(const std::vector<SymbolInfo>& symbols,
                const std::string& name, SymbolKind kind) {
    return std::any_of(symbols.begin(), symbols.end(),
        [&](const SymbolInfo& s) { return s.name == name && s.kind == kind; });
}

/// Check if a symbol with the given name exists (any kind)
bool has_symbol_named(const std::vector<SymbolInfo>& symbols,
                      const std::string& name) {
    return std::any_of(symbols.begin(), symbols.end(),
        [&](const SymbolInfo& s) { return s.name == name; });
}

} // anonymous namespace

/**
 * Property 18.1: All function declarations appear in the document symbol outline.
 */
TEST(DocumentSymbolOutlinePropertyTest, AllFunctionsInOutline) {
    rc::check("Every function declaration appears in document symbols",
        []() {
            int num_funcs = *rc::gen::inRange(1, 8);

            std::ostringstream oss;
            std::vector<std::string> func_names;
            for (int i = 0; i < num_funcs; ++i) {
                std::string name = "func_" + std::to_string(i);
                func_names.push_back(name);
                oss << "fnc " << name << "(x: Int) -> Int {\n";
                oss << "    return x\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            LanguageService service;
            auto symbols = service.get_document_symbols(
                "file:///test.meld", source);

            for (const auto& name : func_names) {
                RC_ASSERT(has_symbol(symbols, name, SymbolKind::Function));
            }
        }
    );
}

/**
 * Property 18.2: All struct declarations appear in the document symbol outline.
 */
TEST(DocumentSymbolOutlinePropertyTest, AllStructsInOutline) {
    rc::check("Every struct declaration appears in document symbols",
        []() {
            int num_structs = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            std::vector<std::string> struct_names;
            for (int i = 0; i < num_structs; ++i) {
                std::string name = "MyStruct" + std::to_string(i);
                struct_names.push_back(name);
                oss << "struct " << name << " {\n";
                oss << "    value: Int\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            LanguageService service;
            auto symbols = service.get_document_symbols(
                "file:///test.meld", source);

            for (const auto& name : struct_names) {
                RC_ASSERT(has_symbol(symbols, name, SymbolKind::Struct));
            }
        }
    );
}

/**
 * Property 18.3: All variable declarations appear in the document symbol outline.
 */
TEST(DocumentSymbolOutlinePropertyTest, AllVariablesInOutline) {
    rc::check("Every let/var declaration appears in document symbols",
        []() {
            int num_vars = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            oss << "fnc main() {\n";
            std::vector<std::string> var_names;
            for (int i = 0; i < num_vars; ++i) {
                std::string name = "myvar_" + std::to_string(i);
                var_names.push_back(name);
                std::string decl = (i % 2 == 0) ? "let" : "var";
                oss << "    " << decl << " " << name << " = " << i << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto symbols = service.get_document_symbols(
                "file:///test.meld", source);

            for (const auto& name : var_names) {
                RC_ASSERT(has_symbol(symbols, name, SymbolKind::Variable));
            }
        }
    );
}

/**
 * Property 18.4: Mixed declarations (functions, structs, enums, traits)
 * all appear in the outline with correct kinds.
 */
TEST(DocumentSymbolOutlinePropertyTest, MixedDeclarationsInOutline) {
    rc::check("Mixed declaration types all appear with correct SymbolKind",
        []() {
            int num_funcs = *rc::gen::inRange(1, 3);
            int num_structs = *rc::gen::inRange(0, 3);
            int num_enums = *rc::gen::inRange(0, 3);

            std::ostringstream oss;
            std::vector<std::pair<std::string, SymbolKind>> expected;

            for (int i = 0; i < num_funcs; ++i) {
                std::string name = "fn_" + std::to_string(i);
                expected.push_back({name, SymbolKind::Function});
                oss << "fnc " << name << "() {\n    return 0\n}\n\n";
            }
            for (int i = 0; i < num_structs; ++i) {
                std::string name = "Struct" + std::to_string(i);
                expected.push_back({name, SymbolKind::Struct});
                oss << "struct " << name << " {\n    x: Int\n}\n\n";
            }
            for (int i = 0; i < num_enums; ++i) {
                std::string name = "Enum" + std::to_string(i);
                expected.push_back({name, SymbolKind::Enum});
                oss << "enum " << name << " {\n    A\n}\n\n";
            }

            std::string source = oss.str();

            LanguageService service;
            auto symbols = service.get_document_symbols(
                "file:///test.meld", source);

            for (const auto& [name, kind] : expected) {
                RC_ASSERT(has_symbol(symbols, name, kind));
            }
        }
    );
}

/**
 * Property 18.5: Document symbols have accurate definition locations.
 */
TEST(DocumentSymbolOutlinePropertyTest, SymbolLocationsAreAccurate) {
    rc::check("Each symbol's definition location points to the correct line",
        []() {
            int num_funcs = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc func_" << i << "() {\n";
                oss << "    return " << i << "\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            LanguageService service;
            auto symbols = service.get_document_symbols(
                "file:///test.meld", source);

            // Each function should be at line i*4
            for (int i = 0; i < num_funcs; ++i) {
                std::string name = "func_" + std::to_string(i);
                auto it = std::find_if(symbols.begin(), symbols.end(),
                    [&](const SymbolInfo& s) { return s.name == name; });
                RC_ASSERT(it != symbols.end());
                RC_ASSERT(it->definition.line == i * 4);
            }
        }
    );
}
