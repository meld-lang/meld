/**
 * **Feature: meld-daemon, Property 73: MCP Module Generation Consistency**
 *
 * For any generated complete module, import/export consistency and dependency
 * requirements SHALL be validated.
 *
 * **Validates: Requirements 25.4**
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

rc::Gen<std::string> genModuleName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

/**
 * Property 73a: Module with distinct imports is consistent.
 */
RC_GTEST_PROP(McpModuleGenConsistencyProperty,
              DistinctImportsAreConsistent,
              ()) {
    auto name = *genModuleName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_module(name, {"std.io", "std.math"}, {"foo"});
    RC_ASSERT(result.import_export_consistent);
    RC_ASSERT(result.dependency_errors.empty());
    RC_ASSERT(!result.code.empty());
}

/**
 * Property 73b: Self-import is detected as inconsistent.
 */
RC_GTEST_PROP(McpModuleGenConsistencyProperty,
              SelfImportDetected,
              ()) {
    auto name = *genModuleName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_module(name, {name}, {"foo"});
    RC_ASSERT(!result.import_export_consistent);
    RC_ASSERT(!result.dependency_errors.empty());
}

/**
 * Property 73c: Duplicate imports are detected.
 */
RC_GTEST_PROP(McpModuleGenConsistencyProperty,
              DuplicateImportsDetected,
              ()) {
    auto name = *genModuleName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_module(name, {"std.io", "std.io"}, {"foo"});
    RC_ASSERT(!result.import_export_consistent);
    RC_ASSERT(!result.dependency_errors.empty());
}

}  // namespace
}  // namespace meld::daemon
