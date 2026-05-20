#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include <iostream>

using namespace meld::parser;

// Test parsing function with @imposes annotation
TEST(EffectImposesTest, ParseSimpleImposes) {
    std::string source = R"(
        @imposes(FileSystem)
        fn readFile(path: string) -> string {
            return "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    // Check that it's a function definition
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check function name
    EXPECT_EQ(func_def->name.name, "readFile");
    
    // Check @imposes annotation
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    // Check annotation parameters
    auto imposes_annotation = func_def->get_annotation("imposes");
    ASSERT_NE(imposes_annotation, nullptr);
    ASSERT_EQ(imposes_annotation->parameters.size(), 1);
    EXPECT_EQ(imposes_annotation->parameters[0], "FileSystem");
}

// Test parsing function with multiple effects in @imposes
TEST(EffectImposesTest, ParseMultipleEffects) {
    std::string source = R"(
        @imposes(Network, FileSystem, Console)
        fn fetchAndSave(url: string, path: string) -> string {
            return "done"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check @imposes annotation
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    auto imposes_annotation = func_def->get_annotation("imposes");
    ASSERT_NE(imposes_annotation, nullptr);
    ASSERT_EQ(imposes_annotation->parameters.size(), 3);
    EXPECT_EQ(imposes_annotation->parameters[0], "Network");
    EXPECT_EQ(imposes_annotation->parameters[1], "FileSystem");
    EXPECT_EQ(imposes_annotation->parameters[2], "Console");
}

// Test parsing function without @imposes annotation
TEST(EffectImposesTest, ParseWithoutImposes) {
    std::string source = R"(
        fn pureFunction(x: int) -> int {
            return x * 2
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check no @imposes annotation
    EXPECT_FALSE(func_def->has_annotation("imposes"));
}

// Test parsing function with @imposes and named return values
TEST(EffectImposesTest, ParseImposesWithNamedReturns) {
    std::string source = R"(
        @imposes(FileSystem)
        fn readAndParse(path: string) -> (content: string, lines: int) {
            content = "data"
            lines = 10
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check named returns
    EXPECT_TRUE(func_def->has_named_returns);
    ASSERT_EQ(func_def->named_returns.size(), 2);
    
    // Check @imposes annotation
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    auto imposes_annotation = func_def->get_annotation("imposes");
    ASSERT_NE(imposes_annotation, nullptr);
    ASSERT_EQ(imposes_annotation->parameters.size(), 1);
    EXPECT_EQ(imposes_annotation->parameters[0], "FileSystem");
}

// Test error: @imposes annotation with invalid syntax
TEST(EffectImposesTest, ErrorInvalidImposeSyntax) {
    std::string source = R"(
        @imposes FileSystem
        fn readFile(path: string) -> string {
            return "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    // Should fail because @imposes requires parentheses
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(parser.error_message().empty());
}

// Test @imposes annotation with empty parameters
TEST(EffectImposesTest, EmptyImposesAnnotation) {
    std::string source = R"(
        @imposes()
        fn readFile(path: string) -> string {
            return "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    // This should parse successfully but with empty effects list
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    auto imposes_annotation = func_def->get_annotation("imposes");
    ASSERT_NE(imposes_annotation, nullptr);
    EXPECT_EQ(imposes_annotation->parameters.size(), 0);
}

// Test parsing function with @imposes and default parameters
TEST(EffectImposesTest, ParseImposesWithDefaultParams) {
    std::string source = R"(
        @imposes(FileSystem)
        fn readFileWithDefault(path: string = "default.txt") -> string {
            return "content"
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check parameters
    ASSERT_EQ(func_def->parameters.size(), 1);
    EXPECT_TRUE(func_def->parameters[0].has_default);
    
    // Check @imposes annotation
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    auto imposes_annotation = func_def->get_annotation("imposes");
    ASSERT_NE(imposes_annotation, nullptr);
    ASSERT_EQ(imposes_annotation->parameters.size(), 1);
    EXPECT_EQ(imposes_annotation->parameters[0], "FileSystem");
}

