/**
 * **Feature: meld-daemon, Property 48: Cross-File Resolution Accuracy**
 *
 * For any cross-file reference, imports, exports, and module dependencies
 * SHALL be resolved correctly.
 *
 * **Validates: Requirements 20.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/workspace_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_file_with_symbol(SemanticModel& model,
                          const std::filesystem::path& file,
                          const std::string& symbol_name,
                          const std::string& kind) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto child = std::make_shared<ASTNode>();
    child->kind = kind;
    child->name = symbol_name;
    child->type_info = kind == "function_definition" ? "() -> Int" : "Int";
    child->location = SourceLocation{file, 5, 0};
    root->children.push_back(child);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    sem.exports.push_back(symbol_name);
    model.update_file(file, std::move(sem));
}

void add_file_with_import(SemanticModel& model,
                          const std::filesystem::path& file,
                          const std::string& import_name) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto imp = std::make_shared<ASTNode>();
    imp->kind = "import";
    imp->name = import_name;
    imp->location = SourceLocation{file, 2, 0};
    root->children.push_back(imp);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    sem.imports.push_back(import_name);
    model.update_file(file, std::move(sem));
}

rc::Gen<std::string> genSymbolName() {
    return rc::gen::map(
        rc::gen::inRange(3, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 48a: A cross-file reference to an existing symbol resolves correctly.
 */
RC_GTEST_PROP(CrossFileResolution,
              ExistingSymbolResolves,
              ()) {
    auto symbol = *genSymbolName();

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    std::filesystem::path def_file = "definitions.meld";
    std::filesystem::path ref_file = "references.meld";

    add_file_with_symbol(model, def_file, symbol, "function_definition");
    add_file_with_import(model, ref_file, symbol);

    auto result = provider.resolve_cross_file_reference(ref_file, symbol);

    RC_ASSERT(result.resolved == true);
    RC_ASSERT(result.target_file == def_file);
    RC_ASSERT(result.symbol_name == symbol);
}

/**
 * Property 48b: A cross-file reference to a non-existent symbol does not resolve.
 */
RC_GTEST_PROP(CrossFileResolution,
              MissingSymbolDoesNotResolve,
              ()) {
    auto symbol = *genSymbolName();

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    std::filesystem::path ref_file = "references.meld";
    add_file_with_import(model, ref_file, symbol);

    auto result = provider.resolve_cross_file_reference(ref_file, symbol);

    RC_ASSERT(result.resolved == false);
}

/**
 * Property 48c: resolve_all_imports finds all import references in a file.
 */
RC_GTEST_PROP(CrossFileResolution,
              AllImportsResolved,
              ()) {
    auto count = *rc::gen::inRange(1, 4);

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    // Create definition files
    std::vector<std::string> symbols;
    for (int i = 0; i < count; ++i) {
        std::string sym = "sym" + std::to_string(i);
        symbols.push_back(sym);
        add_file_with_symbol(model, "def" + std::to_string(i) + ".meld",
                             sym, "function_definition");
    }

    // Create a file that imports all symbols
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "importer";
    root->location = SourceLocation{"importer.meld", 1, 0};
    for (int i = 0; i < count; ++i) {
        auto imp = std::make_shared<ASTNode>();
        imp->kind = "import";
        imp->name = symbols[i];
        imp->location = SourceLocation{"importer.meld",
                                        static_cast<uint32_t>(2 + i), 0};
        root->children.push_back(imp);
    }
    FileSemantics sem;
    sem.path = "importer.meld";
    sem.ast = root;
    model.update_file("importer.meld", std::move(sem));

    auto results = provider.resolve_all_imports("importer.meld");

    RC_ASSERT(results.size() == static_cast<size_t>(count));
    for (const auto& ref : results) {
        RC_ASSERT(ref.resolved == true);
    }
}

}  // namespace
}  // namespace meld::daemon
