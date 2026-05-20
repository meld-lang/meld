/**
 * **Feature: meld-daemon, Property 70: MCP Code Generation Syntax Validation**
 *
 * For any generated code snippet, syntax SHALL be validated according to
 * Meld grammar rules.
 *
 * **Validates: Requirements 25.1**
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

rc::Gen<std::string> genDescription() {
    return rc::gen::map(rc::gen::inRange(1, 8), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

/**
 * Property 70a: Generated snippets with valid descriptions have syntax validated.
 */
RC_GTEST_PROP(McpCodeGenSyntaxProperty,
              ValidDescriptionProducesValidSnippet,
              ()) {
    auto desc = *genDescription();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_snippet(desc, "");
    RC_ASSERT(result.syntax_valid);
    RC_ASSERT(!result.code.empty());
    RC_ASSERT(result.grammar_errors.empty());
}

/**
 * Property 70b: Empty description produces invalid snippet.
 */
RC_GTEST_PROP(McpCodeGenSyntaxProperty,
              EmptyDescriptionProducesInvalid,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_snippet("", "");
    RC_ASSERT(!result.syntax_valid);
    RC_ASSERT(!result.grammar_errors.empty());
}

/**
 * Property 70c: Snippet with unbalanced context is detected.
 */
RC_GTEST_PROP(McpCodeGenSyntaxProperty,
              UnbalancedContextDetected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.generate_snippet("test", "fnc foo() {");
    RC_ASSERT(!result.syntax_valid);
    RC_ASSERT(!result.grammar_errors.empty());
}

}  // namespace
}  // namespace meld::daemon
