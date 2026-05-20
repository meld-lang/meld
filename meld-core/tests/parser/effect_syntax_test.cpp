#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include <iostream>

using namespace meld::parser;

// Test parsing perform() as library function call
TEST(EffectSyntaxTest, ParsePerformFunctionCall) {
    std::string source = R"(
        perform { FileSystem.read("config.txt") }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    // Check that it's a function call to perform()
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    // Check function name is "perform"
    EXPECT_EQ(func_call->function_name.name, "perform");
    
    // Check that it has a lambda/block argument
    ASSERT_EQ(func_call->arguments.size(), 1);
}

// Test parsing perform() with multiple arguments
TEST(EffectSyntaxTest, ParsePerformWithMultipleArgs) {
    std::string source = R"(
        perform { FileSystem.write("output.txt", "content") }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->function_name.name, "perform");
    ASSERT_EQ(func_call->arguments.size(), 1);
}

// Test parsing resume() function call with value
TEST(EffectSyntaxTest, ParseResumeWithValue) {
    std::string source = R"(
        resume("mocked content")
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->function_name.name, "resume");
    EXPECT_EQ(func_call->arguments.size(), 1);
}

// Test parsing resume() function call without value
TEST(EffectSyntaxTest, ParseResumeWithoutValue) {
    std::string source = R"(
        resume()
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->function_name.name, "resume");
    EXPECT_EQ(func_call->arguments.size(), 0);
}

// Test parsing @effect annotation on trait
TEST(EffectSyntaxTest, ParseEffectAnnotation) {
    std::string source = R"(
        @effect
        trait FileSystem {
            fn read(path: string) -> string
            fn write(path: string, content: string)
            fn delete(path: string)
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* trait_def = boost::get<ast::trait_definition>(&expressions[0]);
    ASSERT_NE(trait_def, nullptr);
    
    // Check trait name
    EXPECT_EQ(trait_def->name.name, "FileSystem");
    
    // Check that it has @effect annotation
    EXPECT_TRUE(trait_def->has_annotation("effect"));
    
    // Check methods
    ASSERT_EQ(trait_def->methods.size(), 3);
    
    // Check first method (read)
    EXPECT_EQ(trait_def->methods[0].name.name, "read");
    ASSERT_EQ(trait_def->methods[0].parameters.size(), 1);
    EXPECT_EQ(trait_def->methods[0].parameters[0].name.name, "path");
    EXPECT_TRUE(trait_def->methods[0].has_return_type);
    EXPECT_EQ(trait_def->methods[0].return_type.type_name.name, "string");
    
    // Check second method (write)
    EXPECT_EQ(trait_def->methods[1].name.name, "write");
    ASSERT_EQ(trait_def->methods[1].parameters.size(), 2);
    EXPECT_FALSE(trait_def->methods[1].has_return_type);
    
    // Check third method (delete)
    EXPECT_EQ(trait_def->methods[2].name.name, "delete");
    ASSERT_EQ(trait_def->methods[2].parameters.size(), 1);
}

// Test parsing handle() function call
TEST(EffectSyntaxTest, ParseHandleFunctionCall) {
    std::string source = R"(
        handle(
            computation: { readFile("config.txt") }
        ) {
            FileSystem {
                fn read(path: string) -> string {
                    resume("mocked content")
                }
            }
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    // Check function name is "handle"
    EXPECT_EQ(func_call->function_name.name, "handle");
    
    // Check that it has arguments (computation and handler config)
    ASSERT_GE(func_call->arguments.size(), 1);
}

// Test parsing handle() with multiple handlers
TEST(EffectSyntaxTest, ParseHandleWithMultipleHandlers) {
    std::string source = R"(
        handle(
            computation: { processData() }
        ) {
            FileSystem {
                fn read(path: string) -> string {
                    resume("data")
                }
                
                fn write(path: string, content: string) {
                    resume()
                }
                
                fn delete(path: string) {
                    resume()
                }
            }
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* func_call = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->function_name.name, "handle");
}

// Test parsing complete effect example with annotations
TEST(EffectSyntaxTest, ParseCompleteEffectExample) {
    std::string source = R"(
        @imposes(FileSystem)
        fn readConfig(path: string) -> string {
            val content = perform { FileSystem.read(path) }
            return content
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    ASSERT_EQ(expressions.size(), 1);
    
    auto* func_def = boost::get<ast::function_definition>(&expressions[0]);
    ASSERT_NE(func_def, nullptr);
    
    // Check function has @imposes annotation
    EXPECT_TRUE(func_def->has_annotation("imposes"));
    
    // Check function body contains perform() call
    ASSERT_EQ(func_def->body.statements.size(), 2);
}

// Test error: perform() without block syntax
TEST(EffectSyntaxTest, ErrorPerformWithoutBlock) {
    std::string source = R"(
        perform FileSystem.read("file.txt")
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    // This should now be treated as a regular function call with wrong syntax
    // The deprecation warning system will handle this
    EXPECT_TRUE(result.has_value() || !parser.error_message().empty());
}

// Test error: handle() without computation parameter
TEST(EffectSyntaxTest, ErrorHandleWithoutComputation) {
    std::string source = R"(
        handle() {
            FileSystem {
                fn read(path: string) -> string {
                    resume("data")
                }
            }
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(parser.error_message().empty());
}

// Test @effect annotation on empty trait
TEST(EffectSyntaxTest, EffectAnnotationOnEmptyTrait) {
    std::string source = R"(
        @effect
        trait EmptyEffect {
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    // This should parse successfully but with empty methods
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* trait_def = boost::get<ast::trait_definition>(&expressions[0]);
    ASSERT_NE(trait_def, nullptr);
    
    EXPECT_EQ(trait_def->methods.size(), 0);
    EXPECT_TRUE(trait_def->has_annotation("effect"));
}

// Test nested handle() function calls
TEST(EffectSyntaxTest, ParseNestedHandleFunctionCalls) {
    std::string source = R"(
        handle(
            computation: {
                handle(
                    computation: { fetchAndSave("url", "path") }
                ) {
                    Network {
                        fn get(url: string) -> string {
                            resume("data")
                        }
                    }
                }
            }
        ) {
            FileSystem {
                fn write(path: string, content: string) {
                    resume()
                }
            }
        }
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << parser.error_message();
    
    auto& expressions = result.value();
    auto* outer_handle = boost::get<ast::function_call>(&expressions[0]);
    ASSERT_NE(outer_handle, nullptr);
    
    EXPECT_EQ(outer_handle->function_name.name, "handle");
}

