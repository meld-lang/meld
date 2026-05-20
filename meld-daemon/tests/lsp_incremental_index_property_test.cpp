/**
 * **Feature: meld-daemon, Property 47: LSP Incremental Index Updates**
 *
 * For any file system change (add, modify, delete), the workspace index
 * SHALL be updated incrementally to reflect the change.
 *
 * **Validates: Requirements 20.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/workspace_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void seed_model(SemanticModel& model, const std::vector<std::string>& names) {
    for (const auto& name : names) {
        std::filesystem::path p = name + ".meld";
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = name;
        root->location = SourceLocation{p, 1, 0};
        FileSemantics sem;
        sem.path = p;
        sem.ast = root;
        model.update_file(p, std::move(sem));
    }
}

rc::Gen<std::string> genModuleName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 47a: Adding a file increments the index count by one.
 */
RC_GTEST_PROP(LspIncrementalIndex,
              AddFileIncrementsCount,
              ()) {
    auto initial_count = *rc::gen::inRange(0, 5);
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    std::vector<std::string> names;
    for (int i = 0; i < initial_count; ++i) {
        names.push_back("mod" + std::to_string(i));
    }
    seed_model(model, names);

    size_t before = model.file_count();
    RC_ASSERT(before == static_cast<size_t>(initial_count));

    std::filesystem::path new_file = "new_module.meld";
    provider.handle_file_change(new_file, FileChangeType::Created);

    RC_ASSERT(model.file_count() == before + 1);
    RC_ASSERT(model.has_file(new_file));
}

/**
 * Property 47b: Deleting a file decrements the index count by one.
 */
RC_GTEST_PROP(LspIncrementalIndex,
              DeleteFileDecrementsCount,
              ()) {
    auto initial_count = *rc::gen::inRange(1, 6);
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    std::vector<std::string> names;
    for (int i = 0; i < initial_count; ++i) {
        names.push_back("mod" + std::to_string(i));
    }
    seed_model(model, names);

    size_t before = model.file_count();
    std::filesystem::path to_delete = names[0] + ".meld";
    provider.handle_file_change(to_delete, FileChangeType::Deleted);

    RC_ASSERT(model.file_count() == before - 1);
    RC_ASSERT(!model.has_file(to_delete));
}

/**
 * Property 47c: Modifying a file keeps the count the same but updates content.
 */
RC_GTEST_PROP(LspIncrementalIndex,
              ModifyFileKeepsCount,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    seed_model(model, {"alpha", "beta"});
    size_t before = model.file_count();

    provider.handle_file_change("alpha.meld", FileChangeType::Modified);

    RC_ASSERT(model.file_count() == before);
    RC_ASSERT(model.has_file("alpha.meld"));
}

/**
 * Property 47d: Incremental updates set the incremental flag in stats.
 */
RC_GTEST_PROP(LspIncrementalIndex,
              IncrementalFlagSet,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    provider.handle_file_change("test.meld", FileChangeType::Created);
    auto stats = provider.get_stats();
    RC_ASSERT(stats.incremental == true);
}

}  // namespace
}  // namespace meld::daemon
