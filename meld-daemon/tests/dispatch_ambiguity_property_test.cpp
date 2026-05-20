/**
 * **Feature: meld-daemon, Property 35: Dispatch Ambiguity Detection**
 *
 * For any ambiguous multiple dispatch function call, the LspChannel SHALL
 * detect and report dispatch resolution errors.
 *
 * **Validates: Requirements 17.5**
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

rc::Gen<std::string> genConcreteType() {
    return rc::gen::elementOf(
        std::vector<std::string>{"Int", "Float", "String", "Bool"});
}

/// Generate a pair of identical signatures (always ambiguous for matching args).
rc::Gen<std::pair<std::string, std::string>> genAmbiguousSignatures() {
    return rc::gen::map(
        rc::gen::pair(genConcreteType(), genConcreteType()),
        [](const std::pair<std::string, std::string>& types) {
            std::string sig = "(" + types.first + ", " + types.second + ") -> Unit";
            return std::make_pair(sig, sig);
        }
    );
}

/// Generate two signatures that are NOT ambiguous (different arities).
rc::Gen<std::pair<std::string, std::string>> genNonAmbiguousSignatures() {
    return rc::gen::map(
        genConcreteType(),
        [](const std::string& t) {
            std::string sig_a = "(" + t + ") -> Unit";
            std::string sig_b = "(" + t + ", " + t + ") -> Unit";
            return std::make_pair(sig_a, sig_b);
        }
    );
}

/// Build a model with ambiguous overloads and a call expression.
void populate_with_dispatch(SemanticModel& model,
                             const std::filesystem::path& file,
                             const std::string& func_name,
                             const std::vector<std::string>& signatures,
                             const std::vector<std::string>& arg_types) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    // Add function overloads
    for (const auto& sig : signatures) {
        auto func = std::make_shared<ASTNode>();
        func->kind = "function_definition";
        func->name = func_name;
        func->type_info = sig;
        root->children.push_back(func);
    }

    // Add a call expression
    auto call = std::make_shared<ASTNode>();
    call->kind = "call_expression";
    call->name = func_name;
    call->location = {file, 10, 0};

    for (const auto& arg_type : arg_types) {
        auto arg = std::make_shared<ASTNode>();
        arg->kind = "argument";
        arg->type_info = arg_type;
        call->children.push_back(arg);
    }

    root->children.push_back(call);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 35a: Two identical signatures are always ambiguous for matching args.
 */
RC_GTEST_PROP(DispatchAmbiguityProperty,
              IdenticalSignaturesAreAmbiguous,
              ()) {
    auto [sig_a, sig_b] = *genAmbiguousSignatures();
    auto arg1 = *genConcreteType();
    auto arg2 = *genConcreteType();

    // Parse the param types from sig_a to use as arg_types
    // sig_a is "(Type1, Type2) -> Unit"
    auto paren_start = sig_a.find('(');
    auto paren_end = sig_a.find(')');
    std::string inner = sig_a.substr(paren_start + 1, paren_end - paren_start - 1);
    std::vector<std::string> arg_types;
    size_t comma = inner.find(',');
    if (comma != std::string::npos) {
        std::string t1 = inner.substr(0, comma);
        std::string t2 = inner.substr(comma + 2);  // skip ", "
        arg_types = {t1, t2};
    }

    if (!arg_types.empty()) {
        bool ambiguous = DiagnosticsProvider::signatures_ambiguous(
            sig_a, sig_b, arg_types);
        RC_ASSERT(ambiguous);
    }
}

/**
 * Property 35b: Signatures with different arities are never ambiguous.
 */
RC_GTEST_PROP(DispatchAmbiguityProperty,
              DifferentAritiesNotAmbiguous,
              ()) {
    auto [sig_a, sig_b] = *genNonAmbiguousSignatures();
    auto arg = *genConcreteType();

    // Use single arg (matches sig_a arity but not sig_b)
    bool ambiguous = DiagnosticsProvider::signatures_ambiguous(
        sig_a, sig_b, {arg});
    RC_ASSERT(!ambiguous);
}

/**
 * Property 35c: Ambiguous overloads in the model are detected and reported.
 */
RC_GTEST_PROP(DispatchAmbiguityProperty,
              AmbiguousOverloadsDetected,
              ()) {
    auto type1 = *genConcreteType();
    auto type2 = *genConcreteType();
    std::string sig = "(" + type1 + ", " + type2 + ") -> Unit";

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_dispatch(model, file, "process",
                           {sig, sig},  // Two identical overloads
                           {type1, type2});

    DiagnosticsProvider provider(model);
    auto ambiguities = provider.detect_dispatch_ambiguities(file);

    RC_ASSERT(!ambiguities.empty());
    RC_ASSERT(ambiguities[0].function_name == "process");
    RC_ASSERT(ambiguities[0].candidate_signatures.size() == 2u);
}

/**
 * Property 35d: A single overload never produces ambiguity.
 */
RC_GTEST_PROP(DispatchAmbiguityProperty,
              SingleOverloadNoAmbiguity,
              ()) {
    auto type = *genConcreteType();
    std::string sig = "(" + type + ") -> Unit";

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_dispatch(model, file, "process", {sig}, {type});

    DiagnosticsProvider provider(model);
    auto ambiguities = provider.detect_dispatch_ambiguities(file);

    RC_ASSERT(ambiguities.empty());
}

}  // namespace
}  // namespace meld::daemon
