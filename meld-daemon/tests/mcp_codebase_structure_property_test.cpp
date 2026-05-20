/**
 * **Feature: meld-daemon, Property 57: MCP Codebase Structure Representation**
 *
 * For any Meld codebase, the hierarchical organization of files, modules,
 * and symbols SHALL be accurately represented.
 *
 * **Validates: Requirements 22.2**
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

void add_file_with_symbols(SemanticModel& model,
                           const std::filesystem::path& path,
                           const std::vector<std::string>& symbol_names) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = path.stem().string();
    for (const auto& name : symbol_names) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->type_info = "() -> Int";
        child->location = SourceLocation{path, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = path;
    sem.ast = root;
    model.update_file(path, std::move(sem));
}

rc::Gen<std::string> genIdent() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 57a: The root structure node has kind "workspace" and contains
 * one child per indexed file.
 */
RC_GTEST_PROP(McpCodebaseStructureProperty,
              RootContainsOneNodePerFile,
              ()) {
    auto n_files = *rc::gen::inRange(1, 5);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i)
        add_file_with_symbols(model, "f" + std::to_string(i) + ".meld", {"sym"});

    McpToolProvider provider(model, dep_graph, vi);
    auto root = provider.get_codebase_structure();

    RC_ASSERT(root.kind == "workspace");
    RC_ASSERT(static_cast<int>(root.children.size()) == n_files);
}

/**
 * Property 57b: Every symbol added to a file appears as a descendant in
 * the structure tree for that file.
 */
RC_GTEST_PROP(McpCodebaseStructureProperty,
              SymbolsAppearInStructure,
              ()) {
    auto raw = *rc::gen::container<std::vector<std::string>>(4, genIdent());
    std::vector<std::string> sym_names;
    std::set<std::string> seen_names;
    for (auto& s : raw)
        if (seen_names.insert(s).second) sym_names.push_back(s);
    RC_PRE(!sym_names.empty());

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_file_with_symbols(model, "test.meld", sym_names);

    McpToolProvider provider(model, dep_graph, vi);
    auto root = provider.get_codebase_structure();

    // Collect all names recursively
    std::set<std::string> found;
    std::function<void(const CodebaseNode&)> walk = [&](const CodebaseNode& n) {
        if (!n.name.empty()) found.insert(n.name);
        for (const auto& c : n.children) walk(c);
    };
    walk(root);

    for (const auto& name : sym_names)
        RC_ASSERT(found.count(name) > 0);
}

}  // namespace
}  // namespace meld::daemon
