/**
 * **Feature: meld-daemon, Property 76: MCP Function Extraction Correctness**
 *
 * For any function extraction operation, proper scoping, type signatures,
 * and multiple dispatch compatibility SHALL be maintained.
 *
 * **Validates: Requirements 26.2**
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

void add_function(SemanticModel& model, const std::filesystem::path& file,
                  const std::string& name) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto child = std::make_shared<ASTNode>();
    child->kind = "function_definition";
    child->name = name;
    child->type_info = "(Int) -> Int";
    child->location = SourceLocation{file, 1, 0};
    root->children.push_back(child);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 76a: Extraction with valid range and unique name succeeds.
 */
RC_GTEST_PROP(McpFuncExtractionProperty,
              ValidExtractionSucceeds,
              ()) {
    auto start = *rc::gen::inRange(1u, 10u);
    auto end = *rc::gen::inRange(start, start + 10u);

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "sourceFunc");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.extract_function(file, "sourceFunc", "newHelper", start, end);
    RC_ASSERT(result.scoping_correct);
    RC_ASSERT(result.dispatch_compatible);
    RC_ASSERT(result.errors.empty());
}

/**
 * Property 76b: Extraction from non-existent source fails.
 */
RC_GTEST_PROP(McpFuncExtractionProperty,
              NonExistentSourceFails,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "existing");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.extract_function(file, "missing", "newFunc", 1, 5);
    RC_ASSERT(!result.scoping_correct);
    RC_ASSERT(!result.errors.empty());
}

/**
 * Property 76c: Invalid new function name is rejected.
 */
RC_GTEST_PROP(McpFuncExtractionProperty,
              InvalidNameRejected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "sourceFunc");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.extract_function(file, "sourceFunc", "123bad", 1, 5);
    RC_ASSERT(!result.scoping_correct);
    RC_ASSERT(!result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
