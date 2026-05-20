/**
 * **Feature: meld-daemon, Property 62: MCP Type Analysis Accuracy**
 *
 * For any Meld type system constructs, refinement types, multiple dispatch
 * signatures, and type constraints SHALL be validated correctly.
 *
 * **Validates: Requirements 23.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

VectorIndex make_vector_index() {
    return VectorIndex(std::make_shared<PassthroughEmbeddingProvider>());
}

rc::Gen<std::string> genIdent() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

void add_typed_symbol(SemanticModel& model, const std::filesystem::path& file,
                     const std::string& name, const std::string& kind,
                     const std::string& type_info) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto node = std::make_shared<ASTNode>();
    node->kind = kind;
    node->name = name;
    node->type_info = type_info;
    node->location = SourceLocation{file, 1, 0};
    root->children.push_back(node);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 62a: A refinement type node is detected as having refinement.
 */
RC_GTEST_PROP(McpTypeAnalysisProperty,
              RefinementTypeDetected,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_typed_symbol(model, file, name, "refinement_type", "Int where x > 0");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_type(file, name);
    RC_ASSERT(result.symbol_name == name);
    RC_ASSERT(result.has_refinement);
}

/**
 * Property 62b: A multiple dispatch node is detected as having dispatch.
 */
RC_GTEST_PROP(McpTypeAnalysisProperty,
              MultipleDispatchDetected,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_typed_symbol(model, file, name, "multiple_dispatch", "(Int, String) -> Bool");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_type(file, name);
    RC_ASSERT(result.symbol_name == name);
    RC_ASSERT(result.has_dispatch);
}

/**
 * Property 62c: A regular function has neither refinement nor dispatch flags.
 */
RC_GTEST_PROP(McpTypeAnalysisProperty,
              RegularFunctionHasNoSpecialFlags,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_typed_symbol(model, file, name, "function_definition", "() -> Int");

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_type(file, name);
    RC_ASSERT(!result.has_refinement);
    RC_ASSERT(!result.has_dispatch);
}

}  // namespace
}  // namespace meld::daemon
