/**
 * **Feature: meld-daemon, Property 71: MCP Type Definition Generation Correctness**
 *
 * For any generated type definition, type safety and constraint satisfaction
 * SHALL be ensured.
 *
 * **Validates: Requirements 25.2**
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

rc::Gen<std::string> genTypeName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        s += static_cast<char>('A' + (len % 26));
        for (int i = 1; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

/**
 * Property 71a: Valid type name produces type-safe definition.
 */
RC_GTEST_PROP(McpTypeDefGenProperty,
              ValidTypeNameProducesTypeSafe,
              ()) {
    auto name = *genTypeName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_type_definition(name, {"x: Int"}, {});
    RC_ASSERT(result.type_safe);
    RC_ASSERT(result.type_name == name);
    RC_ASSERT(!result.code.empty());
    RC_ASSERT(result.constraint_errors.empty());
}

/**
 * Property 71b: Keyword as type name is rejected.
 */
RC_GTEST_PROP(McpTypeDefGenProperty,
              KeywordTypeNameRejected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_type_definition("struct", {"x: Int"}, {});
    RC_ASSERT(!result.type_safe);
    RC_ASSERT(!result.constraint_errors.empty());
}

/**
 * Property 71c: Empty constraint is flagged.
 */
RC_GTEST_PROP(McpTypeDefGenProperty,
              EmptyConstraintFlagged,
              ()) {
    auto name = *genTypeName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_type_definition(name, {"x: Int"}, {""});
    RC_ASSERT(!result.type_safe);
    RC_ASSERT(!result.constraint_errors.empty());
}

}  // namespace
}  // namespace meld::daemon
