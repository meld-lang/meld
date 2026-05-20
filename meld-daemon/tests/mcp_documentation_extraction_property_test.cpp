/**
 * **Feature: meld-daemon, Property 60: MCP Documentation Extraction Completeness**
 *
 * For any Meld code with documentation, all inline comments, docstrings,
 * and metadata annotations SHALL be extracted.
 *
 * **Validates: Requirements 22.5**
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

void add_documented_file(SemanticModel& model,
                        const std::filesystem::path& path,
                        const std::vector<std::pair<std::string, std::string>>& docs) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& [name, type_info] : docs) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->type_info = type_info;
        child->location = SourceLocation{path, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = path;
    sem.ast = root;
    model.update_file(path, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 60a: Every symbol with non-empty type_info produces a
 * documentation entry.
 */
RC_GTEST_PROP(McpDocumentationExtractionProperty,
              DocumentedSymbolsAreExtracted,
              ()) {
    auto sym_name = *genIdent();
    auto type_info = *genIdent();

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_documented_file(model, file, {{sym_name, type_info}});

    McpToolProvider provider(model, dep_graph, vi);
    auto docs = provider.extract_documentation(file);

    bool found = false;
    for (const auto& entry : docs)
        if (entry.symbol_name == sym_name) found = true;
    RC_ASSERT(found);
}

/**
 * Property 60b: Symbols with effect annotations produce documentation
 * entries containing the annotation kind.
 */
RC_GTEST_PROP(McpDocumentationExtractionProperty,
              EffectAnnotationsExtracted,
              ()) {
    auto sym_name = *genIdent();

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";

    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    auto child = std::make_shared<ASTNode>();
    child->kind = "function_definition";
    child->name = sym_name;
    child->type_info = "";
    child->effects = {"IO", "Network"};
    child->location = SourceLocation{file, 1, 0};
    root->children.push_back(child);
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));

    McpToolProvider provider(model, dep_graph, vi);
    auto docs = provider.extract_documentation(file);

    bool found = false;
    for (const auto& entry : docs)
        if (entry.symbol_name == sym_name &&
            entry.doc_text.find("@effects") != std::string::npos)
            found = true;
    RC_ASSERT(found);
}

/**
 * Property 60c: A file with no documented symbols returns empty.
 */
RC_GTEST_PROP(McpDocumentationExtractionProperty,
              UndocumentedFileReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    // Add a file with a symbol that has no type_info and no effects
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    auto child = std::make_shared<ASTNode>();
    child->kind = "function_definition";
    child->name = "bare_fn";
    child->type_info = "";
    child->location = SourceLocation{"test.meld", 1, 0};
    root->children.push_back(child);
    FileSemantics sem;
    sem.path = "test.meld";
    sem.ast = root;
    model.update_file("test.meld", std::move(sem));

    McpToolProvider provider(model, dep_graph, vi);
    auto docs = provider.extract_documentation("test.meld");
    RC_ASSERT(docs.empty());
}

}  // namespace
}  // namespace meld::daemon
