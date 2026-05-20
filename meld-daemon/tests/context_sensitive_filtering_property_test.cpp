/**
 * **Feature: meld-daemon, Property 30: Context-Sensitive Filtering**
 *
 * For any syntactic position, completion suggestions SHALL be filtered to
 * exclude inappropriate options for that context.
 *
 * **Validates: Requirements 16.5**
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

void populate_mixed_model(SemanticModel& model, const std::filesystem::path& file) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    auto var = std::make_shared<ASTNode>();
    var->kind = "val_declaration";
    var->name = "myVar";
    var->type_info = "Int";
    root->children.push_back(var);

    auto func = std::make_shared<ASTNode>();
    func->kind = "function_definition";
    func->name = "myFunc";
    func->type_info = "() -> Int";
    root->children.push_back(func);

    auto type = std::make_shared<ASTNode>();
    type->kind = "struct";
    type->name = "MyStruct";
    type->type_info = "MyStruct";
    root->children.push_back(type);

    auto alias = std::make_shared<ASTNode>();
    alias->kind = "type_alias";
    alias->name = "MyAlias";
    alias->type_info = "MyAlias";
    root->children.push_back(alias);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genTypeAnnotationLine() {
    return rc::gen::elementOf(std::vector<std::string>{
        "val x: ", "let y: ", "var z: ", "fnc foo(x: "
    });
}

rc::Gen<std::string> genImportLine() {
    return rc::gen::elementOf(std::vector<std::string>{
        "import ", "imp "
    });
}

rc::Gen<std::string> genGeneralLine() {
    return rc::gen::elementOf(std::vector<std::string>{
        "", "val x = ", "fnc foo() {\n    "
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 30a: In type annotation context, no variables or keywords appear.
 */
RC_GTEST_PROP(ContextSensitiveFilteringProperty,
              TypeContextExcludesVariablesAndKeywords,
              ()) {
    auto line = *genTypeAnnotationLine();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_mixed_model(model, file);

    CompletionProvider provider(model);
    auto result = provider.get_completions(
        file, 5, static_cast<uint32_t>(line.size()), line);

    for (const auto& item : result.items) {
        RC_ASSERT(item.kind != "variable");
        RC_ASSERT(item.kind != "keyword");
        RC_ASSERT(item.kind != "function");
    }
}

/**
 * Property 30b: In import context, only importable modules appear.
 */
RC_GTEST_PROP(ContextSensitiveFilteringProperty,
              ImportContextOnlyShowsModules,
              ()) {
    auto line = *genImportLine();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_mixed_model(model, file);

    CompletionProvider provider(model);
    auto result = provider.get_completions(
        file, 5, static_cast<uint32_t>(line.size()), line);

    for (const auto& item : result.items) {
        RC_ASSERT(item.kind == "import");
    }
}

/**
 * Property 30c: In general context, all symbol kinds are present.
 */
RC_GTEST_PROP(ContextSensitiveFilteringProperty,
              GeneralContextIncludesAllKinds,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_mixed_model(model, file);

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 10, 0, "");

    bool has_var = std::any_of(result.items.begin(), result.items.end(),
        [](const CompletionItem& i) { return i.kind == "variable"; });
    bool has_func = std::any_of(result.items.begin(), result.items.end(),
        [](const CompletionItem& i) { return i.kind == "function"; });
    bool has_type = std::any_of(result.items.begin(), result.items.end(),
        [](const CompletionItem& i) { return i.kind == "type"; });
    bool has_kw = std::any_of(result.items.begin(), result.items.end(),
        [](const CompletionItem& i) { return i.kind == "keyword"; });

    RC_ASSERT(has_var);
    RC_ASSERT(has_func);
    RC_ASSERT(has_type);
    RC_ASSERT(has_kw);
}

/**
 * Property 30d: Prefix filtering reduces results — filtered set is a subset.
 */
RC_GTEST_PROP(ContextSensitiveFilteringProperty,
              PrefixFilteringProducesSubset,
              ()) {
    auto prefix = *rc::gen::elementOf(std::vector<std::string>{
        "my", "My", "f", "v"
    });

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_mixed_model(model, file);

    CompletionProvider provider(model);
    auto all = provider.get_completions(file, 10, 0, "");
    auto filtered = CompletionProvider::filter_by_prefix(all.items, prefix);

    RC_ASSERT(filtered.size() <= all.items.size());

    // Every filtered item must exist in the full set
    for (const auto& item : filtered) {
        bool found = std::any_of(all.items.begin(), all.items.end(),
            [&](const CompletionItem& a) { return a.label == item.label; });
        RC_ASSERT(found);
    }
}

}  // namespace
}  // namespace meld::daemon
