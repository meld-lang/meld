/**
 * **Feature: meld-daemon, Property 77: MCP Module Reorganization Consistency**
 *
 * For any module reorganization, import/export statements and dependency
 * relationships SHALL be updated correctly.
 *
 * **Validates: Requirements 26.3**
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

void add_module(SemanticModel& model, const std::filesystem::path& file,
                const std::string& module_name) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = module_name;
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 77a: Reorganization with distinct imports is consistent.
 */
RC_GTEST_PROP(McpModuleReorgProperty,
              DistinctImportsConsistent,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_module(model, file, "mymod");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.reorganize_module(file, {"std.io", "std.math"}, {"foo"});
    RC_ASSERT(result.dependencies_consistent);
    RC_ASSERT(result.errors.empty());
}

/**
 * Property 77b: Self-import in reorganization is detected.
 */
RC_GTEST_PROP(McpModuleReorgProperty,
              SelfImportDetected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_module(model, file, "mymod");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.reorganize_module(file, {"mymod"}, {"foo"});
    RC_ASSERT(!result.dependencies_consistent);
    RC_ASSERT(!result.errors.empty());
}

/**
 * Property 77c: Reorganization of non-existent file fails.
 */
RC_GTEST_PROP(McpModuleReorgProperty,
              NonExistentFileFails,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.reorganize_module("missing.meld", {"std.io"}, {"foo"});
    RC_ASSERT(!result.dependencies_consistent);
    RC_ASSERT(!result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
