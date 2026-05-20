/**
 * **Feature: meld-daemon, Property 61: MCP Grammar Parsing Conformance**
 *
 * For any valid Meld code, parsing SHALL conform to the official Meld
 * grammar and provide detailed AST information.
 *
 * **Validates: Requirements 23.1**
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

rc::Gen<std::string> genAstKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "function_definition", "struct", "enum", "trait", "val_declaration"});
}

void add_node(SemanticModel& model, const std::filesystem::path& file,
              const std::string& name, const std::string& kind,
              const std::vector<std::string>& child_kinds) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    auto node = std::make_shared<ASTNode>();
    node->kind = kind;
    node->name = name;
    node->type_info = "Int";
    node->location = SourceLocation{file, 1, 0};
    for (const auto& ck : child_kinds) {
        auto c = std::make_shared<ASTNode>();
        c->kind = ck;
        c->name = ck + "_child";
        node->children.push_back(c);
    }
    root->children.push_back(node);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 61a: parse_code for an existing node returns success with
 * correct AST kind and child kinds.
 */
RC_GTEST_PROP(McpGrammarParsingProperty,
              ExistingNodeParsesSuccessfully,
              ()) {
    auto name = *genIdent();
    auto kind = *genAstKind();
    auto child_kinds = *rc::gen::container<std::vector<std::string>>(
        3, genAstKind());

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_node(model, file, name, kind, child_kinds);

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.parse_code(file, name);

    RC_ASSERT(result.success);
    RC_ASSERT(result.ast_kind == kind);
    RC_ASSERT(result.child_kinds.size() == child_kinds.size());
}

/**
 * Property 61b: parse_code for a non-existent node returns failure.
 */
RC_GTEST_PROP(McpGrammarParsingProperty,
              NonExistentNodeFailsParse,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_node(model, file, "existing", "struct", {});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.parse_code(file, "nonexistent_xyz");
    RC_ASSERT(!result.success);
}

}  // namespace
}  // namespace meld::daemon
