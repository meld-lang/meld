/**
 * **Feature: meld-daemon, Property 54: Multiple Dispatch Resolution**
 *
 * For any multiple dispatch function call, dispatch SHALL be resolved
 * based on all argument types correctly.
 *
 * **Validates: Requirements 21.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/meld_feature_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_overloads(SemanticModel& model,
                   const std::filesystem::path& file,
                   const std::string& fn_name,
                   const std::vector<std::string>& signatures) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    uint32_t line = 3;
    for (const auto& sig : signatures) {
        auto fn_node = std::make_shared<ASTNode>();
        fn_node->kind = "function_definition";
        fn_node->name = fn_name;
        fn_node->type_info = sig;
        fn_node->location = SourceLocation{file, line, 0};
        root->children.push_back(fn_node);
        line += 5;
    }

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 54a: A single overload always resolves unambiguously.
 */
RC_GTEST_PROP(MultipleDispatchResolution,
              SingleOverloadResolves,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "dispatch.meld";
    add_overloads(model, file, "process", {"(Int) -> String"});

    MeldFeatureProvider provider(model);
    auto result = provider.resolve_dispatch(file, "process", {"Int"});

    RC_ASSERT(result.resolved == true);
    RC_ASSERT(result.ambiguous == false);
    RC_ASSERT(result.candidates.size() == 1);
    RC_ASSERT(result.resolved_overload == "(Int) -> String");
}

/**
 * Property 54b: Multiple overloads with distinct types resolve correctly
 * when argument types match exactly one.
 */
RC_GTEST_PROP(MultipleDispatchResolution,
              DistinctOverloadsResolve,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "dispatch.meld";
    add_overloads(model, file, "convert", {
        "(Int) -> String",
        "(Float) -> String",
        "(Bool) -> String"
    });

    MeldFeatureProvider provider(model);

    // Resolve with Int — should match only the first overload
    auto result = provider.resolve_dispatch(file, "convert", {"Int"});

    RC_ASSERT(result.resolved == true);
    RC_ASSERT(result.ambiguous == false);
    RC_ASSERT(result.resolved_overload == "(Int) -> String");
    RC_ASSERT(result.candidates.size() == 3);
}

/**
 * Property 54c: Ambiguous overloads are detected.
 */
RC_GTEST_PROP(MultipleDispatchResolution,
              AmbiguousOverloadsDetected,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "dispatch.meld";
    // Two overloads with the same type signature
    add_overloads(model, file, "ambig", {
        "(Int, String) -> Bool",
        "(Int, String) -> Int"
    });

    MeldFeatureProvider provider(model);
    auto result = provider.resolve_dispatch(file, "ambig", {"Int", "String"});

    // Both match, so it's ambiguous
    RC_ASSERT(result.ambiguous == true);
    RC_ASSERT(result.resolved == false);
    RC_ASSERT(result.candidates.size() == 2);
}

/**
 * Property 54d: Non-existent function returns unresolved.
 */
RC_GTEST_PROP(MultipleDispatchResolution,
              NonExistentFunctionUnresolved,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "dispatch.meld";

    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "dispatch";
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));

    MeldFeatureProvider provider(model);
    auto result = provider.resolve_dispatch(file, "missing_fn", {"Int"});

    RC_ASSERT(result.resolved == false);
    RC_ASSERT(result.ambiguous == false);
    RC_ASSERT(result.candidates.empty());
}

}  // namespace
}  // namespace meld::daemon
