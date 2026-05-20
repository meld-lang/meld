/**
 * **Feature: meld-daemon, Property 42: Range Formatting Precision**
 *
 * For any selected code range, formatting SHALL only modify the selected
 * region while preserving surrounding formatting.
 *
 * **Validates: Requirements 19.2**
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
 * Property 42a: Range formatting edits are a subset of full document edits,
 * restricted to the specified range.
 */
RC_GTEST_PROP(RangeFormattingPrecision,
              RangeEditsAreSubsetOfDocumentEdits,
              ()) {
    auto symbols = *rc::gen::container<std::vector<SymbolDef>>(
        5, genSymbolDef());
    std::vector<SymbolDef> unique_symbols;
    std::set<std::string> seen;
    for (auto& s : symbols) {
        if (seen.insert(s.name).second)
            unique_symbols.push_back(s);
    }
    RC_PRE(unique_symbols.size() >= 2);

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, unique_symbols);

    FormattingProvider provider(model);
    auto all_edits = provider.format_document(file);

    // Pick a range covering the middle of the document
    Range range;
    range.start_line = 0;
    range.start_column = 0;
    range.end_line = 50;
    range.end_column = 1000;

    auto range_edits = provider.format_range(file, range);

    // Every range edit must also appear in the full document edits
    for (const auto& re : range_edits) {
        bool found = std::any_of(all_edits.begin(), all_edits.end(),
            [&](const TextEdit& de) {
                return de.start_line == re.start_line &&
                       de.start_column == re.start_column &&
                       de.end_line == re.end_line &&
                       de.end_column == re.end_column &&
                       de.new_text == re.new_text;
            });
        RC_ASSERT(found);
    }
}

/**
 * Property 42b: Range formatting does not produce edits outside the range.
 */
RC_GTEST_PROP(RangeFormattingPrecision,
              NoEditsOutsideRange,
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

    auto range_start = *rc::gen::inRange(0, 50);
    auto range_end = *rc::gen::inRange(range_start + 1, 101);

    Range range;
    range.start_line = static_cast<uint32_t>(range_start);
    range.start_column = 0;
    range.end_line = static_cast<uint32_t>(range_end);
    range.end_column = 1000;

    FormattingProvider provider(model);
    auto edits = provider.format_range(file, range);

    for (const auto& edit : edits) {
        // Edit must start within or after range start
        bool after_start = edit.start_line > range.start_line ||
                           (edit.start_line == range.start_line &&
                            edit.start_column >= range.start_column);
        // Edit must end within or before range end
        bool before_end = edit.end_line < range.end_line ||
                          (edit.end_line == range.end_line &&
                           edit.end_column <= range.end_column);
        RC_ASSERT(after_start);
        RC_ASSERT(before_end);
    }
}

/**
 * Property 42c: An empty range produces no edits.
 */
RC_GTEST_PROP(RangeFormattingPrecision,
              EmptyRangeProducesNoEdits,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {{"foo", "function_definition", 10, 0}});

    // Range that doesn't overlap with any symbol
    Range range;
    range.start_line = 200;
    range.start_column = 0;
    range.end_line = 200;
    range.end_column = 0;

    FormattingProvider provider(model);
    auto edits = provider.format_range(file, range);
    RC_ASSERT(edits.empty());
}

}  // namespace
}  // namespace meld::daemon
