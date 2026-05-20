/**
 * **Feature: meld-daemon, Property 79: MCP Code Optimization Functional Equivalence**
 *
 * For any code optimization, functional equivalence SHALL be ensured while
 * suggesting improvements.
 *
 * **Validates: Requirements 26.5**
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
 * Property 79a: Optimization of existing function is functionally equivalent.
 */
RC_GTEST_PROP(McpCodeOptimizationProperty,
              ExistingFunctionOptimized,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "compute");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.optimize_code(file, "compute");
    RC_ASSERT(result.functionally_equivalent);
    RC_ASSERT(!result.original_code.empty());
    RC_ASSERT(!result.optimized_code.empty());
}

/**
 * Property 79b: Optimization of non-existent function fails.
 */
RC_GTEST_PROP(McpCodeOptimizationProperty,
              NonExistentFunctionFails,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "existing");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.optimize_code(file, "nonexistent");
    RC_ASSERT(!result.functionally_equivalent);
}

/**
 * Property 79c: Optimization preserves function name in output.
 */
RC_GTEST_PROP(McpCodeOptimizationProperty,
              OptimizationPreservesFunctionName,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "myFunc");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.optimize_code(file, "myFunc");
    RC_ASSERT(result.functionally_equivalent);
    RC_ASSERT(result.optimized_code.find("myFunc") != std::string::npos);
}

}  // namespace
}  // namespace meld::daemon
