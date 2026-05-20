/**
 * **Feature: meld-daemon, Property 72: MCP Function Generation Dispatch Compatibility**
 *
 * For any generated function implementation, multiple dispatch rules and
 * signature compatibility SHALL be respected.
 *
 * **Validates: Requirements 25.3**
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

rc::Gen<std::string> genFuncName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

/**
 * Property 72a: Function with unique signature is dispatch-compatible.
 */
RC_GTEST_PROP(McpFuncGenDispatchProperty,
              UniqueSignatureIsCompatible,
              ()) {
    auto name = *genFuncName();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_function(name, "(x: Int) -> Int", {"(y: String) -> String"});
    RC_ASSERT(result.dispatch_compatible);
    RC_ASSERT(result.dispatch_errors.empty());
    RC_ASSERT(!result.code.empty());
}

/**
 * Property 72b: Duplicate signature is flagged as incompatible.
 */
RC_GTEST_PROP(McpFuncGenDispatchProperty,
              DuplicateSignatureFlagged,
              ()) {
    auto name = *genFuncName();
    std::string sig = "(x: Int) -> Int";
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_function(name, sig, {sig});
    RC_ASSERT(!result.dispatch_compatible);
    RC_ASSERT(!result.dispatch_errors.empty());
}

/**
 * Property 72c: Invalid function name is rejected.
 */
RC_GTEST_PROP(McpFuncGenDispatchProperty,
              InvalidNameRejected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_function("123bad", "(x: Int) -> Int", {});
    RC_ASSERT(!result.dispatch_compatible);
    RC_ASSERT(!result.dispatch_errors.empty());
}

}  // namespace
}  // namespace meld::daemon
