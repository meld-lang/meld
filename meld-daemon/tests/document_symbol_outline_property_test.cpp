/**
 * **Feature: meld-daemon, Property 38: Document Symbol Outline Accuracy**
 *
 * For any Meld file, the document symbols SHALL provide a complete
 * hierarchical outline of all functions, types, and declarations.
 *
 * **Validates: Requirements 18.3**
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

struct SymbolSpec {
    std::string name;
    std::string kind;
    std::vector<SymbolSpec> children;
};

std::shared_ptr<ASTNode> build_ast(const std::filesystem::path& file,
                                   const std::vector<SymbolSpec>& specs) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";

    std::function<void(const std::vector<SymbolSpec>&, std::shared_ptr<ASTNode>&)> build;
    build = [&](const std::vector<SymbolSpec>& items, std::shared_ptr<ASTNode>& parent) {
        for (const auto& spec : items) {
            auto node = std::make_shared<ASTNode>();
            node->kind = spec.kind;
            node->name = spec.name;
            node->type_info = spec.kind == "function_definition" ? "() -> Int" : "Int";
            node->location = SourceLocation{file, 0, 0};
            build(spec.children, node);
            parent->children.push_back(node);
        }
    };
    build(specs, root);
    return root;
}

void populate_model(SemanticModel& model, const std::filesystem::path& file,
                    const std::vector<SymbolSpec>& specs) {
    FileSemantics sem;
    sem.path = file;
    sem.ast = build_ast(file, specs);
    model.update_file(file, std::move(sem));
}

/// Collect all symbol names from a DocumentSymbol tree.
void collect_names(const std::vector<DocumentSymbol>& symbols,
                   std::vector<std::string>& names) {
    for (const auto& sym : symbols) {
        names.push_back(sym.name);
        collect_names(sym.children, names);
    }
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
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

rc::Gen<SymbolSpec> genSymbolSpec() {
    return rc::gen::apply(
        [](const std::string& name, const std::string& kind) {
            return SymbolSpec{name, kind, {}};
        },
        genIdentifier(), genKind());
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 38a: Every named symbol in the AST appears in the document outline.
 */
RC_GTEST_PROP(DocumentSymbolOutlineProperty,
              AllNamedSymbolsAppearInOutline,
              ()) {
    auto specs = *rc::gen::container<std::vector<SymbolSpec>>(6, genSymbolSpec());

    // Ensure unique names
    std::set<std::string> seen;
    std::vector<SymbolSpec> unique_specs;
    for (auto& s : specs) {
        if (seen.insert(s.name).second)
            unique_specs.push_back(s);
    }
    RC_PRE(!unique_specs.empty());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, unique_specs);

    NavigationProvider provider(model);
    auto symbols = provider.get_document_symbols(file);

    std::vector<std::string> outline_names;
    collect_names(symbols, outline_names);

    for (const auto& spec : unique_specs) {
        bool found = std::find(outline_names.begin(), outline_names.end(),
                               spec.name) != outline_names.end();
        RC_ASSERT(found);
    }
}

/**
 * Property 38b: The number of top-level document symbols matches the number
 * of top-level named AST children.
 */
RC_GTEST_PROP(DocumentSymbolOutlineProperty,
              TopLevelCountMatchesASTChildren,
              ()) {
    auto specs = *rc::gen::container<std::vector<SymbolSpec>>(8, genSymbolSpec());

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, specs);

    NavigationProvider provider(model);
    auto symbols = provider.get_document_symbols(file);

    RC_ASSERT(symbols.size() == specs.size());
}

/**
 * Property 38c: Hierarchical symbols preserve parent-child relationships.
 */
RC_GTEST_PROP(DocumentSymbolOutlineProperty,
              HierarchicalSymbolsPreserveStructure,
              ()) {
    auto parent_name = *genIdentifier();
    auto child_name = *genIdentifier();
    RC_PRE(parent_name != child_name);

    SymbolSpec child_spec{child_name, "val_declaration", {}};
    SymbolSpec parent_spec{parent_name, "struct", {child_spec}};

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {parent_spec});

    NavigationProvider provider(model);
    auto symbols = provider.get_document_symbols(file);

    RC_ASSERT(symbols.size() == 1u);
    RC_ASSERT(symbols[0].name == parent_name);
    RC_ASSERT(symbols[0].children.size() == 1u);
    RC_ASSERT(symbols[0].children[0].name == child_name);
}

/**
 * Property 38d: Empty file returns empty document symbols.
 */
RC_GTEST_PROP(DocumentSymbolOutlineProperty,
              EmptyFileReturnsEmptySymbols,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "empty.meld";
    populate_model(model, file, {});

    NavigationProvider provider(model);
    auto symbols = provider.get_document_symbols(file);
    RC_ASSERT(symbols.empty());
}

}  // namespace
}  // namespace meld::daemon
