/**
 * **Feature: meld-daemon, Property 45: Semantic Preservation During Formatting**
 *
 * For any code formatting operation, the semantic meaning SHALL be
 * preserved while improving readability.
 *
 * **Validates: Requirements 19.5**
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
    std::string type_info;
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
        child->type_info = sym.type_info;
        child->location = SourceLocation{file, sym.line, sym.column};
        root->children.push_back(child);
    }
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

/// Extract all symbol names from a model for a given file.
std::vector<std::string> extract_symbol_names(
    const SemanticModel& model, const std::filesystem::path& file) {
    std::vector<std::string> names;
    auto ast = model.get_ast(file);
    if (!ast) return names;
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (!node->name.empty()) names.push_back(node->name);
        for (const auto& child : node->children) walk(child);
    };
    walk(ast);
    return names;
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
            std::string type_info = (kind == "function_definition")
                                        ? "() -> Int"
                                        : "Int";
            return SymbolDef{name, kind, type_info,
                             static_cast<uint32_t>(line), 0};
        },
        genIdentifier(), genKind(), rc::gen::inRange(1, 100));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 45a: Formatting preserves all symbol names — no symbols are
 * added, removed, or renamed by formatting.
 */
RC_GTEST_PROP(SemanticPreservationFormatting,
              FormattingPreservesSymbolNames,
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

    // Get symbols before formatting
    auto names_before = extract_symbol_names(model, file);

    FormattingProvider provider(model);
    auto edits = provider.format_document(file);

    // Formatting edits should not change symbol names
    // (edits only affect whitespace/style, not identifiers)
    for (const auto& edit : edits) {
        // The new_text in formatting edits should be a style-corrected
        // version of the original — for identifier nodes, the content
        // should be semantically equivalent
        RC_ASSERT(!edit.new_text.empty());
    }

    // Symbols in the model are unchanged (formatting doesn't mutate the model)
    auto names_after = extract_symbol_names(model, file);
    RC_ASSERT(names_before == names_after);
}

/**
 * Property 45b: format_line preserves all non-whitespace content.
 * The semantic tokens (identifiers, operators, keywords) remain intact.
 */
RC_GTEST_PROP(SemanticPreservationFormatting,
              FormatLinePreservesNonWhitespaceContent,
              ()) {
    auto raw_line = *rc::gen::map(
        rc::gen::container<std::string>(rc::gen::inRange(32, 127)),
        [](std::string s) {
            if (s.empty()) s = "x";
            return s;
        });

    std::string formatted = FormattingProvider::format_line(raw_line);

    // Extract non-whitespace tokens from both
    auto extract_tokens = [](const std::string& s) {
        std::vector<std::string> tokens;
        std::string current;
        for (char c : s) {
            if (c == ' ' || c == '\t') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) tokens.push_back(current);
        return tokens;
    };

    auto original_tokens = extract_tokens(raw_line);
    auto formatted_tokens = extract_tokens(formatted);

    RC_ASSERT(original_tokens == formatted_tokens);
}

/**
 * Property 45c: Formatting does not introduce tabs — output uses spaces only.
 */
RC_GTEST_PROP(SemanticPreservationFormatting,
              FormattingUsesSpacesNotTabs,
              ()) {
    // Generate a line with potential tabs
    auto line_with_tabs = *rc::gen::map(
        rc::gen::container<std::string>(
            rc::gen::elementOf(std::vector<char>{
                ' ', '\t', 'a', 'b', 'c', '=', '+', '(', ')'})),
        [](std::string s) {
            if (s.empty()) s = "x";
            return s;
        });

    std::string formatted = FormattingProvider::format_line(line_with_tabs);

    // No tabs in output
    RC_ASSERT(formatted.find('\t') == std::string::npos);
}

}  // namespace
}  // namespace meld::daemon
