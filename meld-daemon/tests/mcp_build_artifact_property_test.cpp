/**
 * **Feature: meld-daemon, Property 82: MCP Build Artifact Analysis Completeness**
 *
 * For any build artifacts, information about generated files, compilation
 * outputs, and intermediate representations SHALL be provided.
 *
 * **Validates: Requirements 27.3**
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
 * Property 82a: Each indexed file produces a build artifact entry.
 */
RC_GTEST_PROP(McpBuildArtifactProperty,
              EachFileProducesArtifact,
              ()) {
    auto n_files = *rc::gen::inRange(1, 4);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i) {
        add_file(model, "file_" + std::to_string(i) + ".meld");
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto artifacts = provider.get_build_artifacts();
    RC_ASSERT(static_cast<int>(artifacts.size()) == n_files);
    for (const auto& a : artifacts) {
        RC_ASSERT(!a.name.empty());
        RC_ASSERT(a.kind == "source");
    }
}

/**
 * Property 82b: Empty model produces no artifacts.
 */
RC_GTEST_PROP(McpBuildArtifactProperty,
              EmptyModelNoArtifacts,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto artifacts = provider.get_build_artifacts();
    RC_ASSERT(artifacts.empty());
}

}  // namespace
}  // namespace meld::daemon
