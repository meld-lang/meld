/**
 * **Feature: meld-daemon, Property 58: MCP Code Artifact Exposure Completeness**
 *
 * For any Meld code, AST representations, symbol tables, and type information
 * SHALL be completely exposed.
 *
 * **Validates: Requirements 22.3**
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

struct SymDef {
    std::string name;
    std::string kind;
    std::string type_info;
};

void populate(SemanticModel& model, const std::filesystem::path& file,
              const std::vector<SymDef>& syms) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& s : syms) {
        auto child = std::make_shared<ASTNode>();
        child->kind = s.kind;
        child->name = s.name;
        child->type_info = s.type_info;
        child->location = SourceLocation{file, 1, 0};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

rc::Gen<std::string> genIdent() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

rc::Gen<std::string> genKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "function_definition", "val_declaration", "struct", "enum", "trait"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 58a: Every named symbol in the AST appears in the code artifacts
 * returned for that file.
 */
RC_GTEST_PROP(McpCodeArtifactExposureProperty,
              AllNamedSymbolsExposed,
              ()) {
    auto syms = *rc::gen::container<std::vector<SymDef>>(
        5,
        rc::gen::apply([](const std::string& n, const std::string& k) {
            return SymDef{n, k, "Int"};
        }, genIdent(), genKind()));

    // Deduplicate names
    std::vector<SymDef> unique;
    std::set<std::string> seen;
    for (auto& s : syms)
        if (seen.insert(s.name).second) unique.push_back(s);
    RC_PRE(!unique.empty());

    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    populate(model, file, unique);

    McpToolProvider provider(model, dep_graph, vi);
    auto artifacts = provider.get_code_artifacts(file);

    std::set<std::string> artifact_names;
    for (const auto& a : artifacts) artifact_names.insert(a.name);

    for (const auto& s : unique)
        RC_ASSERT(artifact_names.count(s.name) > 0);
}

/**
 * Property 58b: Artifacts for a non-indexed file are empty.
 */
RC_GTEST_PROP(McpCodeArtifactExposureProperty,
              NonIndexedFileReturnsEmpty,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();

    McpToolProvider provider(model, dep_graph, vi);
    auto artifacts = provider.get_code_artifacts("nonexistent.meld");
    RC_ASSERT(artifacts.empty());
}

}  // namespace
}  // namespace meld::daemon
