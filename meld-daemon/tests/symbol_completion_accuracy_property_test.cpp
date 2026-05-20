/**
 * **Feature: meld-daemon, Property 27: Symbol Completion Accuracy**
 *
 * For any symbol completion request, all available variables, functions,
 * types, and imported symbols in scope SHALL be suggested.
 *
 * **Validates: Requirements 16.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/completion_provider.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_model_with_symbols(
    SemanticModel& model,
    const std::filesystem::path& file,
    const std::vector<std::tuple<std::string, std::string, std::string>>& symbols) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    for (const auto& [name, kind, type_info] : symbols) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = type_info;
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

rc::Gen<std::string> genVarName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i)
            s += static_cast<char>('a' + (i % 26));
        return "v" + s;
    });
}

rc::Gen<std::string> genFuncName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i)
            s += static_cast<char>('a' + (i % 26));
        return "f" + s;
    });
}

rc::Gen<std::string> genStructName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s;
        for (int i = 0; i < len; ++i)
            s += static_cast<char>('a' + (i % 26));
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        return "S" + s;
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 27a: All variables defined in the file appear in completions.
 */
RC_GTEST_PROP(SymbolCompletionAccuracyProperty,
              AllVariablesAppearInCompletions,
              ()) {
    auto var_names = *rc::gen::container<std::vector<std::string>>(
        3, genVarName());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    std::vector<std::tuple<std::string, std::string, std::string>> symbols;
    for (const auto& name : var_names) {
        symbols.emplace_back(name, "val_declaration", "Int");
    }
    populate_model_with_symbols(model, file, symbols);

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 10, 0, "");

    for (const auto& name : var_names) {
        bool found = std::any_of(result.items.begin(), result.items.end(),
            [&](const CompletionItem& item) {
                return item.label == name && item.kind == "variable";
            });
        RC_ASSERT(found);
    }
}

/**
 * Property 27b: All functions defined in the file appear in completions.
 */
RC_GTEST_PROP(SymbolCompletionAccuracyProperty,
              AllFunctionsAppearInCompletions,
              ()) {
    auto func_names = *rc::gen::container<std::vector<std::string>>(
        3, genFuncName());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    std::vector<std::tuple<std::string, std::string, std::string>> symbols;
    for (const auto& name : func_names) {
        symbols.emplace_back(name, "function_definition", "() -> Int");
    }
    populate_model_with_symbols(model, file, symbols);

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 10, 0, "");

    for (const auto& name : func_names) {
        bool found = std::any_of(result.items.begin(), result.items.end(),
            [&](const CompletionItem& item) {
                return item.label == name && item.kind == "function";
            });
        RC_ASSERT(found);
    }
}

/**
 * Property 27c: All types defined in the file appear in completions.
 */
RC_GTEST_PROP(SymbolCompletionAccuracyProperty,
              AllTypesAppearInCompletions,
              ()) {
    auto type_names = *rc::gen::container<std::vector<std::string>>(
        3, genStructName());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    std::vector<std::tuple<std::string, std::string, std::string>> symbols;
    for (const auto& name : type_names) {
        symbols.emplace_back(name, "struct", name);
    }
    populate_model_with_symbols(model, file, symbols);

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 10, 0, "");

    for (const auto& name : type_names) {
        bool found = std::any_of(result.items.begin(), result.items.end(),
            [&](const CompletionItem& item) {
                return item.label == name && item.kind == "type";
            });
        RC_ASSERT(found);
    }
}

/**
 * Property 27d: Symbols from other indexed files appear as imported completions.
 */
RC_GTEST_PROP(SymbolCompletionAccuracyProperty,
              ImportedSymbolsAppearInCompletions,
              ()) {
    auto exported_name = *genFuncName();

    SemanticModel model;
    std::filesystem::path main_file = "main.meld";
    std::filesystem::path lib_file = "lib.meld";

    populate_model_with_symbols(model, main_file, {
        {"localVar", "val_declaration", "Int"}
    });
    populate_model_with_symbols(model, lib_file, {
        {exported_name, "function_definition", "() -> String"}
    });

    CompletionProvider provider(model);
    auto result = provider.get_completions(main_file, 5, 0, "");

    // The exported function from lib.meld should appear
    bool found = std::any_of(result.items.begin(), result.items.end(),
        [&](const CompletionItem& item) { return item.label == exported_name; });
    RC_ASSERT(found);
}

}  // namespace
}  // namespace meld::daemon
