/**
 * **Feature: meld-daemon, Property 69: MCP Language Feature Filtering Accuracy**
 *
 * For any specific Meld construct query, code using those features SHALL
 * be located correctly.
 *
 * **Validates: Requirements 24.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
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

void add_feature_nodes(SemanticModel& model,
                       const std::filesystem::path& file,
                       const std::vector<std::pair<std::string, std::string>>& nodes) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, kind] : nodes) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = "";
        child->location = SourceLocation{file, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 69a: Filtering by "multiple_dispatch" returns nodes with
 * that AST kind.
 */
RC_GTEST_PROP(McpLanguageFeatureFilteringProperty,
              MultipleDispatchNodesFound,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_feature_nodes(model, "test.meld", {
        {name, "multiple_dispatch"},
        {"other", "function_definition"}
    });

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.filter_by_feature("dispatch");

    bool found = false;
    for (const auto& r : results)
        if (r.symbol_name == name) found = true;
    RC_ASSERT(found);
}

/**
 * Property 69b: Filtering by "refinement" returns refinement_type nodes.
 */
RC_GTEST_PROP(McpLanguageFeatureFilteringProperty,
              RefinementTypeNodesFound,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_feature_nodes(model, "test.meld", {
        {name, "refinement_type"},
        {"other", "struct"}
    });

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.filter_by_feature("refinement");

    bool found = false;
    for (const auto& r : results)
        if (r.symbol_name == name) found = true;
    RC_ASSERT(found);
}

/**
 * Property 69c: Empty feature query returns empty results.
 */
RC_GTEST_PROP(McpLanguageFeatureFilteringProperty,
              EmptyFeatureReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_feature_nodes(model, "test.meld", {{"foo", "struct"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.filter_by_feature("");
    RC_ASSERT(results.empty());
}

}  // namespace
}  // namespace meld::daemon
