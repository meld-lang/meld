/**
 * **Feature: meld-daemon, Property 78: MCP Design Pattern Application Correctness**
 *
 * For any design pattern transformation, code behavior SHALL be preserved
 * while maintaining Meld idioms.
 *
 * **Validates: Requirements 26.4**
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

void add_file(SemanticModel& model, const std::filesystem::path& file) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "testmod";
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

rc::Gen<std::string> genSupportedPattern() {
    return rc::gen::element<std::string>(
        "observer", "strategy", "factory", "builder", "visitor");
}

/**
 * Property 78a: Supported pattern preserves behavior and is idiomatic.
 */
RC_GTEST_PROP(McpPatternApplicationProperty,
              SupportedPatternPreservesBehavior,
              ()) {
    auto pattern = *genSupportedPattern();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file(model, file);

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.apply_pattern(file, pattern);
    RC_ASSERT(result.behavior_preserved);
    RC_ASSERT(result.meld_idiomatic);
    RC_ASSERT(result.errors.empty());
    RC_ASSERT(!result.transformed_code.empty());
}

/**
 * Property 78b: Unsupported pattern is rejected.
 */
RC_GTEST_PROP(McpPatternApplicationProperty,
              UnsupportedPatternRejected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file(model, file);

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.apply_pattern(file, "nonexistent_pattern_xyz");
    RC_ASSERT(!result.behavior_preserved);
    RC_ASSERT(!result.errors.empty());
}

/**
 * Property 78c: Pattern on non-existent file fails.
 */
RC_GTEST_PROP(McpPatternApplicationProperty,
              NonExistentFileFails,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.apply_pattern("missing.meld", "observer");
    RC_ASSERT(!result.behavior_preserved);
    RC_ASSERT(!result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
