/**
 * **Feature: meld-daemon, Property 84: MCP Test Suite Identification Completeness**
 *
 * For any test suite, test files, test cases, and coverage information
 * SHALL be identified correctly.
 *
 * **Validates: Requirements 27.5**
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
 * Property 84a: Test files with test functions are identified.
 */
RC_GTEST_PROP(McpTestSuiteProperty,
              TestFilesIdentified,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_test_file(model, "my_test.meld", {"test_add", "test_sub"});

    McpToolProvider provider(model, dep_graph, vi);
    auto suites = provider.get_test_suites();
    RC_ASSERT(!suites.empty());
    RC_ASSERT(!suites[0].test_files.empty());
    RC_ASSERT(suites[0].test_cases.size() == 2);
}

/**
 * Property 84b: Non-test files are not included in test suites.
 */
RC_GTEST_PROP(McpTestSuiteProperty,
              NonTestFilesExcluded,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    // File without "test" in name
    add_test_file(model, "main.meld", {"compute", "process"});

    McpToolProvider provider(model, dep_graph, vi);
    auto suites = provider.get_test_suites();
    RC_ASSERT(suites.empty());
}

/**
 * Property 84c: Empty model produces no test suites.
 */
RC_GTEST_PROP(McpTestSuiteProperty,
              EmptyModelNoSuites,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto suites = provider.get_test_suites();
    RC_ASSERT(suites.empty());
}

}  // namespace
}  // namespace meld::daemon
