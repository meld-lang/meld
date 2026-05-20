#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

// ============================================================================
// Lexer Tests: imp keyword recognition and banned keyword errors
// ============================================================================

TEST(ImpLexerTest, ImpIsRecognizedAsKeyword) {
    Lexer lexer("imp");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // imp + EOF
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "imp");
}

TEST(ImpLexerTest, ImportIsBannedKeyword) {
    Lexer lexer("import");
    auto tokens = lexer.tokenize();
    
    ASSERT_FALSE(lexer.errors().empty());
    EXPECT_TRUE(lexer.errors()[0].find("'import' is not a valid Meld keyword") != std::string::npos);
    EXPECT_TRUE(lexer.errors()[0].find("imp") != std::string::npos);
}

TEST(ImpLexerTest, FromIsBannedKeyword) {
    Lexer lexer("from");
    auto tokens = lexer.tokenize();
    
    ASSERT_FALSE(lexer.errors().empty());
    EXPECT_TRUE(lexer.errors()[0].find("'from' keyword is not part of Meld's grammar") != std::string::npos);
}

TEST(ImpLexerTest, AsIsBannedKeyword) {
    Lexer lexer("as");
    auto tokens = lexer.tokenize();
    
    ASSERT_FALSE(lexer.errors().empty());
    EXPECT_TRUE(lexer.errors()[0].find("'as' keyword is not part of Meld's grammar") != std::string::npos);
}

TEST(ImpLexerTest, ImpTokenHasCorrectPosition) {
    Lexer lexer("imp std.math");
    auto tokens = lexer.tokenize();
    
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "imp");
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
}

// ============================================================================
// Parser Tests: Basic import form (imp std.math)
// ============================================================================

TEST(ImpParserTest, ParseBasicImport) {
    std::string code = "imp std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_BASIC);
    EXPECT_TRUE(decl.is_imp);
    ASSERT_EQ(decl.namespace_path.size(), 2);
    EXPECT_EQ(decl.namespace_path[0], "std");
    EXPECT_EQ(decl.namespace_path[1], "math");
}

TEST(ImpParserTest, ParseBasicImportDeepPath) {
    std::string code = "imp app.services.auth";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_BASIC);
    ASSERT_EQ(decl.namespace_path.size(), 3);
    EXPECT_EQ(decl.namespace_path[0], "app");
    EXPECT_EQ(decl.namespace_path[1], "services");
    EXPECT_EQ(decl.namespace_path[2], "auth");
}

TEST(ImpParserTest, ParseBasicImportSingleSegment) {
    std::string code = "imp utils";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_BASIC);
    ASSERT_EQ(decl.namespace_path.size(), 1);
    EXPECT_EQ(decl.namespace_path[0], "utils");
}

// ============================================================================
// Parser Tests: Aliased import form (imp m = std.math)
// ============================================================================

TEST(ImpParserTest, ParseAliasedImport) {
    std::string code = "imp m = std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_ALIASED);
    EXPECT_TRUE(decl.is_imp);
    EXPECT_TRUE(decl.has_alias);
    EXPECT_EQ(decl.alias, "m");
    ASSERT_EQ(decl.namespace_path.size(), 2);
    EXPECT_EQ(decl.namespace_path[0], "std");
    EXPECT_EQ(decl.namespace_path[1], "math");
}

// ============================================================================
// Parser Tests: Destructured import form (imp { sin, cos } = std.math)
// ============================================================================

TEST(ImpParserTest, ParseDestructuredImport) {
    std::string code = "imp { sin, cos } = std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_DESTRUCTURED);
    EXPECT_TRUE(decl.is_imp);
    ASSERT_EQ(decl.namespace_path.size(), 2);
    EXPECT_EQ(decl.namespace_path[0], "std");
    EXPECT_EQ(decl.namespace_path[1], "math");
    
    ASSERT_EQ(decl.symbols.size(), 2);
    EXPECT_EQ(decl.symbols[0].original_name, "sin");
    EXPECT_EQ(decl.symbols[0].local_name, "sin");
    EXPECT_FALSE(decl.symbols[0].has_alias);
    EXPECT_EQ(decl.symbols[1].original_name, "cos");
    EXPECT_EQ(decl.symbols[1].local_name, "cos");
    EXPECT_FALSE(decl.symbols[1].has_alias);
}

