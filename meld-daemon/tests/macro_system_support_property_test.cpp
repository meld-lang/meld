/**
 * **Feature: meld-daemon, Property 52: Macro System Support**
 *
 * For any macro definition, the LspChannel SHALL provide appropriate
 * language service support for meta-macro system constructs.
 *
 * **Validates: Requirements 21.2**
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

void add_macro(SemanticModel& model,
               const std::filesystem::path& file,
               const std::string& macro_name,
               const std::string& macro_kind,
               const std::vector<std::string>& params) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto macro_node = std::make_shared<ASTNode>();
    macro_node->kind = macro_kind;
    macro_node->name = macro_name;
    macro_node->location = SourceLocation{file, 3, 0};

    for (const auto& param : params) {
        auto param_node = std::make_shared<ASTNode>();
        param_node->kind = "parameter";
        param_node->name = param;
        param_node->location = SourceLocation{file, 4, 0};
        macro_node->children.push_back(param_node);
    }

    root->children.push_back(macro_node);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genMacroKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "macro_definition", "derive_macro", "attribute_macro", "meta_macro"});
}

rc::Gen<std::string> genParamName() {
    return rc::gen::map(
        rc::gen::inRange(1, 5),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('x' + (i % 3));
            return s;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 52a: Valid macro kinds are recognized and analyzed correctly.
 */
RC_GTEST_PROP(MacroSystemSupport,
              ValidMacroKindsRecognized,
              ()) {
    auto kind = *genMacroKind();
    auto param_count = *rc::gen::inRange(0, 4);
    std::vector<std::string> params;
    for (int i = 0; i < param_count; ++i) {
        params.push_back("p" + std::to_string(i));
    }

    SemanticModel model;
    std::filesystem::path file = "macros.meld";
    add_macro(model, file, "my_macro", kind, params);

    MeldFeatureProvider provider(model);
    auto result = provider.analyze_macro(file, "my_macro");

    RC_ASSERT(result.valid == true);
    RC_ASSERT(result.macro_name == "my_macro");
    RC_ASSERT(!result.macro_kind.empty());
    RC_ASSERT(result.parameters.size() == static_cast<size_t>(param_count));
    RC_ASSERT(!result.expansion_hint.empty());
}

/**
 * Property 52b: Non-macro nodes are not recognized as macros.
 */
RC_GTEST_PROP(MacroSystemSupport,
              NonMacroNotRecognized,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "code.meld";
    add_macro(model, file, "regular_fn", "function_definition", {});

    MeldFeatureProvider provider(model);
    auto result = provider.analyze_macro(file, "regular_fn");

    RC_ASSERT(result.valid == false);
}

/**
 * Property 52c: Missing macro returns invalid result.
 */
RC_GTEST_PROP(MacroSystemSupport,
              MissingMacroInvalid,
              ()) {
    SemanticModel model;
    MeldFeatureProvider provider(model);

    auto result = provider.analyze_macro("test.meld", "nonexistent");
    RC_ASSERT(result.valid == false);
    RC_ASSERT(!result.expansion_hint.empty());
}

}  // namespace
}  // namespace meld::daemon
