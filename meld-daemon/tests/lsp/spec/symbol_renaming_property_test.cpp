/**
 * **Feature: meld-lsp-server, Property 23: Symbol renaming completeness**
 *
 * For any symbol rename operation, all references across the workspace
 * should be updated correctly.
 *
 * **Validates: Requirements 5.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>
#include <algorithm>

namespace {

using namespace meld::lsp::services;

/// Helper: count occurrences of a word (whole-word match) in text
int count_word_occurrences(const std::string& text, const std::string& word) {
    int count = 0;
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        // Check word boundaries
        bool left_ok = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(text[pos - 1])) &&
                                      text[pos - 1] != '_'));
        size_t end = pos + word.size();
        bool right_ok = (end >= text.size() || (!std::isalnum(static_cast<unsigned char>(text[end])) &&
                                                 text[end] != '_'));
        if (left_ok && right_ok) count++;
        pos += word.size();
    }
    return count;
}

} // anonymous namespace

/**
 * Property 23.1: Renaming a function updates all references including
 * the declaration and all call sites.
 */
TEST(SymbolRenamingPropertyTest, FunctionRenameUpdatesAllReferences) {
    rc::check("Renaming a function produces edits for all occurrences",
        []() {
            int num_calls = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            oss << "fnc compute(x: Int) -> Int {\n";
            oss << "    return x\n";
            oss << "}\n\n";
            oss << "fnc main() {\n";
            for (int i = 0; i < num_calls; ++i) {
                oss << "    let r_" << i << " = compute(" << i << ")\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            int total_occurrences = count_word_occurrences(source, "compute");

            LanguageService service;

            // Rename "compute" at its declaration (line 0, col 4)
            auto result = service.rename_symbol("file:///test.meld", source,
                                                 0, 4, "calculate");

            RC_ASSERT(result.success);
            RC_ASSERT(static_cast<int>(result.edits.size()) == total_occurrences);

            // Every edit should replace "compute" with "calculate"
            for (const auto& edit : result.edits) {
                RC_ASSERT(edit.new_text == "calculate");
            }
        }
    );
}

/**
 * Property 23.2: Renaming a variable updates all references within
 * the document.
 */
TEST(SymbolRenamingPropertyTest, VariableRenameUpdatesAllReferences) {
    rc::check("Renaming a variable produces edits for all occurrences",
        []() {
            int num_uses = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            oss << "fnc example() {\n";
            oss << "    let counter = 0\n";
            for (int i = 0; i < num_uses; ++i) {
                oss << "    let use_" << i << " = counter\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            int total_occurrences = count_word_occurrences(source, "counter");

            LanguageService service;

            // Rename "counter" at its declaration (line 1, col 8)
            auto result = service.rename_symbol("file:///test.meld", source,
                                                 1, 8, "total");

            RC_ASSERT(result.success);
            RC_ASSERT(static_cast<int>(result.edits.size()) == total_occurrences);

            for (const auto& edit : result.edits) {
                RC_ASSERT(edit.new_text == "total");
            }
        }
    );
}

/**
 * Property 23.3: Rename edits cover the exact character range of each
 * symbol occurrence.
 */
TEST(SymbolRenamingPropertyTest, EditRangesMatchSymbolLength) {
    rc::check("Each rename edit spans exactly the length of the old symbol name",
        []() {
            std::string source =
                "fnc process(data: Int) -> Int {\n"
                "    return data\n"
                "}\n\n"
                "fnc main() {\n"
                "    let result = process(42)\n"
                "}\n";

            LanguageService service;

            auto result = service.rename_symbol("file:///test.meld", source,
                                                 0, 4, "transform");

            RC_ASSERT(result.success);
            int old_len = 7;  // "process"
            for (const auto& edit : result.edits) {
                // Edit should span exactly the old name length
                if (edit.start_line == edit.end_line) {
                    RC_ASSERT(edit.end_character - edit.start_character == old_len);
                }
            }
        }
    );
}

/**
 * Property 23.4: Renaming with an empty cursor position returns an error.
 */
TEST(SymbolRenamingPropertyTest, NoSymbolAtPositionReturnsError) {
    rc::check("Rename at a position with no symbol returns an error",
        []() {
            std::string source = "fnc hello() {\n    return 42\n}\n";

            LanguageService service;

            // Position on whitespace (line 1, col 0 is a space)
            auto result = service.rename_symbol("file:///test.meld", source,
                                                 1, 0, "new_name");

            // Should fail — no symbol at whitespace
            RC_ASSERT(!result.success || result.edits.empty());
        }
    );
}
