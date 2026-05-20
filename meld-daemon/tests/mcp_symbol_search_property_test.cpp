/**
 * **Feature: meld-daemon, Property 66: MCP Symbol Search Completeness**
 *
 * For any symbol in a codebase, all definitions, references, and usages
 * SHALL be located by symbol name search.
 *
 * **Validates: Requirements 24.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

VectorIndex make_vector_index() {
    return VectorIndex(std::make_shared<PassthroughEmbeddingProvider>());
}

rc::Gen<std::string> genIdent() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

void add_symbols(SemanticModel& model, const std::filesystem::path& file,
                 const std::vector<std::pair<std::string, std::string>>& syms) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, kind] : syms) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = "Int";
        child->location = SourceLocation{file, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 66a: A symbol placed in multiple files is found in all of them.
 */
RC_GTEST_PROP(McpSymbolSearchProperty,
              SymbolFoundAcrossFiles,
              ()) {
    auto sym_name = *genIdent();
    auto n_files = *rc::gen::inRange(1, 4);

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i) {
        std::filesystem::path f = "file_" + std::to_string(i) + ".meld";
        add_symbols(model, f, {{sym_name, "function_definition"}});
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_symbol(sym_name);
    RC_ASSERT(static_cast<int>(results.size()) == n_files);
    for (const auto& r : results)
        RC_ASSERT(r.name == sym_name);
}

/**
 * Property 66b: Searching for a non-existent symbol returns empty.
 */
RC_GTEST_PROP(McpSymbolSearchProperty,
              NonExistentSymbolReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_symbols(model, "test.meld", {{"existing", "struct"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_symbol("nonexistent_xyz_123");
    RC_ASSERT(results.empty());
}

/**
 * Property 66c: Empty symbol name returns empty results.
 */
RC_GTEST_PROP(McpSymbolSearchProperty,
              EmptyNameReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_symbols(model, "test.meld", {{"foo", "struct"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_symbol("");
    RC_ASSERT(results.empty());
}

}  // namespace
}  // namespace meld::daemon
