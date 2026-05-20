/**
 * **Feature: meld-daemon, Property 75: MCP Symbol Renaming Semantic Preservation**
 *
 * For any symbol rename operation via MCP, all references SHALL be updated
 * while preserving semantic correctness.
 *
 * **Validates: Requirements 26.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
#include <string>

namespace meld::daemon {
namespace {

VectorIndex make_vector_index() {
    return VectorIndex(std::make_shared<PassthroughEmbeddingProvider>());
}

void add_symbol(SemanticModel& model, const std::filesystem::path& file,
                const std::string& name, const std::string& kind) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto child = std::make_shared<ASTNode>();
    child->kind = kind;
    child->name = name;
    child->type_info = "Int";
    child->location = SourceLocation{file, 1, 0};
    root->children.push_back(child);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

rc::Gen<std::string> genIdent() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

/**
 * Property 75a: Renaming to a valid, non-conflicting name succeeds.
 */
RC_GTEST_PROP(McpSymbolRenameProperty,
              ValidRenameSucceeds,
              ()) {
    auto old_name = *genIdent();
    auto new_name = *genIdent();
    RC_PRE(old_name != new_name);

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_symbol(model, file, old_name, "function_definition");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.rename_symbol(file, old_name, new_name);
    RC_ASSERT(result.semantically_correct);
    RC_ASSERT(result.references_updated > 0);
    RC_ASSERT(result.errors.empty());
}

/**
 * Property 75b: Renaming to a keyword is rejected.
 */
RC_GTEST_PROP(McpSymbolRenameProperty,
              RenameToKeywordRejected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_symbol(model, file, "myFunc", "function_definition");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.rename_symbol(file, "myFunc", "fnc");
    RC_ASSERT(!result.semantically_correct);
    RC_ASSERT(!result.errors.empty());
}

/**
 * Property 75c: Renaming a non-existent symbol fails.
 */
RC_GTEST_PROP(McpSymbolRenameProperty,
              NonExistentSymbolFails,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_symbol(model, file, "existing", "struct");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.rename_symbol(file, "nonexistent", "newName");
    RC_ASSERT(!result.semantically_correct);
    RC_ASSERT(!result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
