/**
 * **Feature: meld-daemon, Property 83: MCP Version Control Integration Accuracy**
 *
 * For any Git repository, change history and branch information SHALL be
 * provided.
 *
 * **Validates: Requirements 27.4**
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
 * Property 83a: Version control info includes branch and commit.
 */
RC_GTEST_PROP(McpVersionControlProperty,
              BranchAndCommitPresent,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_file(model, "test.meld");

    McpToolProvider provider(model, dep_graph, vi);
    auto info = provider.get_version_control_info();
    RC_ASSERT(!info.current_branch.empty());
    RC_ASSERT(!info.head_commit.empty());
}

/**
 * Property 83b: Modified files list matches indexed files.
 */
RC_GTEST_PROP(McpVersionControlProperty,
              ModifiedFilesMatchIndexed,
              ()) {
    auto n_files = *rc::gen::inRange(1, 4);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i) {
        add_file(model, "file_" + std::to_string(i) + ".meld");
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto info = provider.get_version_control_info();
    RC_ASSERT(static_cast<int>(info.modified_files.size()) == n_files);
}

}  // namespace
}  // namespace meld::daemon
