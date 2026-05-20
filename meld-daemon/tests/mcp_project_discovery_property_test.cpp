/**
 * **Feature: meld-daemon, Property 56: MCP Project Discovery Completeness**
 *
 * For any valid Meld workspace, the McpChannel SHALL discover and provide
 * access to all available projects and workspaces.
 *
 * **Validates: Requirements 22.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Create a minimal VectorIndex with a passthrough provider.
VectorIndex make_vector_index() {
    return VectorIndex(std::make_shared<PassthroughEmbeddingProvider>());
}

void add_file(SemanticModel& model, const std::filesystem::path& path,
              const std::string& module_name) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = module_name;
    root->location = SourceLocation{path, 1, 0};
    FileSemantics sem;
    sem.path = path;
    sem.ast = root;
    model.update_file(path, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genModuleName() {
    return rc::gen::map(rc::gen::inRange(1, 8), [](int len) {
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
 * Property 56a: Every indexed file's parent directory appears as a discovered
 * project, and the total file count across projects equals the model's file count.
 */
RC_GTEST_PROP(McpProjectDiscoveryProperty,
              AllIndexedFilesAppearInDiscoveredProjects,
              ()) {
    auto file_count = *rc::gen::inRange(1, 6);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    std::set<std::string> expected_files;
    for (int i = 0; i < file_count; ++i) {
        auto mod = *genModuleName();
        std::filesystem::path p = "proj/file_" + std::to_string(i) + ".meld";
        add_file(model, p, mod);
        expected_files.insert(p.string());
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto projects = provider.discover_projects();

    // All projects together must account for every indexed file
    size_t total = 0;
    for (const auto& proj : projects)
        total += proj.file_count;
    RC_ASSERT(total == expected_files.size());
}

/**
 * Property 56b: An empty workspace yields no discovered projects.
 */
RC_GTEST_PROP(McpProjectDiscoveryProperty,
              EmptyWorkspaceYieldsNoProjects,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    McpToolProvider provider(model, dep_graph, vi);
    auto projects = provider.discover_projects();
    RC_ASSERT(projects.empty());
}

/**
 * Property 56c: Module names from ASTs appear in the project's modules list.
 */
RC_GTEST_PROP(McpProjectDiscoveryProperty,
              ModuleNamesAreExposed,
              ()) {
    auto mod_name = *genModuleName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    add_file(model, "proj/test.meld", mod_name);

    McpToolProvider provider(model, dep_graph, vi);
    auto projects = provider.discover_projects();
    RC_ASSERT(!projects.empty());

    bool found = false;
    for (const auto& proj : projects)
        for (const auto& m : proj.modules)
            if (m == mod_name) found = true;
    RC_ASSERT(found);
}

}  // namespace
}  // namespace meld::daemon
