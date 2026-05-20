#include <gtest/gtest.h>
#include "meld/daemon/language_service.hpp"

namespace meld::lsp::services {

TEST(LanguageServiceTest, EmptyContentReturnsNoTokens) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "");
    EXPECT_TRUE(tokens.empty());
}

TEST(LanguageServiceTest, KeywordTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "fnc");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, SemanticTokenType::Keyword);
    EXPECT_EQ(tokens[0].length, 3);
}

TEST(LanguageServiceTest, CommentTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "// a comment");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, SemanticTokenType::Comment);
}

TEST(LanguageServiceTest, StringTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", R"("hello")");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, SemanticTokenType::String);
}

TEST(LanguageServiceTest, NumberTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "42");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, SemanticTokenType::Number);
}

TEST(LanguageServiceTest, DecoratorTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "@uses");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, SemanticTokenType::Decorator);
}

TEST(LanguageServiceTest, MultilineTokenization) {
    LanguageService service;
    auto tokens = service.get_semantic_tokens("file:///test.meld", "fnc\nlet");
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].line, 0);
    EXPECT_EQ(tokens[1].line, 1);
}

TEST(LanguageServiceTest, EncodeSemanticTokensDeltaEncoding) {
    LanguageService service;
    std::vector<SemanticToken> tokens = {
        {0, 0, 3, SemanticTokenType::Keyword, 0},
        {0, 4, 5, SemanticTokenType::Function, 0},
        {1, 2, 3, SemanticTokenType::Variable, 0},
    };
    auto encoded = service.encode_semantic_tokens(tokens);
    // 3 tokens * 5 ints each = 15
    ASSERT_EQ(encoded.size(), 15u);
    // First token: deltaLine=0, deltaChar=0, length=3, type=Keyword(0), mod=0
    EXPECT_EQ(encoded[0], 0);
    EXPECT_EQ(encoded[1], 0);
    EXPECT_EQ(encoded[2], 3);
    // Second token: deltaLine=0, deltaChar=4, length=5
    EXPECT_EQ(encoded[5], 0);
    EXPECT_EQ(encoded[6], 4);
    EXPECT_EQ(encoded[7], 5);
    // Third token: deltaLine=1, deltaChar=2 (new line, so absolute)
    EXPECT_EQ(encoded[10], 1);
    EXPECT_EQ(encoded[11], 2);
}

TEST(LanguageServiceTest, SemanticTokensLegendHasTypes) {
    LanguageService service;
    auto legend = service.get_semantic_tokens_legend();
    EXPECT_TRUE(legend.contains("tokenTypes"));
    EXPECT_TRUE(legend.contains("tokenModifiers"));
    EXPECT_FALSE(legend["tokenTypes"].empty());
}

TEST(LanguageServiceTest, TokenTypeNamesNotEmpty) {
    auto names = LanguageService::token_type_names();
    EXPECT_FALSE(names.empty());
    // Should contain at least "keyword" and "function"
    EXPECT_NE(std::find(names.begin(), names.end(), "keyword"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "function"), names.end());
}

} // namespace meld::lsp::services
