/**
 * **Feature: meld-lsp-server, Property 2: Language construct classification**
 *
 * For any Meld source code, all language constructs should be correctly
 * identified and classified by type (symbols, literals, keywords, operators,
 * comments).
 *
 * **Validates: Requirements 1.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/meld_parser_adapter.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace {

using namespace meld::lsp::integration;
using namespace meld::lsp::services;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a Meld keyword
rc::Gen<std::string> genKeyword() {
    static const std::vector<std::string> kws = {
        "fnc", "let", "val", "var", "if", "else", "match", "return", "rtn",
        "import", "struct", "enum", "trait", "impl",
        "async", "await", "for", "while", "true", "false",
        "effect", "handle", "perform", "resume",
        "type", "extend", "operator", "namespace",
        "query", "from", "where", "select",
        "flow", "test", "assert"
    };
    return rc::gen::elementOf(kws);
}

/// Generate a simple lowercase identifier
rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 10),
        [](int len) {
            std::string s;
            s.reserve(len);
            s += static_cast<char>('a' + (len % 26));
            for (int i = 1; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 7) % 26));
            }
            return s;
        }
    );
}

/// Generate a type-like identifier (uppercase start)
rc::Gen<std::string> genTypeName() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& id) {
            std::string result = id;
            if (!result.empty()) {
                result[0] = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(result[0])));
            }
            return result;
        }
    );
}

/// Generate a string literal
rc::Gen<std::string> genStringLiteral() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& content) {
            return "\"" + content + "\"";
        }
    );
}

/// Generate an integer literal
rc::Gen<std::string> genIntLiteral() {
    return rc::gen::map(
        rc::gen::inRange(0, 9999),
        [](int v) { return std::to_string(v); }
    );
}

/// Generate an effect annotation
rc::Gen<std::string> genEffectAnnotation() {
    static const std::vector<std::string> annotations = {
        "@effect", "@uses", "@const", "@mut", "@ref", "@move"
    };
    return rc::gen::elementOf(annotations);
}

/// Generate a Meld operator
rc::Gen<std::string> genOperator() {
    static const std::vector<std::string> ops = {
        "+", "-", "*", "=", "<", ">", "!", "&", "|"
    };
    return rc::gen::elementOf(ops);
}

/// Generate a Meld source snippet containing a keyword
rc::Gen<std::string> genSourceWithKeyword() {
    return rc::gen::map(
        rc::gen::tuple(genKeyword(), genIdentifier()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [kw, id] = t;
            return kw + " " + id;
        }
    );
}

/// Generate a Meld function definition
rc::Gen<std::string> genFunctionSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            return "fnc " + name + "() {\n    val x = 42\n    return x\n}";
        }
    );
}

/// Generate a Meld struct definition
rc::Gen<std::string> genStructSource() {
    return rc::gen::map(
        genTypeName(),
        [](const std::string& name) {
            return "struct " + name + " {\n    val value = 0\n}";
        }
    );
}

/// Generate a Meld source with effect annotations
rc::Gen<std::string> genEffectSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            std::string type_name(1, static_cast<char>(
                std::toupper(static_cast<unsigned char>(name[0]))));
            type_name += name.substr(1);
            return "@effect trait " + type_name +
                   " {\n    fnc read() {\n    }\n}";
        }
    );
}

// Set of all Meld keywords for checking
const std::unordered_set<std::string> ALL_KEYWORDS = {
    "fnc", "let", "val", "var", "if", "else", "match", "return", "rtn",
    "import", "imp", "struct", "enum", "trait", "impl",
    "async", "await", "for", "while", "true", "false",
    "effect", "handle", "perform", "resume",
    "type", "newtype", "extend", "operator",
    "namespace", "from", "where", "select", "join", "group",
    "query", "flow", "on", "goto", "test", "assert",
    "infix", "prefix", "postfix"
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 2a: Keywords in Meld source are classified as Keyword tokens.
 */
TEST(ConstructClassificationPropertyTest, KeywordsAreClassifiedAsKeyword) {
    rc::check("Meld keywords must be classified as Keyword tokens",
        []() {
            auto source = *genSourceWithKeyword();
            RC_PRE(!source.empty());

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

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
 * Property 2b: String literals are classified as String tokens.
 */
TEST(ConstructClassificationPropertyTest, StringLiteralsAreClassifiedAsString) {
    rc::check("String literals must be classified as String tokens",
        []() {
            auto str_lit = *genStringLiteral();
            std::string source = "val x = " + str_lit;

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            bool has_string = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::String;
                });
            RC_ASSERT(has_string);
        }
    );
}

/**
 * Property 2c: Number literals are classified as Number tokens.
 */
TEST(ConstructClassificationPropertyTest, NumberLiteralsAreClassifiedAsNumber) {
    rc::check("Number literals must be classified as Number tokens",
        []() {
            auto num = *genIntLiteral();
            std::string source = "val x = " + num;

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

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
 * Property 2d: Effect annotations are classified as Decorator tokens.
 */
TEST(ConstructClassificationPropertyTest, EffectAnnotationsAreClassifiedAsDecorator) {
    rc::check("Effect annotations must be classified as Decorator tokens",
        []() {
            auto annotation = *genEffectAnnotation();
            std::string source = annotation + " trait MyEffect {}";

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            bool has_decorator = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Decorator;
                });
            RC_ASSERT(has_decorator);
        }
    );
}

/**
 * Property 2e: Operators are classified as Operator tokens.
 */
TEST(ConstructClassificationPropertyTest, OperatorsAreClassifiedAsOperator) {
    rc::check("Operators must be classified as Operator tokens",
        []() {
            auto op = *genOperator();
            std::string source = "val x = 1 " + op + " 2";

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            bool has_operator = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Operator;
                });
            RC_ASSERT(has_operator);
        }
    );
}

/**
 * Property 2f: All tokens have valid, positive lengths.
 */
TEST(ConstructClassificationPropertyTest, AllTokensHavePositiveLength) {
    rc::check("All semantic tokens must have positive length",
        []() {
            auto source = *rc::gen::oneOf(
                genFunctionSource(),
                genStructSource(),
                genEffectSource()
            );

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            for (const auto& token : result.tokens) {
                RC_ASSERT(token.length > 0);
                RC_ASSERT(token.line >= 0);
                RC_ASSERT(token.start_char >= 0);
            }
        }
    );
}

/**
 * Property 2g: classify_construct correctly maps keywords.
 */
TEST(ConstructClassificationPropertyTest, ClassifyConstructMapsKeywordsCorrectly) {
    rc::check("classify_construct maps keywords to Keyword type",
        []() {
            auto kw = *genKeyword();
            auto result = classify_construct(kw);
            RC_ASSERT(result == SemanticTokenType::Keyword);
        }
    );
}

/**
 * Property 2h: classify_construct maps uppercase identifiers to Type.
 */
TEST(ConstructClassificationPropertyTest, ClassifyConstructMapsTypeNames) {
    rc::check("classify_construct maps uppercase identifiers to Type",
        []() {
            auto name = *genTypeName();
            RC_PRE(!ALL_KEYWORDS.count(name));

            auto result = classify_construct(name);
            RC_ASSERT(result == SemanticTokenType::Type);
        }
    );
}
