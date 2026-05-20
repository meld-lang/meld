/**
 * **Feature: meld-daemon, Property 22: Language Construct Classification**
 *
 * For any Meld source code, all language constructs SHALL be correctly
 * identified and classified by type (symbols, literals, keywords,
 * operators, comments).
 *
 * **Validates: Requirements 15.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/semantic_token_provider.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genKeyword() {
    return rc::gen::elementOf(std::vector<std::string>{
        "fnc", "let", "val", "var", "if", "else", "match", "return",
        "import", "struct", "enum", "trait", "impl",
        "effect", "handle", "perform", "async", "await",
        "for", "while", "true", "false"
    });
}

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + ((i * 7 + 3) % 26));
            return s;
        }
    );
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& id) {
            std::string r = id;
            if (!r.empty()) r[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(r[0])));
            return r;
        }
    );
}

rc::Gen<std::string> genStringLiteral() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& content) {
            return "\"" + content + "\"";
        }
    );
}

rc::Gen<std::string> genIntLiteral() {
    return rc::gen::map(
        rc::gen::inRange(0, 9999),
        [](int val) { return std::to_string(val); }
    );
}

rc::Gen<std::string> genEffectAnnotation() {
    return rc::gen::elementOf(std::vector<std::string>{
        "@effect", "@uses", "@pure", "@handler"
    });
}

rc::Gen<std::string> genOperator() {
    return rc::gen::elementOf(std::vector<std::string>{
        "+", "-", "*", "/", "==", "!=", "<", ">", "<=", ">=",
        "|>", "->", "=>", "&&", "||"
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 22a: Keywords are classified as Keyword tokens.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              KeywordsAreClassifiedAsKeyword,
              ()) {
    auto kw = *genKeyword();
    std::string source = kw + " x = 42";

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool found = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(found);
}

/**
 * Property 22b: String literals are classified as String tokens.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              StringLiteralsAreClassifiedAsString,
              ()) {
    auto str = *genStringLiteral();
    std::string source = "val x = " + str;

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool found = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::String; });
    RC_ASSERT(found);
}

/**
 * Property 22c: Number literals are classified as Number tokens.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              NumberLiteralsAreClassifiedAsNumber,
              ()) {
    auto num = *genIntLiteral();
    std::string source = "val x = " + num;

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool found = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Number; });
    RC_ASSERT(found);
}

/**
 * Property 22d: Effect annotations are classified as Decorator tokens.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              EffectAnnotationsAreClassifiedAsDecorator,
              ()) {
    auto ann = *genEffectAnnotation();
    std::string source = ann + "\nfnc foo() {}";

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool found = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Decorator; });
    RC_ASSERT(found);
}

/**
 * Property 22e: Operators are classified as Operator tokens.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              OperatorsAreClassifiedAsOperator,
              ()) {
    auto op = *genOperator();
    std::string source = "val x = 1 " + op + " 2";

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    bool found = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Operator; });
    RC_ASSERT(found);
}

/**
 * Property 22f: All tokens have positive length.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              AllTokensHavePositiveLength,
              ()) {
    auto source = *rc::gen::map(
        rc::gen::tuple(genKeyword(), genIdentifier(), genIntLiteral()),
        [](const std::tuple<std::string, std::string, std::string>& t) {
            auto [kw, id, num] = t;
            return kw + " " + id + " = " + num;
        }
    );

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    for (const auto& tok : result.tokens) {
        RC_ASSERT(tok.length > 0);
    }
}

/**
 * Property 22g: classify_construct maps keywords correctly.
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              ClassifyConstructMapsKeywordsCorrectly,
              ()) {
    auto kw = *genKeyword();
    RC_ASSERT(classify_construct(kw) == SemanticTokenType::Keyword);
}

/**
 * Property 22h: classify_construct maps type names (uppercase start).
 */
RC_GTEST_PROP(LanguageConstructClassificationProperty,
              ClassifyConstructMapsTypeNames,
              ()) {
    auto tn = *genTypeName();
    RC_PRE(!is_meld_keyword(tn));
    RC_ASSERT(classify_construct(tn) == SemanticTokenType::Type);
}

}  // namespace
}  // namespace meld::daemon
