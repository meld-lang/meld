/**
 * **Feature: meld-daemon, Property 25: Parse Error Recovery**
 *
 * For any Meld file with parsing errors, the LspChannel SHALL continue
 * processing the remainder of the file after encountering errors.
 *
 * **Validates: Requirements 15.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/semantic_token_provider.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators — produce Meld source with intentional errors
// ---------------------------------------------------------------------------

rc::Gen<std::string> genId() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + ((i * 5 + 3) % 26));
            return s;
        }
    );
}

rc::Gen<std::string> genBrokenAssignment() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "val " + name + " = ";
    });
}

rc::Gen<std::string> genUnclosedString() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "val " + name + " = \"unclosed";
    });
}

rc::Gen<std::string> genBrokenFunction() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "fnc " + name + "() {\n    val x = 42";
    });
}

rc::Gen<std::string> genErrorThenValid() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, std::string, int>& t) {
            auto [bad_name, good_name, val] = t;
            return "val " + bad_name + " = \"unclosed\n" +
                   "val " + good_name + " = " + std::to_string(val);
        }
    );
}

rc::Gen<std::string> genValidErrorValid() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), genId(),
                       rc::gen::inRange(0, 99), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, std::string, std::string, int, int>& t) {
            auto [name1, bad_name, name2, v1, v2] = t;
            return "val " + name1 + " = " + std::to_string(v1) + "\n" +
                   "val " + bad_name + " = \"unclosed\n" +
                   "val " + name2 + " = " + std::to_string(v2);
        }
    );
}

rc::Gen<std::string> genMultipleErrors() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, std::string, int>& t) {
            auto [name1, name2, val] = t;
            std::ostringstream oss;
            oss << "val " << name1 << " = " << val << "\n";
            oss << "fnc broken( {\n";
            oss << "val " << name2 << " = " << (val + 1) << "\n";
            oss << "struct {\n";
            oss << "val final_val = 100\n";
            return oss.str();
        }
    );
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 25a: Source with errors still produces partial semantic tokens.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              ErrorsStillProducePartialTokens,
              ()) {
    auto source = *rc::gen::oneOf(
        genBrokenAssignment(),
        genBrokenFunction(),
        genUnclosedString()
    );

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());

    bool has_keyword = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(has_keyword);
}

/**
 * Property 25b: Valid code after an error is still tokenized.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              ValidCodeAfterErrorIsTokenized,
              ()) {
    auto source = *genErrorThenValid();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());

    int keyword_count = static_cast<int>(std::count_if(
        result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; }));
    RC_ASSERT(keyword_count >= 2);
}

/**
 * Property 25c: Tokens from valid code surrounding errors are preserved.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              TokensFromSurroundingValidCodePreserved,
              ()) {
    auto source = *genValidErrorValid();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());

    int number_count = static_cast<int>(std::count_if(
        result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Number; }));
    RC_ASSERT(number_count >= 2);
}

/**
 * Property 25d: Multiple errors don't prevent tokenization of valid parts.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              MultipleErrorsDontPreventTokenization,
              ()) {
    auto source = *genMultipleErrors();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());

    bool has_keyword = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(has_keyword);

    bool has_number = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Number; });
    RC_ASSERT(has_number);
}

/**
 * Property 25e: has_partial_results is true when parsing fails but tokens exist.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              PartialResultsFlagSetOnFailure,
              ()) {
    auto source = *genBrokenFunction();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    if (!result.success && !result.tokens.empty()) {
        RC_ASSERT(result.has_partial_results);
    }
}

/**
 * Property 25f: Cache stores results even for files with errors.
 */
RC_GTEST_PROP(ParseErrorRecoveryProperty,
              CacheStoresResultsWithErrors,
              ()) {
    auto source = *genBrokenFunction();
    std::string uri = "file:///broken.meld";

    SemanticTokenProvider provider;
    provider.parse_document(uri, source, 1);

    auto cached = provider.get_cached(uri);
    RC_ASSERT(cached.has_value());
    RC_ASSERT(!cached->tokens.empty());
}

}  // namespace
}  // namespace meld::daemon
