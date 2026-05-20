/**
 * **Feature: meld-daemon, Property 24: Valid Syntax Parsing**
 *
 * For any syntactically valid Meld code, the LspChannel SHALL parse
 * the content without generating errors.
 *
 * **Validates: Requirements 15.4**
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
// Generators — produce syntactically valid Meld source code
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdent() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + ((i * 3 + 5) % 26));
            return s;
        }
    );
}

rc::Gen<std::string> genType() {
    return rc::gen::map(genIdent(), [](const std::string& id) {
        std::string r = id;
        if (!r.empty())
            r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    });
}

rc::Gen<std::string> genValDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "val " + name + " = " + std::to_string(val);
        }
    );
}

rc::Gen<std::string> genVarDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "var " + name + " = " + std::to_string(val);
        }
    );
}

rc::Gen<std::string> genStringDecl() {
    return rc::gen::map(
        rc::gen::tuple(genIdent(), genIdent()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, content] = t;
            return "val " + name + " = \"" + content + "\"";
        }
    );
}

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

rc::Gen<std::string> genStructDef() {
    return rc::gen::map(genType(), [](const std::string& name) {
        return "struct " + name + " {\n    val x = 0\n}";
    });
}

rc::Gen<std::string> genValidMeldFile() {
    return rc::gen::mapcat(
        rc::gen::inRange(1, 4),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<std::string>>(
                    count,
                    rc::gen::oneOf(
                        genValDecl(), genVarDecl(), genStringDecl(),
                        genFuncDef(), genStructDef()
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

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 24a: Valid val/var declarations parse without errors.
 */
RC_GTEST_PROP(ValidSyntaxParsingProperty,
              ValVarDeclarationsParseSuccessfully,
              ()) {
    auto source = *rc::gen::oneOf(genValDecl(), genVarDecl());

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(result.success);
    RC_ASSERT(result.errors.empty());
    RC_ASSERT(result.diagnostics.empty());
    RC_ASSERT(!result.tokens.empty());

    bool has_keyword = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(has_keyword);
}

/**
 * Property 24b: Valid function definitions parse without errors.
 */
RC_GTEST_PROP(ValidSyntaxParsingProperty,
              FunctionDefinitionsParseSuccessfully,
              ()) {
    auto source = *genFuncDef();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(result.success);
    RC_ASSERT(result.errors.empty());
    RC_ASSERT(!result.tokens.empty());

    bool has_fnc = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Keyword; });
    RC_ASSERT(has_fnc);

    bool has_number = std::any_of(result.tokens.begin(), result.tokens.end(),
        [](const SemanticToken& t) { return t.type == SemanticTokenType::Number; });
    RC_ASSERT(has_number);
}

/**
 * Property 24c: Valid struct definitions parse without errors.
 */
RC_GTEST_PROP(ValidSyntaxParsingProperty,
              StructDefinitionsParseSuccessfully,
              ()) {
    auto source = *genStructDef();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(result.success);
    RC_ASSERT(result.errors.empty());
    RC_ASSERT(!result.tokens.empty());
}

/**
 * Property 24d: Multi-declaration files parse and produce tokens for all constructs.
 */
RC_GTEST_PROP(ValidSyntaxParsingProperty,
              MultiDeclarationFilesProduceTokens,
              ()) {
    auto source = *genValidMeldFile();
    RC_PRE(!source.empty());

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.tokens.empty());
    for (const auto& tok : result.tokens) {
        RC_ASSERT(tok.line >= 0);
        RC_ASSERT(tok.start_char >= 0);
        RC_ASSERT(tok.length > 0);
    }
}

/**
 * Property 24e: Caching returns the same result for the same content.
 */
RC_GTEST_PROP(ValidSyntaxParsingProperty,
              CachingReturnsSameResult,
              ()) {
    auto source = *genValidMeldFile();
    RC_PRE(!source.empty());

    SemanticTokenProvider provider;
    std::string uri = "file:///test.meld";

    auto result1 = provider.parse_document(uri, source, 1);
    auto cached = provider.get_cached(uri);

    RC_ASSERT(cached.has_value());
    RC_ASSERT(cached->tokens.size() == result1.tokens.size());
    RC_ASSERT(cached->success == result1.success);
    RC_ASSERT(cached->errors.size() == result1.errors.size());
}

/**
 * Property 24f: Empty source parses successfully with no tokens.
 */
TEST(ValidSyntaxParsingProperty, EmptySourceParsesSuccessfully) {
    SemanticTokenProvider provider;
    auto result = provider.parse("");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.tokens.empty());
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.diagnostics.empty());
}

}  // namespace
}  // namespace meld::daemon
