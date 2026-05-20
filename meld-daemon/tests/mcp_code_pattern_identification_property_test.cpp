/**
 * **Feature: meld-daemon, Property 68: MCP Code Pattern Identification Accuracy**
 *
 * For any code pattern, similar constructs, idioms, and implementation
 * approaches SHALL be identified correctly.
 *
 * **Validates: Requirements 24.3**
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

void add_nodes(SemanticModel& model, const std::filesystem::path& file,
               const std::vector<std::pair<std::string, std::string>>& nodes) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, kind] : nodes) {
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
 * Property 68a: Searching for a pattern that matches a node's kind
 * returns that node.
 */
RC_GTEST_PROP(McpCodePatternIdentificationProperty,
              KindMatchReturnsNode,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_nodes(model, "test.meld", {{name, "pattern_match"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.find_code_patterns("pattern");

    bool found = false;
    for (const auto& r : results)
        if (r.matched_code.find(name) != std::string::npos) found = true;
    RC_ASSERT(found);
}

/**
 * Property 68b: Searching for a pattern that matches a node's name
 * returns that node.
 */
RC_GTEST_PROP(McpCodePatternIdentificationProperty,
              NameMatchReturnsNode,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_nodes(model, "test.meld", {{name, "struct"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.find_code_patterns(name);

    bool found = false;
    for (const auto& r : results)
        if (r.matched_code.find(name) != std::string::npos) found = true;
    RC_ASSERT(found);
}

/**
 * Property 68c: Empty pattern returns empty results.
 */
RC_GTEST_PROP(McpCodePatternIdentificationProperty,
              EmptyPatternReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_nodes(model, "test.meld", {{"foo", "struct"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.find_code_patterns("");
    RC_ASSERT(results.empty());
}

}  // namespace
}  // namespace meld::daemon
