/**
 * **Feature: meld-daemon, Property 32: Type Error Reporting**
 *
 * For any code with type mismatches, the LspChannel SHALL report clear type
 * errors with descriptions and suggested fixes.
 *
 * **Validates: Requirements 17.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/diagnostics_provider.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genTypeName() {
    return rc::gen::elementOf(
        std::vector<std::string>{"Int", "Float", "String", "Bool"});
}

/// Generate a pair of distinct types to create a mismatch.
rc::Gen<std::pair<std::string, std::string>> genTypeMismatch() {
    return rc::gen::suchThat(
        rc::gen::pair(genTypeName(), genTypeName()),
        [](const std::pair<std::string, std::string>& p) {
            return p.first != p.second;
        }
    );
}

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        }
    );
}

/// Build a SemanticModel with a function that has a return type mismatch.
void populate_with_type_mismatch(SemanticModel& model,
                                  const std::filesystem::path& file,
                                  const std::string& func_name,
                                  const std::string& declared_type,
                                  const std::string& actual_type) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    auto func = std::make_shared<ASTNode>();
    func->kind = "function_definition";
    func->name = func_name;
    func->type_info = declared_type;
    func->location = {file, 5, 0};

    auto ret = std::make_shared<ASTNode>();
    ret->kind = "return_statement";
    ret->type_info = actual_type;
    ret->location = {file, 6, 4};
    func->children.push_back(ret);

    root->children.push_back(func);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 32a: make_type_error always produces a diagnostic with both
 * expected and actual types in the message, plus a non-empty suggestion.
 */
RC_GTEST_PROP(TypeErrorReportingProperty,
              TypeErrorContainsTypesAndSuggestion,
              ()) {
    auto [expected, actual] = *genTypeMismatch();
    auto context = *genIdentifier();

    auto diag = DiagnosticsProvider::make_type_error(
        "test.meld", 10, 5, expected, actual, context);

    RC_ASSERT(diag.message.find(expected) != std::string::npos);
    RC_ASSERT(diag.message.find(actual) != std::string::npos);
    RC_ASSERT(!diag.suggestion.empty());
    RC_ASSERT(diag.code == "E1001");
    RC_ASSERT(diag.severity == TypeDiagnosticSeverity::Error);
}

/**
 * Property 32b: A function with mismatched return type always produces
 * at least one type error diagnostic.
 */
RC_GTEST_PROP(TypeErrorReportingProperty,
              ReturnTypeMismatchDetected,
              ()) {
    auto [declared, actual] = *genTypeMismatch();
    auto func_name = *genIdentifier();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_type_mismatch(model, file, func_name, declared, actual);

    DiagnosticsProvider provider(model);
    auto result = provider.check_types(file);

    RC_ASSERT(!result.errors.empty());
    // The error should mention both types
    bool found = std::any_of(result.errors.begin(), result.errors.end(),
        [&](const TypeDiagnostic& d) {
            return d.message.find(declared) != std::string::npos &&
                   d.message.find(actual) != std::string::npos;
        });
    RC_ASSERT(found);
}

/**
 * Property 32c: A function with matching return type produces no type errors.
 */
RC_GTEST_PROP(TypeErrorReportingProperty,
              MatchingReturnTypeNoErrors,
              ()) {
    auto type = *genTypeName();
    auto func_name = *genIdentifier();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_type_mismatch(model, file, func_name, type, type);

    DiagnosticsProvider provider(model);
    auto result = provider.check_types(file);

    RC_ASSERT(result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
