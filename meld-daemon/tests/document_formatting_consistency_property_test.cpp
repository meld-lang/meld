/**
 * **Feature: meld-daemon, Property 41: Document Formatting Consistency**
 *
 * For any Meld document, formatting SHALL produce output that conforms
 * to standard style conventions.
 *
 * **Validates: Requirements 19.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/formatting_provider.hpp"

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
 * Property 41a: Formatting any document produces edits that are sorted
 * by position (top-to-bottom, left-to-right).
 */
RC_GTEST_PROP(DocumentFormattingConsistency,
              EditsAreSortedByPosition,
              ()) {
    auto symbols = *rc::gen::container<std::vector<SymbolDef>>(
        5, genSymbolDef());
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

    FormattingProvider provider(model);
    auto edits = provider.format_document(file);

    // Edits must be sorted by position
    for (size_t i = 1; i < edits.size(); ++i) {
        RC_ASSERT(edits[i].start_line > edits[i - 1].start_line ||
                  (edits[i].start_line == edits[i - 1].start_line &&
                   edits[i].start_column >= edits[i - 1].start_column));
    }
}

/**
 * Property 41b: Formatting is idempotent — formatting an already-formatted
 * line produces the same result.
 */
RC_GTEST_PROP(DocumentFormattingConsistency,
              FormattingIsIdempotent,
              ()) {
    auto raw_line = *rc::gen::map(
        rc::gen::container<std::string>(rc::gen::inRange(32, 127)),
        [](std::string s) {
            // Ensure at least one non-whitespace character
            if (s.empty() || std::all_of(s.begin(), s.end(),
                    [](char c) { return c == ' ' || c == '\t'; })) {
                s = "x";
            }
            return s;
        });

    std::string first = FormattingProvider::format_line(raw_line);
    std::string second = FormattingProvider::format_line(first);
    RC_ASSERT(first == second);
}

/**
 * Property 41c: Formatting a document with no AST returns no edits.
 */
RC_GTEST_PROP(DocumentFormattingConsistency,
              NoAstReturnsNoEdits,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "nonexistent.meld";

    FormattingProvider provider(model);
    auto edits = provider.format_document(file);
    RC_ASSERT(edits.empty());
}

}  // namespace
}  // namespace meld::daemon
