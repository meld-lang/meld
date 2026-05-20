#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include <iostream>

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

// Test parsing function with effects clause
TEST(EffectsClauseTest, ParseSimpleEffects) {
    std::string source = R"(
        fnc readFile(path: string) -> string
            effects { EffectIO }
        {
            rtn "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    // Check that it's a function definition
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check function name
    EXPECT_EQ(func_def->get().name.name, "readFile");
    
    // Check effects clause
    EXPECT_TRUE(func_def->get().has_effects);
    ASSERT_EQ(func_def->get().effects_clause.size(), 1);
    EXPECT_EQ(func_def->get().effects_clause[0].name, "EffectIO");
}

// Test parsing function with multiple effects
TEST(EffectsClauseTest, ParseMultipleEffects) {
    std::string source = R"(
        fnc fetchAndSave(url: string, path: string) -> string
            effects { EffectNetwork, EffectIO, EffectState }
        {
            rtn "done"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check effects clause
    EXPECT_TRUE(func_def->get().has_effects);
    ASSERT_EQ(func_def->get().effects_clause.size(), 3);
    EXPECT_EQ(func_def->get().effects_clause[0].name, "EffectNetwork");
    EXPECT_EQ(func_def->get().effects_clause[1].name, "EffectIO");
    EXPECT_EQ(func_def->get().effects_clause[2].name, "EffectState");
}

// Test parsing function without effects clause (defaults to pure)
TEST(EffectsClauseTest, ParseWithoutEffects) {
    std::string source = R"(
        fnc pureFunction(x: int) -> int {
            rtn x * 2
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check no effects clause (defaults to pure)
    EXPECT_FALSE(func_def->get().has_effects);
    EXPECT_EQ(func_def->get().effects_clause.size(), 0);
}

// Test parsing function with effects and named return values
TEST(EffectsClauseTest, ParseEffectsWithNamedReturns) {
    std::string source = R"(
        fnc readAndParse(path: string) -> (content: string, lines: int)
            effects { EffectIO }
        {
            content = "data"
            lines = 10
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check named returns
    EXPECT_TRUE(func_def->get().has_named_returns);
    ASSERT_EQ(func_def->get().named_returns.size(), 2);
    
    // Check effects clause
    EXPECT_TRUE(func_def->get().has_effects);
    ASSERT_EQ(func_def->get().effects_clause.size(), 1);
    EXPECT_EQ(func_def->get().effects_clause[0].name, "EffectIO");
}

// Test parsing function with empty effects clause (explicit pure)
TEST(EffectsClauseTest, ParseEmptyEffects) {
    std::string source = R"(
        fnc pureFunction(x: int) -> int
            effects { }
        {
            rtn x * 2
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    // This should parse successfully but with empty effects list
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->get().has_effects);
    EXPECT_EQ(func_def->get().effects_clause.size(), 0);
}

// Test parsing function with effects and default parameters
TEST(EffectsClauseTest, ParseEffectsWithDefaultParams) {
    std::string source = R"(
        fnc readFileWithDefault(path: string = "default.txt") -> string
            effects { EffectIO }
        {
            rtn "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check parameters
    ASSERT_EQ(func_def->get().parameters.size(), 1);
    EXPECT_TRUE(func_def->get().parameters[0].has_default);
    
    // Check effects clause
    EXPECT_TRUE(func_def->get().has_effects);
    ASSERT_EQ(func_def->get().effects_clause.size(), 1);
    EXPECT_EQ(func_def->get().effects_clause[0].name, "EffectIO");
}

// Test parsing function with custom effect types
TEST(EffectsClauseTest, ParseCustomEffects) {
    std::string source = R"(
        fnc queryDatabase(sql: string) -> ResultSet
            effects { EffectDatabase, EffectLogging }
        {
            rtn executeQuery(sql)
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<x3::forward_ast<ast::function_definition>>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check effects clause
    EXPECT_TRUE(func_def->get().has_effects);
    ASSERT_EQ(func_def->get().effects_clause.size(), 2);
    EXPECT_EQ(func_def->get().effects_clause[0].name, "EffectDatabase");
    EXPECT_EQ(func_def->get().effects_clause[1].name, "EffectLogging");
}

// Test error: missing closing brace
TEST(EffectsClauseTest, ErrorMissingClosingBrace) {
    std::string source = R"(
        fnc readFile(path: string) -> string
            effects { EffectIO
        {
            rtn "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(parser.error_message().empty());
}

// Test error: missing opening brace
TEST(EffectsClauseTest, ErrorMissingOpeningBrace) {
    std::string source = R"(
        fnc readFile(path: string) -> string
            effects EffectIO }
        {
            rtn "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(parser.error_message().empty());
}

// Test parsing lambda with inferred effects (future feature)
TEST(EffectsClauseTest, ParseLambdaEffectInference) {
    std::string source = R"(
        val lambda = { path: string =>
            File.read(path)  // Should infer EffectIO
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    // This test just ensures the lambda parses correctly
    // Effect inference will be handled by the type checker
    auto* val_decl = boost::get<x3::forward_ast<ast::val_declaration>>(&expressions[0]);
    ASSERT_NE(val_decl, nullptr);
    EXPECT_EQ(val_decl->get().name.name, "lambda");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
