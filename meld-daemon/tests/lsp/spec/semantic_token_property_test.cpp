/**
 * **Feature: meld-lsp-server, Property 1: Semantic token provision**
 *
 * For any valid Meld file, the LSP server should provide semantic tokens
 * that enable syntax highlighting.
 *
 * **Validates: Requirements 1.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

namespace {

using namespace meld::lsp::services;

/// Generate a random Meld keyword
rc::Gen<std::string> genMeldKeyword() {
    static const std::vector<std::string> keywords = {
        "fnc", "let", "var", "if", "else", "match", "return",
        "import", "struct", "enum", "trait", "impl",
        "effect", "handle", "perform", "async", "await",
        "for", "while", "true", "false"
    };
    return rc::gen::elementOf(keywords);
}

/// Generate a random Meld identifier
rc::Gen<std::string> genMeldIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 12),
        [](int len) {
            std::string result;
            result.reserve(len);
            // Simple deterministic-ish identifier generation
            for (int i = 0; i < len; ++i) {
                result += static_cast<char>('a' + (i % 26));
            }
            return result;
        }
    );
}

/// Generate a simple Meld function definition
rc::Gen<std::string> genMeldFunction() {
    return rc::gen::map(
        genMeldIdentifier(),
        [](const std::string& name) {
            return "fnc " + name + "() {\n    let x = 42\n    return x\n}";
        }
    );
}

/// Generate a Meld struct definition
rc::Gen<std::string> genMeldStruct() {
    return rc::gen::map(
        genMeldIdentifier(),
        [](const std::string& name) {
            return "struct " + name + " {\n    let value = 0\n}";
        }
    );
}

/// Generate a comment line
rc::Gen<std::string> genMeldComment() {
    return rc::gen::map(
        genMeldIdentifier(),
        [](const std::string& name) {
            return "// comment about " + name;
        }
    );
}

/// Generate a valid Meld source file by combining constructs
rc::Gen<std::string> genMeldSourceFile() {
    return rc::gen::mapcat(
        rc::gen::inRange(1, 5),
        [](int num_items) {
            return rc::gen::map(
                rc::gen::container<std::vector<std::string>>(
                    num_items,
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

} // anonymous namespace

/**
 * Property 1: Semantic token provision
 *
 * For any valid Meld source file, the language service MUST provide
 * a non-empty set of semantic tokens that enable syntax highlighting.
 */
TEST(SemanticTokenProvisionPropertyTest, ValidMeldFileProducesSemanticTokens) {
    rc::check("Any non-empty valid Meld source must produce semantic tokens with valid positions",
        []() {
            auto source = *genMeldSourceFile();
            RC_PRE(!source.empty());

            LanguageService service;
            auto tokens = service.get_semantic_tokens("file:///test.meld", source);

            // Property: any non-empty valid Meld source must produce at least one semantic token
            RC_ASSERT(!tokens.empty());

            // Property: all tokens must have valid positions (non-negative)
            for (const auto& token : tokens) {
                RC_ASSERT(token.line >= 0);
                RC_ASSERT(token.start_char >= 0);
                RC_ASSERT(token.length > 0);
            }
        }
    );
}

/**
 * Property 1 (continued): Encoded semantic tokens are well-formed
 *
 * For any set of semantic tokens, the delta-encoded representation
 * must have exactly 5 integers per token.
 */
TEST(SemanticTokenProvisionPropertyTest, EncodedTokensHaveCorrectSize) {
    rc::check("Encoded tokens must have exactly 5 integers per token",
        []() {
            auto source = *genMeldSourceFile();
            RC_PRE(!source.empty());

            LanguageService service;
            auto tokens = service.get_semantic_tokens("file:///test.meld", source);
            auto encoded = service.encode_semantic_tokens(tokens);

            // Property: encoded size must be exactly 5 * number of tokens
            RC_ASSERT(encoded.size() == tokens.size() * 5);
        }
    );
}

/**
 * Property 1 (continued): Semantic tokens cover Meld keywords
 *
 * For any Meld source containing a keyword, at least one token
 * must be classified as a Keyword type.
 */
TEST(SemanticTokenProvisionPropertyTest, KeywordsAreClassifiedCorrectly) {
    rc::check("Keywords in Meld source must be classified as Keyword tokens",
        []() {
            auto keyword = *genMeldKeyword();
            // Wrap keyword in a simple context so it's parseable
            std::string source = keyword + " something";

            LanguageService service;
            auto tokens = service.get_semantic_tokens("file:///test.meld", source);

            // Property: the keyword must appear as a Keyword token
            bool has_keyword_token = std::any_of(tokens.begin(), tokens.end(),
                [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
            RC_ASSERT(has_keyword_token);
        }
    );
}

/**
 * Property 1 (continued): Token positions are monotonically ordered
 *
 * For any Meld source, the delta-encoded tokens must have
 * non-negative delta values (monotonic ordering).
 */
TEST(SemanticTokenProvisionPropertyTest, DeltaEncodedPositionsAreNonNegative) {
    rc::check("Delta-encoded token positions must be non-negative",
        []() {
            auto source = *genMeldSourceFile();
            RC_PRE(!source.empty());

            LanguageService service;
            auto tokens = service.get_semantic_tokens("file:///test.meld", source);
            auto encoded = service.encode_semantic_tokens(tokens);

            // Property: delta line and delta char values must be non-negative
            for (size_t i = 0; i < encoded.size(); i += 5) {
                RC_ASSERT(encoded[i] >= 0);     // delta line
                RC_ASSERT(encoded[i + 1] >= 0); // delta char
                RC_ASSERT(encoded[i + 2] > 0);  // length must be positive
            }
        }
    );
}
