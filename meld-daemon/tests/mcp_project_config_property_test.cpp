/**
 * **Feature: meld-daemon, Property 80: MCP Project Configuration Access Completeness**
 *
 * For any project configuration, build settings, dependencies, and
 * compilation options SHALL be provided.
 *
 * **Validates: Requirements 27.1**
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

/**
 * Property 80a: Project config includes dependencies from the graph.
 */
RC_GTEST_PROP(McpProjectConfigProperty,
              DependenciesIncluded,
              ()) {
    auto n_deps = *rc::gen::inRange(1, 4);
    SemanticModel model;
    DependencyGraph dep_graph;
    for (int i = 0; i < n_deps; ++i) {
        DependencyNode dn;
        dn.name = "dep_" + std::to_string(i);
        dn.version = "1.0";
        dn.source = "registry";
        dep_graph.upsert(std::move(dn));
    }
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto config = provider.get_project_config();
    RC_ASSERT(static_cast<int>(config.dependencies.size()) == n_deps);
    RC_ASSERT(!config.build_settings.empty());
    RC_ASSERT(!config.compilation_options.empty());
}

/**
 * Property 80b: Empty project still returns valid config.
 */
RC_GTEST_PROP(McpProjectConfigProperty,
              EmptyProjectReturnsConfig,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto config = provider.get_project_config();
    RC_ASSERT(!config.project_name.empty());
    RC_ASSERT(!config.build_settings.empty());
}

}  // namespace
}  // namespace meld::daemon
