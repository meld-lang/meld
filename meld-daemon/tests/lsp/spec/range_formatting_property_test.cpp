/**
 * **Feature: meld-lsp-server, Property 22: Range formatting precision**
 *
 * For any selected code range, formatting should only modify the selected
 * region while preserving surrounding formatting.
 *
 * **Validates: Requirements 5.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>
#include <vector>

namespace {

using namespace meld::lsp::services;

/// Helper: split string into lines
std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream stream(s);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

/// Helper: apply line-level edits to content
std::string apply_line_edits(const std::string& content,
                             const std::vector<LanguageService::TextEdit>& edits) {
    auto lines = split_lines(content);
    // Apply edits in reverse order to preserve line numbers
    auto sorted_edits = edits;
    std::sort(sorted_edits.begin(), sorted_edits.end(),
              [](const LanguageService::TextEdit& a, const LanguageService::TextEdit& b) {
                  return a.start_line > b.start_line;
              });
    for (const auto& edit : sorted_edits) {
        if (edit.start_line >= 0 && edit.start_line < static_cast<int>(lines.size())) {
            lines[edit.start_line] = edit.new_text;
        }
    }
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += "\n";
        result += lines[i];
    }
    return result;
}

} // anonymous namespace

/**
 * Property 22.1: Range formatting only produces edits within the
 * specified line range.
 */
TEST(RangeFormattingPropertyTest, EditsOnlyWithinRange) {
    rc::check("Range formatting edits are confined to the specified range",
        []() {
            // Build a multi-function document
            std::ostringstream oss;
            oss << "fnc first() {\n";
            oss << "    let a = 1\n";
            oss << "}\n\n";
            oss << "fnc second() {\n";
            oss << "      let b = 2\n";  // intentionally bad indent
            oss << "}\n\n";
            oss << "fnc third() {\n";
            oss << "    let c = 3\n";
            oss << "}\n";

            std::string source = oss.str();

            // Format only the second function (lines 4-6)
            int range_start = 4;
            int range_end = 6;

            LanguageService service;
            auto edits = service.format_range("file:///test.meld", source,
                                              range_start, range_end);

            // All edits must be within the specified range
            for (const auto& edit : edits) {
                RC_ASSERT(edit.start_line >= range_start);
                RC_ASSERT(edit.end_line <= range_end);
            }
        }
    );
}

/**
 * Property 22.2: Lines outside the formatted range remain unchanged.
 */
TEST(RangeFormattingPropertyTest, SurroundingLinesPreserved) {
    rc::check("Lines outside the range are not modified",
        []() {
            std::ostringstream oss;
            oss << "fnc alpha() {\n";
            oss << "    let x = 10\n";
            oss << "}\n\n";
            oss << "fnc beta() {\n";
            oss << "        let y = 20\n";  // bad indent
            oss << "}\n\n";
            oss << "fnc gamma() {\n";
            oss << "    let z = 30\n";
            oss << "}\n";

            std::string source = oss.str();
            auto original_lines = split_lines(source);

            int range_start = 4;
            int range_end = 6;

            LanguageService service;
            auto edits = service.format_range("file:///test.meld", source,
                                              range_start, range_end);

            // Verify no edits touch lines outside the range
            for (const auto& edit : edits) {
                RC_ASSERT(edit.start_line >= range_start);
                RC_ASSERT(edit.start_line <= range_end);
            }

            // Apply edits and verify surrounding lines are unchanged
            std::string result = apply_line_edits(source, edits);
            auto result_lines = split_lines(result);

            for (int i = 0; i < range_start && i < static_cast<int>(original_lines.size()); ++i) {
                RC_ASSERT(i < static_cast<int>(result_lines.size()));
                RC_ASSERT(result_lines[i] == original_lines[i]);
            }
            for (int i = range_end + 1; i < static_cast<int>(original_lines.size()); ++i) {
                RC_ASSERT(i < static_cast<int>(result_lines.size()));
                RC_ASSERT(result_lines[i] == original_lines[i]);
            }
        }
    );
}

/**
 * Property 22.3: Range formatting respects brace depth from preceding code.
 */
TEST(RangeFormattingPropertyTest, RespectsContextDepth) {
    rc::check("Range formatting uses correct indentation from surrounding context",
        []() {
            int num_inner = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            oss << "fnc outer() {\n";
            for (int i = 0; i < num_inner; ++i) {
                // Intentionally wrong indentation inside function body
                int bad_indent = *rc::gen::inRange(0, 10);
                std::string spaces(bad_indent, ' ');
                oss << spaces << "let v_" << i << " = " << i << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            // Format only the body lines (line 1 to num_inner)
            int range_start = 1;
            int range_end = num_inner;

            LanguageService service;
            auto edits = service.format_range("file:///test.meld", source,
                                              range_start, range_end);

            // Apply edits and check indentation is 4 spaces (depth 1)
            std::string result = apply_line_edits(source, edits);
            auto result_lines = split_lines(result);

            for (int i = range_start; i <= range_end && i < static_cast<int>(result_lines.size()); ++i) {
                const auto& line = result_lines[i];
                if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
                // Should be indented at depth 1 (4 spaces)
                int spaces = 0;
                for (char c : line) {
                    if (c == ' ') spaces++;
                    else break;
                }
                RC_ASSERT(spaces == 4);
            }
        }
    );
}

/**
 * Property 22.4: Empty or out-of-bounds range produces no edits.
 */
TEST(RangeFormattingPropertyTest, InvalidRangeProducesNoEdits) {
    rc::check("Invalid or empty range produces no edits",
        []() {
            std::string source = "fnc hello() {\n    return 42\n}\n";

            LanguageService service;

            // Range beyond document
            auto edits1 = service.format_range("file:///test.meld", source, 100, 200);
            RC_ASSERT(edits1.empty());

            // Inverted range
            auto edits2 = service.format_range("file:///test.meld", source, 2, 0);
            RC_ASSERT(edits2.empty());
        }
    );
}
