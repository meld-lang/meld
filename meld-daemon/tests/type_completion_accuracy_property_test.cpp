/**
 * **Feature: meld-daemon, Property 29: Type Completion Accuracy**
 *
 * For any type annotation context, completions SHALL include all valid type
 * names (built-in, user-defined, and aliases).
 *
 * **Validates: Requirements 16.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/completion_provider.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_with_types(SemanticModel& model,
                         const std::filesystem::path& file,
                         const std::vector<std::pair<std::string, std::string>>& types) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, kind] : types) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = name;
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genUserTypeName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s = "T";
        for (int i = 0; i < len; ++i)
            s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

rc::Gen<std::string> genTypeKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "struct", "enum", "trait", "type_alias"
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 29a: All built-in types appear in type annotation completions.
 */
RC_GTEST_PROP(TypeCompletionAccuracyProperty,
              AllBuiltinTypesAppearInTypeContext,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_types(model, file, {});

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 5, 7, "val x: ");

    for (const auto& bt : builtin_types()) {
        bool found = std::any_of(result.items.begin(), result.items.end(),
            [&](const CompletionItem& item) { return item.label == bt; });
        RC_ASSERT(found);
    }
}

/**
 * Property 29b: User-defined types appear in type annotation completions.
 */
RC_GTEST_PROP(TypeCompletionAccuracyProperty,
              UserDefinedTypesAppearInTypeContext,
              ()) {
    auto type_name = *genUserTypeName();
    auto type_kind = *genTypeKind();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_types(model, file, {{type_name, type_kind}});

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 5, 7, "val x: ");

    bool found = std::any_of(result.items.begin(), result.items.end(),
        [&](const CompletionItem& item) {
            return item.label == type_name && item.kind == "type";
        });
    RC_ASSERT(found);
}

/**
 * Property 29c: Type aliases appear alongside structs and enums.
 */
RC_GTEST_PROP(TypeCompletionAccuracyProperty,
              TypeAliasesAppearInTypeContext,
              ()) {
    auto alias_name = *genUserTypeName();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_types(model, file, {{alias_name, "type_alias"}});

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 5, 7, "val x: ");

    bool found = std::any_of(result.items.begin(), result.items.end(),
        [&](const CompletionItem& item) {
            return item.label == alias_name && item.kind == "type";
        });
    RC_ASSERT(found);
}

/**
 * Property 29d: Types from other files appear in type annotation completions.
 */
RC_GTEST_PROP(TypeCompletionAccuracyProperty,
              CrossFileTypesAppearInTypeContext,
              ()) {
    auto remote_type = *genUserTypeName();

    SemanticModel model;
    std::filesystem::path main_file = "main.meld";
    std::filesystem::path lib_file = "lib.meld";

    populate_with_types(model, main_file, {});
    populate_with_types(model, lib_file, {{remote_type, "struct"}});

    CompletionProvider provider(model);
    auto result = provider.get_completions(main_file, 5, 7, "val x: ");

    bool found = std::any_of(result.items.begin(), result.items.end(),
        [&](const CompletionItem& item) {
            return item.label == remote_type && item.kind == "type";
        });
    RC_ASSERT(found);
}

}  // namespace
}  // namespace meld::daemon
