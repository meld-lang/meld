/**
 * **Feature: meld-daemon, Property 64: MCP Macro Processing Accuracy**
 *
 * For any macro definition, meta-macro system constructs and expansion
 * rules SHALL be understood correctly.
 *
 * **Validates: Requirements 23.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/mcp_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <memory>
#include <set>
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

void add_macro(SemanticModel& model, const std::filesystem::path& file,
               const std::string& name, const std::string& kind,
               const std::string& type_info,
               const std::vector<std::string>& params) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    auto node = std::make_shared<ASTNode>();
    node->kind = kind;
    node->name = name;
    node->type_info = type_info;
    node->location = SourceLocation{file, 1, 0};
    for (const auto& p : params) {
        auto param = std::make_shared<ASTNode>();
        param->kind = "parameter";
        param->name = p;
        node->children.push_back(param);
    }
    root->children.push_back(node);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 64a: A macro_definition node is processed as valid with
 * correct parameters.
 */
RC_GTEST_PROP(McpMacroProcessingProperty,
              MacroDefinitionIsValid,
              ()) {
    auto name = *genIdent();
    auto raw_params = *rc::gen::container<std::vector<std::string>>(3, genIdent());
    std::vector<std::string> params;
    std::set<std::string> seen_p;
    for (auto& s : raw_params)
        if (seen_p.insert(s).second) params.push_back(s);

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_macro(model, file, name, "macro_definition", "expanded_form", params);

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.process_macro(file, name);
    RC_ASSERT(result.valid);
    RC_ASSERT(result.macro_name == name);
    RC_ASSERT(result.parameters.size() == params.size());
}

/**
 * Property 64b: A non-macro node is processed as invalid.
 */
RC_GTEST_PROP(McpMacroProcessingProperty,
              NonMacroNodeIsInvalid,
              ()) {
    auto name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_macro(model, file, name, "function_definition", "() -> Int", {});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.process_macro(file, name);
    RC_ASSERT(!result.valid);
}

/**
 * Property 64c: A macro with type_info produces an expansion hint.
 */
RC_GTEST_PROP(McpMacroProcessingProperty,
              MacroWithTypeInfoHasExpansionHint,
              ()) {
    auto name = *genIdent();
    auto expansion = *genIdent();

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_macro(model, file, name, "macro", expansion, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.process_macro(file, name);
    RC_ASSERT(result.valid);
    RC_ASSERT(!result.expansion_hint.empty());
}

}  // namespace
}  // namespace meld::daemon
