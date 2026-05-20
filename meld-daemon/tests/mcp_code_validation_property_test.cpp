/**
 * **Feature: meld-daemon, Property 65: MCP Code Validation Completeness**
 *
 * For any invalid Meld code, syntax errors, type mismatches, and semantic
 * violations SHALL be identified.
 *
 * **Validates: Requirements 23.5**
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

void add_file_with_diagnostics(SemanticModel& model,
                               const std::filesystem::path& file,
                               const std::vector<Diagnostic>& diags) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    sem.diagnostics = diags;
    model.update_file(file, std::move(sem));
}

/**
 * Property 65a: A file with syntax error diagnostics is reported as invalid
 * with syntax_errors populated.
 */
RC_GTEST_PROP(McpCodeValidationProperty,
              SyntaxErrorsDetected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";

    Diagnostic d;
    d.severity = DiagnosticSeverity::Error;
    d.rule_id = "syntax-error";
    d.message = "unexpected token";
    d.location = SourceLocation{file, 1, 0};
    add_file_with_diagnostics(model, file, {d});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.validate_code(file);
    RC_ASSERT(!result.valid);
    RC_ASSERT(!result.syntax_errors.empty());
}

/**
 * Property 65b: A file with type error diagnostics is reported as invalid
 * with type_errors populated.
 */
RC_GTEST_PROP(McpCodeValidationProperty,
              TypeErrorsDetected,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";

    Diagnostic d;
    d.severity = DiagnosticSeverity::Error;
    d.rule_id = "type-mismatch";
    d.message = "expected Int, got String";
    d.location = SourceLocation{file, 1, 0};
    add_file_with_diagnostics(model, file, {d});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.validate_code(file);
    RC_ASSERT(!result.valid);
    RC_ASSERT(!result.type_errors.empty());
}

/**
 * Property 65c: A file with no diagnostics is reported as valid.
 */
RC_GTEST_PROP(McpCodeValidationProperty,
              CleanFileIsValid,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file_with_diagnostics(model, file, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto result = provider.validate_code(file);
    RC_ASSERT(result.valid);
    RC_ASSERT(result.syntax_errors.empty());
    RC_ASSERT(result.type_errors.empty());
    RC_ASSERT(result.semantic_errors.empty());
}

}  // namespace
}  // namespace meld::daemon
