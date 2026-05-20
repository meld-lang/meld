/**
 * **Feature: meld-daemon, Property 81: MCP Project Structure Exposure Accuracy**
 *
 * For any project structure, directory organization, module hierarchies,
 * and file relationships SHALL be exposed correctly.
 *
 * **Validates: Requirements 27.2**
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

void add_module_file(SemanticModel& model, const std::filesystem::path& file,
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
 * Property 81a: Each indexed file with a module name appears in the hierarchy.
 */
RC_GTEST_PROP(McpProjectStructureProperty,
              ModulesAppearInHierarchy,
              ()) {
    auto n_files = *rc::gen::inRange(1, 4);
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    for (int i = 0; i < n_files; ++i) {
        std::filesystem::path f = "src/mod_" + std::to_string(i) + ".meld";
        add_module_file(model, f, "mod_" + std::to_string(i));
    }

    McpToolProvider provider(model, dep_graph, vi);
    auto structure = provider.get_project_structure();
    RC_ASSERT(static_cast<int>(structure.module_hierarchy.size()) == n_files);
    RC_ASSERT(static_cast<int>(structure.file_relationships.size()) == n_files);
}

/**
 * Property 81b: Empty model produces empty structure.
 */
RC_GTEST_PROP(McpProjectStructureProperty,
              EmptyModelEmptyStructure,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto structure = provider.get_project_structure();
    RC_ASSERT(structure.module_hierarchy.empty());
    RC_ASSERT(structure.directories.empty());
}

}  // namespace
}  // namespace meld::daemon
