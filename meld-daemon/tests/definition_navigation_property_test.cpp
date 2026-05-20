/**
 * **Feature: meld-daemon, Property 36: Definition Navigation Accuracy**
 *
 * For any symbol, "go to definition" SHALL navigate to the correct
 * declaration location.
 *
 * **Validates: Requirements 18.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/navigation_provider.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct SymbolDef {
    std::string name;
    std::string kind;
    uint32_t line;
    uint32_t column;
};

void populate_model(SemanticModel& model, const std::filesystem::path& file,
                    const std::vector<SymbolDef>& symbols) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    for (const auto& sym : symbols) {
        auto child = std::make_shared<ASTNode>();
        child->kind = sym.kind;
        child->name = sym.name;
        child->type_info = sym.kind == "function_definition" ? "() -> Int" : "Int";
        child->location = SourceLocation{file, sym.line, sym.column};
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

rc::Gen<std::string> genKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "function_definition", "val_declaration",
        "struct", "enum", "trait"});
}

rc::Gen<SymbolDef> genSymbolDef() {
    return rc::gen::apply(
        [](const std::string& name, const std::string& kind, int line) {
            return SymbolDef{name, kind, static_cast<uint32_t>(line), 0};
        },
        genIdentifier(), genKind(), rc::gen::inRange(1, 100));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 36a: For any symbol defined in a file, go_to_definition returns
 * the correct file and location.
 */
RC_GTEST_PROP(DefinitionNavigationProperty,
              DefinedSymbolNavigatesToCorrectLocation,
              ()) {
    auto symbols = *rc::gen::container<std::vector<SymbolDef>>(
        5, genSymbolDef());
    // Ensure unique names
    std::vector<SymbolDef> unique_symbols;
    std::set<std::string> seen;
    for (auto& s : symbols) {
        if (seen.insert(s.name).second)
            unique_symbols.push_back(s);
    }
    RC_PRE(!unique_symbols.empty());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, unique_symbols);

    NavigationProvider provider(model);

    for (const auto& sym : unique_symbols) {
        auto result = provider.go_to_definition(file, sym.name);
        RC_ASSERT(result.has_value());
        RC_ASSERT(result->name == sym.name);
        RC_ASSERT(result->file == file);
        RC_ASSERT(result->line == sym.line);
    }
}

/**
 * Property 36b: For any symbol NOT defined anywhere, go_to_definition
 * returns nullopt.
 */
RC_GTEST_PROP(DefinitionNavigationProperty,
              UndefinedSymbolReturnsNullopt,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {{"existing", "val_declaration", 1, 0}});

    NavigationProvider provider(model);
    auto result = provider.go_to_definition(file, "nonexistent_symbol_xyz");
    RC_ASSERT(!result.has_value());
}

/**
 * Property 36c: Cross-file definition navigation — a symbol defined in
 * another file is found when not present in the current file.
 */
RC_GTEST_PROP(DefinitionNavigationProperty,
              CrossFileDefinitionIsFound,
              ()) {
    auto sym_name = *genIdentifier();
    auto line = *rc::gen::inRange(1, 100);

    SemanticModel model;
    std::filesystem::path file_a = "a.meld";
    std::filesystem::path file_b = "b.meld";

    // Symbol defined in file_b only
    populate_model(model, file_a, {});
    populate_model(model, file_b, {{sym_name, "function_definition",
                                    static_cast<uint32_t>(line), 0}});

    NavigationProvider provider(model);
    auto result = provider.go_to_definition(file_a, sym_name);
    RC_ASSERT(result.has_value());
    RC_ASSERT(result->file == file_b);
    RC_ASSERT(result->name == sym_name);
}

}  // namespace
}  // namespace meld::daemon
