/**
 * **Feature: meld-daemon, Property 88: MCP Documentation Tool Integration Completeness**
 *
 * For any documentation generation, API documentation and code examples
 * SHALL be generated and updated correctly.
 *
 * **Validates: Requirements 28.5**
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

void add_function(SemanticModel& model, const std::filesystem::path& file,
                  const std::string& name, const std::string& type,
                  const std::vector<std::string>& effects = {}) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto child = std::make_shared<ASTNode>();
    child->kind = "function_definition";
    child->name = name;
    child->type_info = type;
    child->effects = effects;
    child->location = SourceLocation{file, 1, 0};
    root->children.push_back(child);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 88a: Documentation for existing function includes API doc.
 */
RC_GTEST_PROP(McpDocToolProperty,
              ExistingFunctionDocGenerated,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "compute", "(Int) -> Int", {"IO"});

    McpToolProvider provider(model, dep_graph, vi);
    auto doc = provider.generate_documentation(file, "compute");
    RC_ASSERT(doc.up_to_date);
    RC_ASSERT(!doc.api_doc.empty());
    RC_ASSERT(doc.api_doc.find("compute") != std::string::npos);
    RC_ASSERT(!doc.code_examples.empty());
}

/**
 * Property 88b: Documentation for non-existent symbol is not up-to-date.
 */
RC_GTEST_PROP(McpDocToolProperty,
              NonExistentSymbolNotUpToDate,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "existing", "(Int) -> Int");

    McpToolProvider provider(model, dep_graph, vi);
    auto doc = provider.generate_documentation(file, "nonexistent");
    RC_ASSERT(!doc.up_to_date);
}

/**
 * Property 88c: Documentation includes effect information when present.
 */
RC_GTEST_PROP(McpDocToolProperty,
              EffectsIncludedInDoc,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function(model, file, "sideEffect", "() -> Void", {"IO", "Network"});

    McpToolProvider provider(model, dep_graph, vi);
    auto doc = provider.generate_documentation(file, "sideEffect");
    RC_ASSERT(doc.up_to_date);
    RC_ASSERT(doc.api_doc.find("IO") != std::string::npos);
    RC_ASSERT(doc.api_doc.find("Network") != std::string::npos);
}

}  // namespace
}  // namespace meld::daemon
