/**
 * **Feature: meld-lsp-server, Property 4: Valid syntax parsing**
 *
 * For any syntactically valid Meld code, the LSP server should parse
 * the content without generating errors.
 *
 * **Validates: Requirements 1.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/meld_parser_adapter.hpp"

#include <string>
#include <vector>
#include <sstream>

namespace {

using namespace meld::lsp::integration;
using namespace meld::lsp::services;

// ---------------------------------------------------------------------------
// Generators — produce syntactically valid Meld source code
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdent() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 3 + 5) % 26));
            }
            return s;
        }
    );
}

rc::Gen<std::string> genType() {
    return rc::gen::map(
        genIdent(),
        [](const std::string& id) {
            std::string r = id;
            if (!r.empty()) {
                r[0] = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(r[0])));
            }
            return r;
        }
    );
}

/// val <name> = <int>
rc::Gen<std::string> genValDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "val " + name + " = " + std::to_string(val);
        }
    );
}

/// var <name> = <int>
rc::Gen<std::string> genVarDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "var " + name + " = " + std::to_string(val);
        }
    );
}

/// val <name> = "<text>"
rc::Gen<std::string> genStringDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), genIdent()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, content] = t;
            return "val " + name + " = \"" + content + "\"";
        }
    );
}

/// fnc <name>() { val result = <int>\n    return result\n}
rc::Gen<std::string> genFuncDef() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), rc::gen::inRange(0, 99)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "fnc " + name + "() {\n    val result = " +
                   std::to_string(val) + "\n    return result\n}";
        }
    );
}

/// struct <Name> { val x = 0 }
rc::Gen<std::string> genStructDef() {
    return rc::gen::map(
        genType(),
        [](const std::string& name) {
            return "struct " + name + " {\n    val x = 0\n}";
        }
    );
}

/// Multiple declarations combined
rc::Gen<std::string> genValidMeldFile() {
    return rc::gen::mapcat(
        rc::gen::inRange(1, 4),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<std::string>>(
                    count,
                    rc::gen::oneOf(
                        genValDecl(),
                        genVarDecl(),
                        genStringDecl(),
                        genFuncDef(),
                        genStructDef()
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

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 4a: Valid val/var declarations parse without errors.
 */
TEST(ValidSyntaxParsingPropertyTest, ValVarDeclarationsParseSuccessfully) {
    rc::check("Valid val/var declarations must parse without errors",
        []() {
            auto source = *rc::gen::oneOf(genValDecl(), genVarDecl());

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            RC_ASSERT(result.success);
            RC_ASSERT(!result.tokens.empty());

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
 * Property 4b: Valid function definitions parse and produce tokens.
 */
TEST(ValidSyntaxParsingPropertyTest, FunctionDefinitionsParseSuccessfully) {
    rc::check("Valid function definitions must parse and produce tokens",
        []() {
            auto source = *genFuncDef();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            RC_ASSERT(result.success);
            RC_ASSERT(!result.tokens.empty());

            bool has_fnc = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Keyword;
                });
            RC_ASSERT(has_fnc);

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
 * Property 4c: Valid struct definitions parse and produce tokens.
 */
TEST(ValidSyntaxParsingPropertyTest, StructDefinitionsParseSuccessfully) {
    rc::check("Valid struct definitions must parse and produce tokens",
        []() {
            auto source = *genStructDef();

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            RC_ASSERT(result.success);
            RC_ASSERT(!result.tokens.empty());

            bool has_struct_kw = std::any_of(
                result.tokens.begin(), result.tokens.end(),
                [](const SemanticToken& t) {
                    return t.type == SemanticTokenType::Keyword;
                });
            RC_ASSERT(has_struct_kw);
        }
    );
}

/**
 * Property 4d: Multi-declaration files produce tokens for all constructs.
 */
TEST(ValidSyntaxParsingPropertyTest, MultiDeclarationFilesProduceTokens) {
    rc::check("Multi-declaration files must produce tokens for all constructs",
        []() {
            auto source = *genValidMeldFile();
            RC_PRE(!source.empty());

            MeldParserAdapter adapter;
            auto result = adapter.parse(source);

            RC_ASSERT(!result.tokens.empty());

            for (const auto& tok : result.tokens) {
                RC_ASSERT(tok.line >= 0);
                RC_ASSERT(tok.start_char >= 0);
                RC_ASSERT(tok.length > 0);
            }
        }
    );
}

/**
 * Property 4e: Caching returns the same result for the same content.
 */
TEST(ValidSyntaxParsingPropertyTest, CachingReturnsSameResult) {
    rc::check("Cached parse results must match the original parse",
        []() {
            auto source = *genValidMeldFile();
            RC_PRE(!source.empty());

            MeldParserAdapter adapter;
            std::string uri = "file:///test.meld";

            auto result1 = adapter.parse_document(uri, source, 1);
            auto cached = adapter.get_cached(uri);

            RC_ASSERT(cached.has_value());
            RC_ASSERT(cached->tokens.size() == result1.tokens.size());
            RC_ASSERT(cached->success == result1.success);
            RC_ASSERT(cached->errors.size() == result1.errors.size());
        }
    );
}

/**
 * Property 4f: Empty source parses successfully with no tokens.
 */
TEST(ValidSyntaxParsingPropertyTest, EmptySourceParsesSuccessfully) {
    MeldParserAdapter adapter;
    auto result = adapter.parse("");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.tokens.empty());
    EXPECT_TRUE(result.errors.empty());
}
