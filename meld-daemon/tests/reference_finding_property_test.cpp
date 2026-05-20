/**
 * **Feature: meld-daemon, Property 37: Reference Finding Completeness**
 *
 * For any symbol, "find references" SHALL locate all usages across the
 * workspace without missing any.
 *
 * **Validates: Requirements 18.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/navigation_provider.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Insert nodes with the given name at specific locations in a file's AST.
void populate_with_references(SemanticModel& model,
                              const std::filesystem::path& file,
                              const std::string& target_name,
                              const std::vector<std::pair<uint32_t, std::string>>& refs,
                              const std::vector<std::pair<std::string, std::string>>& others = {}) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";

    for (const auto& [line, kind] : refs) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = target_name;
        child->type_info = "Int";
        child->location = SourceLocation{file, line, 0};
        root->children.push_back(child);
    }

    for (const auto& [name, kind] : others) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = "Int";
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

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

rc::Gen<std::string> genRefKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "val_declaration", "function_definition", "identifier"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 37a: The number of references found equals the total number of
 * AST nodes with that symbol name across all files.
 */
RC_GTEST_PROP(ReferenceFindingProperty,
              FindsAllReferencesAcrossWorkspace,
              ()) {
    auto target = *genIdentifier();
    auto ref_count_a = *rc::gen::inRange(1, 5);
    auto ref_count_b = *rc::gen::inRange(0, 4);

    std::vector<std::pair<uint32_t, std::string>> refs_a;
    for (int i = 0; i < ref_count_a; ++i)
        refs_a.push_back({static_cast<uint32_t>(i + 1), "val_declaration"});

    std::vector<std::pair<uint32_t, std::string>> refs_b;
    for (int i = 0; i < ref_count_b; ++i)
        refs_b.push_back({static_cast<uint32_t>(i + 10), "identifier"});

    SemanticModel model;
    populate_with_references(model, "a.meld", target, refs_a);
    populate_with_references(model, "b.meld", target, refs_b);

    NavigationProvider provider(model);
    auto results = provider.find_references(target);

    RC_ASSERT(static_cast<int>(results.size()) == ref_count_a + ref_count_b);
}

/**
 * Property 37b: Every reference returned has the correct symbol name.
 */
RC_GTEST_PROP(ReferenceFindingProperty,
              AllReferencesHaveCorrectName,
              ()) {
    auto target = *genIdentifier();

    SemanticModel model;
    populate_with_references(model, "test.meld", target,
                             {{1, "val_declaration"}, {5, "identifier"}, {10, "function_definition"}});

    NavigationProvider provider(model);
    auto results = provider.find_references(target);

    for (const auto& ref : results) {
        RC_ASSERT(ref.name == target);
    }
}

/**
 * Property 37c: References for a non-existent symbol returns empty.
 */
RC_GTEST_PROP(ReferenceFindingProperty,
              NonExistentSymbolReturnsEmpty,
              ()) {
    SemanticModel model;
    populate_with_references(model, "test.meld", "existing",
                             {{1, "val_declaration"}});

    NavigationProvider provider(model);
    auto results = provider.find_references("nonexistent_xyz");
    RC_ASSERT(results.empty());
}

/**
 * Property 37d: Other symbols in the same file are not included in results.
 */
RC_GTEST_PROP(ReferenceFindingProperty,
              OtherSymbolsNotIncluded,
              ()) {
    auto target = *genIdentifier();
    auto other = *genIdentifier();
    RC_PRE(target != other);

    SemanticModel model;
    populate_with_references(model, "test.meld", target,
                             {{1, "val_declaration"}},
                             {{other, "function_definition"}});

    NavigationProvider provider(model);
    auto results = provider.find_references(target);

    for (const auto& ref : results) {
        RC_ASSERT(ref.name == target);
        RC_ASSERT(ref.name != other);
    }
}

}  // namespace
}  // namespace meld::daemon
