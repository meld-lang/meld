/**
 * **Feature: meld-daemon, Property 85: MCP Build System Integration Accuracy**
 *
 * For any build system configuration, Bazel SHALL be interfaced correctly
 * to understand compilation processes.
 *
 * **Validates: Requirements 28.1**
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

void add_file(SemanticModel& model, const std::filesystem::path& file) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 85a: Build system is identified as Bazel.
 */
RC_GTEST_PROP(McpBuildSystemProperty,
              BuildSystemIsBazel,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto info = provider.get_build_system_info();
    RC_ASSERT(info.build_system == "bazel");
    RC_ASSERT(!info.rules.empty());
}

/**
 * Property 85b: Each indexed file produces a build target.
 */
RC_GTEST_PROP(McpBuildSystemProperty,
              EachFileProducesTarget,
              ()) {
    auto n_files = *rc::gen::inRange(1, 4);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i) {
        add_file(model, "src/file_" + std::to_string(i) + ".meld");
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto info = provider.get_build_system_info();
    RC_ASSERT(static_cast<int>(info.targets.size()) == n_files);
}

}  // namespace
}  // namespace meld::daemon
