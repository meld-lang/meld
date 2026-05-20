/**
 * **Feature: meld-daemon, Property 74: MCP Generated Code Formatting Compliance**
 *
 * For any generated code, standard Meld style conventions and formatting
 * rules SHALL be applied.
 *
 * **Validates: Requirements 25.5**
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

/**
 * Property 74a: Non-empty code is formatted successfully.
 */
RC_GTEST_PROP(McpCodeFormattingComplianceProperty,
              NonEmptyCodeFormatted,
              ()) {
    auto code = *rc::gen::nonEmpty<std::string>();
    RC_PRE(code.find('\0') == std::string::npos);

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.format_code(code);
    RC_ASSERT(result.formatting_applied);
    RC_ASSERT(!result.code.empty());
}

/**
 * Property 74b: Empty code is not formatted.
 */
RC_GTEST_PROP(McpCodeFormattingComplianceProperty,
              EmptyCodeNotFormatted,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.format_code("");
    RC_ASSERT(!result.formatting_applied);
    RC_ASSERT(!result.style_warnings.empty());
}

/**
 * Property 74c: Code with tabs produces style warning.
 */
RC_GTEST_PROP(McpCodeFormattingComplianceProperty,
              TabsProduceWarning,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    McpToolProvider provider(model, dep_graph, vi);

    auto result = provider.format_code("fnc foo() {\n\treturn 1\n}\n");
    RC_ASSERT(result.formatting_applied);
    RC_ASSERT(!result.style_warnings.empty());
}

}  // namespace
}  // namespace meld::daemon
