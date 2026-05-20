/**
 * **Feature: meld-daemon, Property 63: MCP Control Flow Analysis Completeness**
 *
 * For any Meld control flow patterns, pattern matching and functional
 * constructs SHALL be analyzed correctly.
 *
 * **Validates: Requirements 23.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <algorithm>
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

void add_function_with_children(SemanticModel& model,
                               const std::filesystem::path& file,
                               const std::string& fn_name,
                               const std::vector<std::string>& child_kinds) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto fn = std::make_shared<ASTNode>();
    fn->kind = "function_definition";
    fn->name = fn_name;
    fn->location = SourceLocation{file, 1, 0};
    for (const auto& ck : child_kinds) {
        auto c = std::make_shared<ASTNode>();
        c->kind = ck;
        c->name = ck + "_node";
        fn->children.push_back(c);
    }
    root->children.push_back(fn);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 63a: A function containing pattern_match children has
 * has_pattern_match set to true.
 */
RC_GTEST_PROP(McpControlFlowAnalysisProperty,
              PatternMatchDetected,
              ()) {
    auto fn_name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function_with_children(model, file, fn_name, {"pattern_match"});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_control_flow(file, fn_name);
    RC_ASSERT(result.function_name == fn_name);
    RC_ASSERT(result.has_pattern_match);
}

/**
 * Property 63b: A function containing functional constructs (map, filter,
 * fold, pipe) has has_functional_constructs set to true.
 */
RC_GTEST_PROP(McpControlFlowAnalysisProperty,
              FunctionalConstructsDetected,
              ()) {
    auto fn_name = *genIdent();
    auto construct = *rc::gen::elementOf(
        std::vector<std::string>{"map", "filter", "fold", "pipe"});

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function_with_children(model, file, fn_name, {construct});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_control_flow(file, fn_name);
    RC_ASSERT(result.has_functional_constructs);
    RC_ASSERT(std::find(result.constructs.begin(), result.constructs.end(),
                        construct) != result.constructs.end());
}

/**
 * Property 63c: A function with no control flow children has both flags false.
 */
RC_GTEST_PROP(McpControlFlowAnalysisProperty,
              NoControlFlowYieldsEmptyAnalysis,
              ()) {
    auto fn_name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_function_with_children(model, file, fn_name, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.analyze_control_flow(file, fn_name);
    RC_ASSERT(!result.has_pattern_match);
    RC_ASSERT(!result.has_functional_constructs);
}

}  // namespace
}  // namespace meld::daemon
