/**
 * **Feature: meld-daemon, Property 86: MCP Compiler Output Access Completeness**
 *
 * For any compilation result, errors, warnings, and diagnostic information
 * SHALL be provided.
 *
 * **Validates: Requirements 28.2**
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
 * Property 86a: Errors and warnings are separated correctly.
 */
RC_GTEST_PROP(McpCompilerOutputProperty,
              ErrorsAndWarningsSeparated,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";

    Diagnostic err;
    err.severity = DiagnosticSeverity::Error;
    err.rule_id = "type-error";
    err.message = "type mismatch";
    err.location = SourceLocation{file, 1, 0};

    Diagnostic warn;
    warn.severity = DiagnosticSeverity::Warning;
    warn.rule_id = "unused-var";
    warn.message = "unused variable";
    warn.location = SourceLocation{file, 2, 0};

    add_file_with_diagnostics(model, file, {err, warn});

    McpToolProvider provider(model, dep_graph, vi);
    auto output = provider.get_compiler_output(file);
    RC_ASSERT(output.errors.size() == 1);
    RC_ASSERT(output.warnings.size() == 1);
    RC_ASSERT(!output.human_summary.empty());
    RC_ASSERT(!output.agent_context.empty());
}

/**
 * Property 86b: Clean file produces zero errors and warnings.
 */
RC_GTEST_PROP(McpCompilerOutputProperty,
              CleanFileNoErrors,
              ()) {
    SemanticModel model;
    DependencyGraph dep_graph;
    auto vi = make_vector_index();
    std::filesystem::path file = "test.meld";
    add_file_with_diagnostics(model, file, {});

    McpToolProvider provider(model, dep_graph, vi);
    auto output = provider.get_compiler_output(file);
    RC_ASSERT(output.errors.empty());
    RC_ASSERT(output.warnings.empty());
}

}  // namespace
}  // namespace meld::daemon
