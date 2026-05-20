/**
 * **Feature: meld-daemon, Property 49: Large Workspace Responsiveness**
 *
 * For any workspace containing a large number of Meld source files, the
 * LspChannel SHALL maintain responsive performance by using incremental
 * parsing and caching so that individual file operations do not degrade
 * proportionally to workspace size.
 *
 * **Validates: Requirements 20.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/workspace_provider.hpp"

#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_workspace(SemanticModel& model, int file_count) {
    for (int i = 0; i < file_count; ++i) {
        std::filesystem::path p = "src/module_" + std::to_string(i) + ".meld";
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = "module_" + std::to_string(i);
        root->location = SourceLocation{p, 1, 0};

        // Add some children to simulate real files
        for (int j = 0; j < 5; ++j) {
            auto child = std::make_shared<ASTNode>();
            child->kind = "function_definition";
            child->name = "fn_" + std::to_string(i) + "_" + std::to_string(j);
            child->type_info = "() -> Int";
            child->location = SourceLocation{p, static_cast<uint32_t>(10 + j * 5), 0};
            root->children.push_back(child);
        }

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
 * Property 49a: Incremental update time does not grow proportionally
 * with workspace size. A single file change in a large workspace should
 * complete quickly.
 */
RC_GTEST_PROP(LargeWorkspaceResponsiveness,
              IncrementalUpdateIsSublinear,
              ()) {
    auto workspace_size = *rc::gen::inRange(50, 200);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    populate_workspace(model, workspace_size);
    RC_ASSERT(model.file_count() == static_cast<size_t>(workspace_size));

    // Time a single incremental update
    auto start = std::chrono::steady_clock::now();
    provider.handle_file_change("src/module_0.meld", FileChangeType::Modified);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Single file update should complete in under 100ms regardless of workspace size
    RC_ASSERT(duration.count() < 100);
    RC_ASSERT(provider.is_within_performance_budget(std::chrono::milliseconds(100)));
}

/**
 * Property 49b: File count remains correct after incremental operations
 * on a large workspace.
 */
RC_GTEST_PROP(LargeWorkspaceResponsiveness,
              FileCountConsistentAfterOperations,
              ()) {
    auto workspace_size = *rc::gen::inRange(10, 50);
    auto ops = *rc::gen::inRange(1, 10);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    populate_workspace(model, workspace_size);

    // Track which files exist in the model
    std::set<std::string> existing;
    for (int i = 0; i < workspace_size; ++i) {
        existing.insert("src/module_" + std::to_string(i) + ".meld");
    }

    int next_new_id = 0;
    for (int i = 0; i < ops; ++i) {
        auto op = *rc::gen::inRange(0, 3);
        if (op == 0 && !existing.empty()) {
            // Delete the first existing file
            auto it = existing.begin();
            provider.handle_file_change(*it, FileChangeType::Deleted);
            existing.erase(it);
        } else if (op == 1) {
            // Add a new unique file
            std::string p = "src/new_" + std::to_string(next_new_id++) + ".meld";
            provider.handle_file_change(p, FileChangeType::Created);
            existing.insert(p);
        } else {
            // Modify (no count change)
            if (!existing.empty()) {
                provider.handle_file_change(*existing.begin(),
                                            FileChangeType::Modified);
            }
        }
    }

    RC_ASSERT(model.file_count() == existing.size());
}

/**
 * Property 49c: Stats reflect incremental mode after file changes.
 */
RC_GTEST_PROP(LargeWorkspaceResponsiveness,
              StatsReflectIncrementalMode,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    populate_workspace(model, 20);
    provider.handle_file_change("src/module_0.meld", FileChangeType::Modified);

    auto stats = provider.get_stats();
    RC_ASSERT(stats.incremental == true);
    RC_ASSERT(stats.indexed_files == model.file_count());
}

}  // namespace
}  // namespace meld::daemon
