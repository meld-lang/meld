/**
 * **Feature: meld-daemon, Property 39: Workspace Symbol Search Completeness**
 *
 * For any workspace, symbol search SHALL provide access to all symbols
 * across all files.
 *
 * **Validates: Requirements 18.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/navigation_provider.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_file_with_symbols(SemanticModel& model,
                           const std::filesystem::path& file,
                           const std::vector<std::pair<std::string, std::string>>& symbols) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& [name, kind] : symbols) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = "Int";
        child->location = SourceLocation{file, 0, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

rc::Gen<std::pair<std::string, std::string>> genSymbol() {
    return rc::gen::oneOf(
        rc::gen::map(genIdentifier(), [](const std::string& n) {
            return std::make_pair(n, std::string("function_definition"));
        }),
        rc::gen::map(genIdentifier(), [](const std::string& n) {
            return std::make_pair(n, std::string("val_declaration"));
        }),
        rc::gen::map(genIdentifier(), [](const std::string& n) {
            return std::make_pair(n, std::string("struct"));
        }));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 39a: An empty query returns all symbols across all files.
 */
RC_GTEST_PROP(WorkspaceSymbolSearchProperty,
              EmptyQueryReturnsAllSymbols,
              ()) {
    auto syms_a = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        4, genSymbol());
    auto syms_b = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        4, genSymbol());

    SemanticModel model;
    add_file_with_symbols(model, "a.meld", syms_a);
    add_file_with_symbols(model, "b.meld", syms_b);

    NavigationProvider provider(model);
    auto results = provider.get_workspace_symbols("");

    // Should find at least as many symbols as we inserted
    RC_ASSERT(results.size() >= syms_a.size() + syms_b.size());
}

/**
 * Property 39b: Every symbol in the workspace is findable by its exact name.
 */
RC_GTEST_PROP(WorkspaceSymbolSearchProperty,
              EverySymbolFindableByName,
              ()) {
    auto syms = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        5, genSymbol());

    // Ensure unique names
    std::set<std::string> seen;
    std::vector<std::pair<std::string, std::string>> unique_syms;
    for (auto& s : syms) {
        if (seen.insert(s.first).second)
            unique_syms.push_back(s);
    }
    RC_PRE(!unique_syms.empty());

    SemanticModel model;
    add_file_with_symbols(model, "test.meld", unique_syms);

    NavigationProvider provider(model);

    for (const auto& [name, kind] : unique_syms) {
        auto results = provider.get_workspace_symbols(name);
        bool found = std::any_of(results.begin(), results.end(),
            [&](const WorkspaceSymbol& ws) { return ws.name == name; });
        RC_ASSERT(found);
    }
}

/**
 * Property 39c: Symbols from multiple files are all included in results.
 */
RC_GTEST_PROP(WorkspaceSymbolSearchProperty,
              SymbolsFromMultipleFilesIncluded,
              ()) {
    auto name_a = *genIdentifier();
    auto name_b = *genIdentifier();
    RC_PRE(name_a != name_b);

    SemanticModel model;
    add_file_with_symbols(model, "a.meld", {{name_a, "function_definition"}});
    add_file_with_symbols(model, "b.meld", {{name_b, "struct"}});

    NavigationProvider provider(model);
    auto results = provider.get_workspace_symbols("");

    bool found_a = std::any_of(results.begin(), results.end(),
        [&](const WorkspaceSymbol& ws) { return ws.name == name_a; });
    bool found_b = std::any_of(results.begin(), results.end(),
        [&](const WorkspaceSymbol& ws) { return ws.name == name_b; });

    RC_ASSERT(found_a);
    RC_ASSERT(found_b);
}

/**
 * Property 39d: Search is case-insensitive substring matching.
 */
RC_GTEST_PROP(WorkspaceSymbolSearchProperty,
              SearchIsCaseInsensitive,
              ()) {
    SemanticModel model;
    add_file_with_symbols(model, "test.meld",
                          {{"myFunction", "function_definition"},
                           {"MyStruct", "struct"}});

    NavigationProvider provider(model);

    // Lowercase query should find uppercase symbol
    auto results = provider.get_workspace_symbols("mystruct");
    bool found = std::any_of(results.begin(), results.end(),
        [](const WorkspaceSymbol& ws) { return ws.name == "MyStruct"; });
    RC_ASSERT(found);

    // Partial query should match
    auto partial = provider.get_workspace_symbols("func");
    bool found_partial = std::any_of(partial.begin(), partial.end(),
        [](const WorkspaceSymbol& ws) { return ws.name == "myFunction"; });
    RC_ASSERT(found_partial);
}

}  // namespace
}  // namespace meld::daemon
