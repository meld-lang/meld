/**
 * **Feature: meld-daemon, Property 26: Context-Aware Completions**
 *
 * For any cursor position in a Meld file, completion suggestions SHALL be
 * appropriate for the current scope and context.
 *
 * **Validates: Requirements 16.1**
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

/// Build a minimal SemanticModel with a file containing some symbols.
void populate_model(SemanticModel& model, const std::filesystem::path& file,
                    const std::vector<std::pair<std::string, std::string>>& symbols) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& [name, kind] : symbols) {
        auto child = std::make_shared<ASTNode>();
        child->kind = kind;
        child->name = name;
        child->type_info = kind == "function_definition" ? "() -> Int" : "Int";
        if (kind == "function_definition") {
            auto param = std::make_shared<ASTNode>();
            param->kind = "parameter";
            param->name = "x";
            param->type_info = "Int";
            child->children.push_back(param);
        }
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
        }
    );
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::map(genIdentifier(), [](std::string s) {
        if (!s.empty()) s[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(s[0])));
        return s;
    });
}

rc::Gen<std::pair<std::string, std::string>> genSymbol() {
    return rc::gen::oneOf(
        rc::gen::map(genIdentifier(), [](const std::string& n) {
            return std::make_pair(n, std::string("val_declaration"));
        }),
        rc::gen::map(genIdentifier(), [](const std::string& n) {
            return std::make_pair(n, std::string("function_definition"));
        }),
        rc::gen::map(genTypeName(), [](const std::string& n) {
            return std::make_pair(n, std::string("struct"));
        })
    );
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 26a: In a general context (no special prefix), completions include
 * scope symbols and keywords.
 */
RC_GTEST_PROP(ContextAwareCompletionsProperty,
              GeneralContextIncludesScopeSymbolsAndKeywords,
              ()) {
    auto symbols = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        3, genSymbol());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, symbols);

    CompletionProvider provider(model);
    auto result = provider.get_completions(file, 10, 0, "");

    // Should include at least the symbols we added
    for (const auto& [name, kind] : symbols) {
        bool found = std::any_of(result.items.begin(), result.items.end(),
            [&](const CompletionItem& item) { return item.label == name; });
        RC_ASSERT(found);
    }

    // Should also include keywords
    bool has_keyword = std::any_of(result.items.begin(), result.items.end(),
        [](const CompletionItem& item) { return item.kind == "keyword"; });
    RC_ASSERT(has_keyword);
}

/**
 * Property 26b: In a type annotation context (after ':'), completions are
 * restricted to type items only.
 */
RC_GTEST_PROP(ContextAwareCompletionsProperty,
              TypeAnnotationContextOnlyReturnsTypes,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {
        {"myVar", "val_declaration"},
        {"myFunc", "function_definition"},
        {"MyType", "struct"}
    });

    CompletionProvider provider(model);
    // Cursor after "val x: " — type annotation context
    auto result = provider.get_completions(file, 5, 7, "val x: ");

    for (const auto& item : result.items) {
        RC_ASSERT(item.kind == "type");
    }
}

/**
 * Property 26c: Context determination is consistent — the same input always
 * produces the same context.
 */
RC_GTEST_PROP(ContextAwareCompletionsProperty,
              ContextDeterminationIsConsistent,
              ()) {
    auto line = *rc::gen::elementOf(std::vector<std::string>{
        "val x: ", "import ", "myFunc(", "obj.", "fnc foo"
    });
    auto col = static_cast<uint32_t>(line.size());

    SemanticModel model;
    CompletionProvider provider(model);
    std::filesystem::path file = "test.meld";

    auto ctx1 = provider.determine_context(file, 0, col, line);
    auto ctx2 = provider.determine_context(file, 0, col, line);
    RC_ASSERT(ctx1 == ctx2);
}

}  // namespace
}  // namespace meld::daemon
