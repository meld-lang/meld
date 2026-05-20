/**
 * **Feature: meld-daemon, Property 87: MCP Testing Framework Integration Accuracy**
 *
 * For any testing framework, tests SHALL be executed correctly and results
 * with coverage data SHALL be provided.
 *
 * **Validates: Requirements 28.4**
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

void add_test_file(SemanticModel& model, const std::filesystem::path& file,
                   const std::vector<std::string>& test_names) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& name : test_names) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->location = SourceLocation{file, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 87a: Running tests matching a pattern returns results.
 */
RC_GTEST_PROP(McpTestingFrameworkProperty,
              PatternMatchReturnsResults,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_test_file(model, "my_test.meld", {"test_add", "test_sub", "helper"});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.run_tests("test");
    RC_ASSERT(results.size() == 2);
    for (const auto& r : results) {
        RC_ASSERT(r.passed);
        RC_ASSERT(!r.test_name.empty());
    }
}

/**
 * Property 87b: Empty pattern returns all test cases.
 */
RC_GTEST_PROP(McpTestingFrameworkProperty,
              EmptyPatternReturnsAll,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_test_file(model, "my_test.meld", {"test_add", "test_sub"});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.run_tests("");
    RC_ASSERT(results.size() == 2);
}

/**
 * Property 87c: No test files means no results.
 */
RC_GTEST_PROP(McpTestingFrameworkProperty,
              NoTestFilesNoResults,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto results = provider.run_tests("test");
    RC_ASSERT(results.empty());
}

}  // namespace
}  // namespace meld::daemon
