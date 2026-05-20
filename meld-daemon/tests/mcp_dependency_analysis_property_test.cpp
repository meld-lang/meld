/**
 * **Feature: meld-daemon, Property 59: MCP Dependency Analysis Accuracy**
 *
 * For any module structure, import/export relationships and dependency graphs
 * SHALL be correctly analyzed.
 *
 * **Validates: Requirements 22.4**
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

void add_file_with_imports_exports(
    SemanticModel& model, const std::filesystem::path& path,
    const std::vector<std::string>& imports,
    const std::vector<std::string>& exports) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& imp : imports) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "import";
        child->name = imp;
        child->location = SourceLocation{path, 1, 0};
        root->children.push_back(child);
    }
    for (const auto& exp : exports) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = exp;
        child->type_info = "() -> Int";
        child->location = SourceLocation{path, 2, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = path;
    sem.ast = root;
    model.update_file(path, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 59a: All import nodes in the AST appear in the dependency
 * analysis imports list.
 */
RC_GTEST_PROP(McpDependencyAnalysisProperty,
              ImportsAreReported,
              ()) {
    auto raw_imports = *rc::gen::container<std::vector<std::string>>(4, genIdent());
    std::vector<std::string> imports;
    std::set<std::string> seen_imp;
    for (auto& s : raw_imports)
        if (seen_imp.insert(s).second) imports.push_back(s);
    RC_PRE(!imports.empty());

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file_with_imports_exports(model, file, imports, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto analysis = provider.analyze_dependencies(file);

    std::set<std::string> found(analysis.imports.begin(), analysis.imports.end());
    for (const auto& imp : imports)
        RC_ASSERT(found.count(imp) > 0);
}

/**
 * Property 59b: All exported definitions appear in the exports list.
 */
RC_GTEST_PROP(McpDependencyAnalysisProperty,
              ExportsAreReported,
              ()) {
    auto raw_exports = *rc::gen::container<std::vector<std::string>>(4, genIdent());
    std::vector<std::string> exports;
    std::set<std::string> seen_exp;
    for (auto& s : raw_exports)
        if (seen_exp.insert(s).second) exports.push_back(s);
    RC_PRE(!exports.empty());

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file_with_imports_exports(model, file, {}, exports);

    McpToolProvider provider(model, dep_graph, vi);
    auto analysis = provider.analyze_dependencies(file);

    std::set<std::string> found(analysis.exports.begin(), analysis.exports.end());
    for (const auto& exp : exports)
        RC_ASSERT(found.count(exp) > 0);
}

/**
 * Property 59c: Dependency graph nodes are included in the analysis.
 */
RC_GTEST_PROP(McpDependencyAnalysisProperty,
              DependencyGraphNodesIncluded,
              ()) {
    auto dep_name = *genIdent();

    SemanticModel model;
    DependencyGraph dep_graph;
    dep_graph.upsert(DependencyNode{dep_name, "1.0", "registry", "", {}});
    auto vi = make_vector_index();

    std::filesystem::path file = "test.meld";
    add_file_with_imports_exports(model, file, {}, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto analysis = provider.analyze_dependencies(file);

    bool found = false;
    for (const auto& d : analysis.dependencies)
        if (d.name == dep_name) found = true;
    RC_ASSERT(found);
}

}  // namespace
}  // namespace meld::daemon
