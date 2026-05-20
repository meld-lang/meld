/**
 * **Feature: meld-daemon, Property 67: MCP Type Signature Search Accuracy**
 *
 * For any type pattern query, all functions and methods with matching
 * signatures SHALL be found.
 *
 * **Validates: Requirements 24.2**
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

void add_typed_functions(SemanticModel& model,
                        const std::filesystem::path& file,
                        const std::vector<std::pair<std::string, std::string>>& fns) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, type_sig] : fns) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->type_info = type_sig;
        child->location = SourceLocation{file, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/**
 * Property 67a: Functions whose type_info contains the search pattern
 * are returned.
 */
RC_GTEST_PROP(McpTypeSignatureSearchProperty,
              MatchingSignaturesFound,
              ()) {
    auto fn_name = *genIdent();
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_typed_functions(model, file, {
        {fn_name, "(Int, String) -> Bool"},
        {"other", "() -> Unit"}
    });

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_type_signature("Int");

    bool found = false;
    for (const auto& r : results)
        if (r.name == fn_name) found = true;
    RC_ASSERT(found);
}

/**
 * Property 67b: A pattern that matches no type signatures returns empty.
 */
RC_GTEST_PROP(McpTypeSignatureSearchProperty,
              NonMatchingPatternReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_typed_functions(model, "test.meld", {{"foo", "() -> Int"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_type_signature("ComplexType_XYZ_999");
    RC_ASSERT(results.empty());
}

/**
 * Property 67c: Empty pattern returns empty results.
 */
RC_GTEST_PROP(McpTypeSignatureSearchProperty,
              EmptyPatternReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    add_typed_functions(model, "test.meld", {{"foo", "() -> Int"}});

    McpToolProvider provider(model, dep_graph, vi);
    auto results = provider.search_type_signature("");
    RC_ASSERT(results.empty());
}

}  // namespace
}  // namespace meld::daemon
