/**
 * **Feature: meld-lsp-server, Property 5: Parse error recovery**
 *
 * For any Meld file with parsing errors, the LSP server should continue
 * processing the remainder of the file after encountering errors.
 *
 * **Validates: Requirements 1.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/meld_parser_adapter.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

namespace {

using namespace meld::lsp::integration;
using namespace meld::lsp::services;

// ---------------------------------------------------------------------------
// Generators — produce Meld source with intentional errors
// ---------------------------------------------------------------------------

/// Generate a simple identifier
rc::Gen<std::string> genId() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 5 + 3) % 26));
            }
            return s;
        }
    );
}

/// Generate a valid Meld statement
rc::Gen<std::string> genValidStatement() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "val " + name + " = " + std::to_string(val);
        }
    );
}

/// Generate a syntactically broken statement (missing value after =)
rc::Gen<std::string> genBrokenAssignment() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "val " + name + " = ";
        }
    );
}

/// Generate an unclosed string literal
rc::Gen<std::string> genUnclosedString() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "val " + name + " = \"unclosed";
        }
    );
}

/// Generate a broken function (missing closing brace)
rc::Gen<std::string> genBrokenFunction() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "fnc " + name + "() {\n    val x = 42";
        }
    );
}

/// Generate source with an error line followed by valid code
rc::Gen<std::string> genErrorThenValid() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, std::string, int>& t) {
            auto [bad_name, good_name, val] = t;
            return "val " + bad_name + " = \n" +
                   "val " + good_name + " = " + std::to_string(val);
        }
    );
}

/// Generate source with valid code, then error, then more valid code
rc::Gen<std::string> genValidErrorValid() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), genId(),
                       rc::gen::inRange(0, 99), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, std::string, std::string, int, int>& t) {
            auto [name1, bad_name, name2, v1, v2] = t;
            return "val " + name1 + " = " + std::to_string(v1) + "\n" +
                   "val " + bad_name + " = \n" +
                   "val " + name2 + " = " + std::to_string(v2);
        }
    );
}

/// Generate source with multiple errors interspersed with valid code
rc::Gen<std::string> genMultipleErrors() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, std::string, int>& t) {
            auto [name1, name2, val] = t;
            std::ostringstream oss;
            oss << "val " << name1 << " = " << val << "\n";
            oss << "fnc broken( {\n";  // syntax error
            oss << "val " << name2 << " = " << (val + 1) << "\n";
            oss << "struct {\n";  // another error — missing name
            oss << "val final_val = 100\n";
            return oss.str();
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 5a: Source with errors still produces partial semantic tokens.
 *
 * For any Meld source containing syntax errors, the adapter must still
 * produce semantic tokens from the portions it can tokenize.
 */
TEST(ParseErrorRecoveryPropertyTest, ErrorsStillProducePartialTokens) {
    rc::check("Source with errors must still produce partial semantic tokens",
        []() {
            auto source = *rc::gen::oneOf(
                genBrokenAssignment(),
                genBrokenFunction(),
                genUnclosedString()
            );

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            // Even with errors, the lexer should produce tokens for the
            // valid portions of the source
            RC_ASSERT(!result.tokens.empty());

            // Should have at least a keyword token (val or fnc)
            bool has_keyword = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Keyword;
                });
            RC_ASSERT(has_keyword);
        }
    );
}

/**
 * Property 5b: Valid code after an error is still tokenized.
 *
 * For any source where an error line is followed by valid code,
 * the adapter must produce tokens covering the valid portion.
 */
TEST(ParseErrorRecoveryPropertyTest, ValidCodeAfterErrorIsTokenized) {
    rc::check("Valid code after an error must still be tokenized",
        []() {
            auto source = *genErrorThenValid();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            // Must produce tokens
            RC_ASSERT(!result.tokens.empty());

            // Should have at least 2 keyword tokens (two 'val' keywords)
            int keyword_count = static_cast<int>(std::count_if(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Keyword;
                }));
            RC_ASSERT(keyword_count >= 2);
        }
    );
}

/**
 * Property 5c: Tokens from valid code surrounding errors are preserved.
 *
 * For any source with valid-error-valid pattern, tokens from both
 * valid sections must be present.
 */
TEST(ParseErrorRecoveryPropertyTest, TokensFromSurroundingValidCodePreserved) {
    rc::check("Tokens from valid code surrounding errors must be preserved",
        []() {
            auto source = *genValidErrorValid();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            RC_ASSERT(!result.tokens.empty());

            // Count number tokens — should have at least 2 (one from each valid section)
            int number_count = static_cast<int>(std::count_if(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Number;
                }));
            RC_ASSERT(number_count >= 2);
        }
    );
}

/**
 * Property 5d: Multiple errors don't prevent tokenization of valid parts.
 *
 * For any source with multiple syntax errors interspersed with valid code,
 * the adapter must still produce tokens for the valid portions.
 */
TEST(ParseErrorRecoveryPropertyTest, MultipleErrorsDontPreventTokenization) {
    rc::check("Multiple errors must not prevent tokenization of valid parts",
        []() {
            auto source = *genMultipleErrors();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            // Must produce tokens despite multiple errors
            RC_ASSERT(!result.tokens.empty());

            // Should have keyword tokens from the valid declarations
            bool has_keyword = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Keyword;
                });
            RC_ASSERT(has_keyword);

            // Should have number tokens from the valid val declarations
            bool has_number = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Number;
                });
            RC_ASSERT(has_number);
        }
    );
}

/**
 * Property 5e: has_partial_results is true when parsing fails but tokens exist.
 *
 * For any source that fails to parse but has tokenizable content,
 * the result must indicate partial results are available.
 */
TEST(ParseErrorRecoveryPropertyTest, PartialResultsFlagSetOnFailure) {
    rc::check("has_partial_results must be true when parse fails but tokens exist",
        []() {
            auto source = *genBrokenFunction();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            if (!result.success && !result.tokens.empty()) {
                RC_ASSERT(result.has_partial_results);
            }
        }
    );
}

/**
 * Property 5f: Cache stores results even for files with errors.
 *
 * For any source with errors, parse_document must still cache the result
 * so subsequent requests don't re-parse.
 */
TEST(ParseErrorRecoveryPropertyTest, CacheStoresResultsWithErrors) {
    rc::check("Cache must store results even for files with parse errors",
        []() {
            auto source = *genBrokenFunction();
            std::string uri = "file:///broken.meld";

            MeldParserAdapter adapter;
            adapter.parse_document(uri, source, 1);

            auto cached = adapter.get_cached(uri);
            RC_ASSERT(cached.has_value());
            RC_ASSERT(!cached->tokens.empty());
        }
    );
}

/**
 * Property 5g: Invalidation removes cached results.
 */
TEST(ParseErrorRecoveryPropertyTest, InvalidationRemovesCachedResults) {
    MeldParserAdapter adapter;
    std::string uri = "file:///test.meld";

    adapter.parse_document(uri, "val x = 42", 1);
    ASSERT_TRUE(adapter.get_cached(uri).has_value());

    adapter.invalidate(uri);
    EXPECT_FALSE(adapter.get_cached(uri).has_value());
}