// ============================================================================
// Parser Tests: Destructured with aliasing
// ============================================================================

TEST(ImpParserTest, ParseDestructuredImportWithAliasing) {
    std::string code = "imp { sin -> s, cos -> c } = std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    EXPECT_EQ(decl.import_type, ast::ImportType::IMP_DESTRUCTURED);
    
    ASSERT_EQ(decl.symbols.size(), 2);
    EXPECT_EQ(decl.symbols[0].original_name, "sin");
    EXPECT_EQ(decl.symbols[0].local_name, "s");
    EXPECT_TRUE(decl.symbols[0].has_alias);
    EXPECT_EQ(decl.symbols[1].original_name, "cos");
    EXPECT_EQ(decl.symbols[1].local_name, "c");
    EXPECT_TRUE(decl.symbols[1].has_alias);
}

TEST(ImpParserTest, ParseDestructuredMixedAliasAndNonAlias) {
    std::string code = "imp { sin -> s, cos, PI } = std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& decl = imp->get();
    ASSERT_EQ(decl.symbols.size(), 3);
    
    EXPECT_EQ(decl.symbols[0].original_name, "sin");
    EXPECT_EQ(decl.symbols[0].local_name, "s");
    EXPECT_TRUE(decl.symbols[0].has_alias);
    
    EXPECT_EQ(decl.symbols[1].original_name, "cos");
    EXPECT_EQ(decl.symbols[1].local_name, "cos");
    EXPECT_FALSE(decl.symbols[1].has_alias);
    
    EXPECT_EQ(decl.symbols[2].original_name, "PI");
    EXPECT_EQ(decl.symbols[2].local_name, "PI");
    EXPECT_FALSE(decl.symbols[2].has_alias);
}

// ============================================================================
// Parser Tests: Multiple imp statements
// ============================================================================

TEST(ImpParserTest, ParseMultipleImports) {
    std::string code = R"(
        imp std.math
        imp std.io
        val x = 42
    )";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 3);
    
    auto* imp1 = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp1, nullptr);
    EXPECT_EQ(imp1->get().namespace_path.back(), "math");
    
    auto* imp2 = boost::get<x3::forward_ast<ast::import_declaration>>(&result[1]);
    ASSERT_NE(imp2, nullptr);
    EXPECT_EQ(imp2->get().namespace_path.back(), "io");
    
    auto* val = boost::get<x3::forward_ast<ast::val_declaration>>(&result[2]);
    ASSERT_NE(val, nullptr);
}

// ============================================================================
// Parser Tests: Ordering enforcement (imp must come before declarations)
// ============================================================================

TEST(ImpParserTest, ImpAfterDeclarationProducesError) {
    std::string code = R"(
        val x = 42
        imp std.math
    )";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_FALSE(parser.parse_file(code, result));
    EXPECT_TRUE(parser.error_message().find("must appear at the beginning") != std::string::npos);
}

TEST(ImpParserTest, ImpAfterFunctionDefinitionProducesError) {
    std::string code = R"(
        fnc foo() {}
        imp std.math
    )";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_FALSE(parser.parse_file(code, result));
    EXPECT_TRUE(parser.error_message().find("must appear at the beginning") != std::string::npos);
}

// ============================================================================
// Parser Tests: Banned keywords produce errors
// ============================================================================

TEST(ImpParserTest, ImportKeywordProducesParserError) {
    std::string code = "import std.math";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_FALSE(parser.parse_file(code, result));
    EXPECT_TRUE(parser.error_message().find("'import' is not a valid Meld keyword") != std::string::npos);
    EXPECT_TRUE(parser.error_message().find("imp") != std::string::npos);
}
