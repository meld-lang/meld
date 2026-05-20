/**
 * **Feature: meld-daemon, Property 21: Semantic Token Provision**
 *
 * For any valid Meld file, the LspChannel SHALL provide semantic tokens
 * that enable syntax highlighting.
 *
 * **Validates: Requirements 15.1**
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
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 10),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        }
    );
}

rc::Gen<std::string> genMeldFunction() {
    return rc::gen::map(
        rc::gen::tuple(genIdentifier(), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "fnc " + name + "() {\n    val result = " +
                   std::to_string(val) + "\n    return result\n}";
        }
    );
}

rc::Gen<std::string> genMeldStruct() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            std::string n = name;
            if (!n.empty()) n[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(n[0])));
            return "struct " + n + " {\n    val x = 0\n}";
        }
    );
}

rc::Gen<std::string> genMeldComment() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            return "// comment about " + name;
        }
    );
}

rc::Gen<std::string> genMeldSourceFile() {
    return rc::gen::mapcat(
        rc::gen::inRange(1, 4),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<std::string>>(
                    count,
                    rc::gen::oneOf(
                        genMeldFunction(),
                        genMeldStruct(),
                        genMeldComment()
                    )
                ),
                [](const std::vector<std::string>& items) {
                    std::ostringstream oss;
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (i > 0) oss << "\n\n";
                        oss << items[i];
                    }
                    return oss.str();
                }
            );
        }
    );
}

rc::Gen<std::string> genMeldKeyword() {
    return rc::gen::elementOf(std::vector<std::string>{
        "fnc", "let", "val", "var", "if", "else", "match", "return",
        "import", "struct", "enum", "trait", "impl",
        "effect", "handle", "perform", "async", "await",
        "for", "while", "true", "false"
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 21a: Any non-empty valid Meld source produces semantic tokens.
 */
RC_GTEST_PROP(SemanticTokenProvisionProperty,
              ValidMeldFileProducesSemanticTokens,
              ()) {
    auto source = *genMeldSourceFile();
    RC_PRE(!source.empty());

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());

    for (const auto& token : result.tokens) {
        RC_ASSERT(token.line >= 0);
        RC_ASSERT(token.start_char >= 0);
        RC_ASSERT(token.length > 0);
    }
}

/**
 * Property 21b: Encoded semantic tokens have exactly 5 integers per token.
 */
RC_GTEST_PROP(SemanticTokenProvisionProperty,
              EncodedTokensHaveCorrectSize,
              ()) {
    auto source = *genMeldSourceFile();
    RC_PRE(!source.empty());

    SemanticTokenProvider provider;
    auto result = provider.parse(source);
    auto encoded = SemanticTokenProvider::encode_semantic_tokens(result.tokens);

    RC_ASSERT(encoded.size() == result.tokens.size() * 5);
}

/**
 * Property 21c: Keywords in Meld source are classified as Keyword tokens.
 */
RC_GTEST_PROP(SemanticTokenProvisionProperty,
              KeywordsAreClassifiedCorrectly,
              ()) {
    auto keyword = *genMeldKeyword();
    std::string source = keyword + " something";

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool has_keyword = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(has_keyword);
}

/**
 * Property 21d: Delta-encoded token positions are non-negative.
 */
RC_GTEST_PROP(SemanticTokenProvisionProperty,
              DeltaEncodedPositionsAreNonNegative,
              ()) {
    auto source = *genMeldSourceFile();
    RC_PRE(!source.empty());

    SemanticTokenProvider provider;
    auto result = provider.parse(source);
    auto encoded = SemanticTokenProvider::encode_semantic_tokens(result.tokens);

    for (size_t i = 0; i < encoded.size(); i += 5) {
        RC_ASSERT(encoded[i] >= 0);      // delta line
        RC_ASSERT(encoded[i + 1] >= 0);  // delta char
        RC_ASSERT(encoded[i + 2] > 0);   // length
    }
}

}  // namespace
}  // namespace meld::daemon
