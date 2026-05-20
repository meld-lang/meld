/**
 * **Feature: meld-daemon, Property 44: Rename Conflict Detection**
 *
 * For any rename operation that would cause naming conflicts, the
 * LspChannel SHALL detect and prevent the unsafe rename.
 *
 * **Validates: Requirements 19.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/formatting_provider.hpp"

#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_model_with_symbols(SemanticModel& model,
                                  const std::filesystem::path& file,
                                  const std::vector<std::string>& names) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    uint32_t line = 1;
    for (const auto& name : names) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->type_info = "() -> Int";
        child->location = SourceLocation{file, line++, 0};
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

rc::Gen<std::string> genValidIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            s += 'a';
            for (int i = 1; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

rc::Gen<std::string> genInvalidIdentifier() {
    return rc::gen::elementOf(std::vector<std::string>{
        "123abc", "!invalid", " spaces", "", "@symbol",
        "has space", "has-dash"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 44a: Renaming to an existing symbol name is detected as a conflict.
 */
RC_GTEST_PROP(RenameConflictDetection,
              RenamingToExistingNameIsConflict,
              ()) {
    auto name_a = *genValidIdentifier();
    auto name_b = *rc::gen::distinctFrom(genValidIdentifier(), name_a);

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model_with_symbols(model, file, {name_a, name_b});

    FormattingProvider provider(model);

    // Renaming name_a to name_b should be a conflict
    RC_ASSERT(provider.has_naming_conflict(name_a, name_b));

    // And the rename should fail
    auto result = provider.rename_symbol(name_a, name_b);
    RC_ASSERT(!result.success);
    RC_ASSERT(!result.error_message.empty());
}

/**
 * Property 44b: Renaming to an invalid identifier is detected as a conflict.
 */
RC_GTEST_PROP(RenameConflictDetection,
              InvalidIdentifierIsConflict,
              ()) {
    auto old_name = *genValidIdentifier();
    auto invalid_name = *genInvalidIdentifier();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model_with_symbols(model, file, {old_name});

    FormattingProvider provider(model);
    RC_ASSERT(provider.has_naming_conflict(old_name, invalid_name));
}

/**
 * Property 44c: Renaming to a fresh, valid name that doesn't exist
 * is NOT a conflict.
 */
RC_GTEST_PROP(RenameConflictDetection,
              FreshValidNameIsNotConflict,
              ()) {
    auto old_name = *genValidIdentifier();
    // Generate a name that won't collide
    auto new_name = *rc::gen::map(
        genValidIdentifier(),
        [](std::string s) { return "fresh_" + s; });

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model_with_symbols(model, file, {old_name});

    FormattingProvider provider(model);
    RC_ASSERT(!provider.has_naming_conflict(old_name, new_name));
}

/**
 * Property 44d: Renaming a symbol to itself is not a conflict.
 */
RC_GTEST_PROP(RenameConflictDetection,
              RenamingToSelfIsNotConflict,
              ()) {
    auto name = *genValidIdentifier();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model_with_symbols(model, file, {name});

    FormattingProvider provider(model);
    RC_ASSERT(!provider.has_naming_conflict(name, name));
}

}  // namespace
}  // namespace meld::daemon
