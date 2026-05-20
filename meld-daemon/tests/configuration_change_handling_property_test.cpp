/**
 * **Feature: meld-daemon, Property 50: Configuration Change Handling**
 *
 * For any workspace configuration change, affected files SHALL be reloaded
 * and reindexed appropriately.
 *
 * **Validates: Requirements 20.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/workspace_provider.hpp"

#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void seed_workspace(SemanticModel& model, int count) {
    for (int i = 0; i < count; ++i) {
        std::filesystem::path p = "src/mod_" + std::to_string(i) + ".meld";
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = "mod_" + std::to_string(i);
        root->location = SourceLocation{p, 1, 0};
        FileSemantics sem;
        sem.path = p;
        sem.ast = root;
        model.update_file(p, std::move(sem));
    }
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 50a: After a config change, all indexed files are reindexed.
 */
RC_GTEST_PROP(ConfigurationChangeHandling,
              AllFilesReindexedOnConfigChange,
              ()) {
    auto file_count = *rc::gen::inRange(1, 8);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    seed_workspace(model, file_count);
    auto before_files = model.get_indexed_files();

    auto reindexed = provider.handle_config_change("meld.toml");

    // All previously indexed files should be reindexed
    std::set<std::filesystem::path> reindexed_set(reindexed.begin(),
                                                   reindexed.end());
    for (const auto& f : before_files) {
        RC_ASSERT(reindexed_set.count(f) == 1);
    }
}

/**
 * Property 50b: File count is preserved after config change reindexing.
 */
RC_GTEST_PROP(ConfigurationChangeHandling,
              FileCountPreservedAfterConfigChange,
              ()) {
    auto file_count = *rc::gen::inRange(1, 10);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    seed_workspace(model, file_count);
    size_t before = model.file_count();

    provider.handle_config_change("meld.toml");

    RC_ASSERT(model.file_count() == before);
}

/**
 * Property 50c: Stats reflect incremental mode after config change.
 */
RC_GTEST_PROP(ConfigurationChangeHandling,
              StatsUpdatedAfterConfigChange,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    seed_workspace(model, 3);
    provider.handle_config_change("meld.toml");

    auto stats = provider.get_stats();
    RC_ASSERT(stats.incremental == true);
    RC_ASSERT(stats.indexed_files == model.file_count());
}

/**
 * Property 50d: All files remain accessible in the model after config change.
 */
RC_GTEST_PROP(ConfigurationChangeHandling,
              AllFilesAccessibleAfterConfigChange,
              ()) {
    auto file_count = *rc::gen::inRange(1, 6);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    seed_workspace(model, file_count);
    auto files_before = model.get_indexed_files();

    provider.handle_config_change("meld.toml");

    for (const auto& f : files_before) {
        RC_ASSERT(model.has_file(f));
        RC_ASSERT(model.get_ast(f) != nullptr);
    }
}

}  // namespace
}  // namespace meld::daemon
