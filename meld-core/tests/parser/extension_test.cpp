#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/kernel/extension_registry.hpp"
#include "meld/kernel/primitives.hpp"
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

// Windows headers (pulled in by gtest) define CONST as a macro
#ifdef CONST
#undef CONST
#endif

using namespace meld::parser;
using namespace meld::parser::ast;
using namespace meld::kernel;

// Test parsing extension block syntax
TEST(ExtensionTest, ParseBasicExtensionBlock) {
    std::string input = R"(
        extend String {
            fn shout() -> String {
                val result = "HELLO"
            }
        }
    )";
    
    Parser parser;
    std::vector<expression> result;
    
    ASSERT_TRUE(parser.parse_file(input, result)) << "Parse error: " << parser.error_message();
    ASSERT_EQ(result.size(), 1);
    
    // Check that we got an extension_block
    auto* ext_block = boost::get<x3::forward_ast<extension_block>>(&result[0]);
    ASSERT_NE(ext_block, nullptr);
    
    const extension_block& block = ext_block->get();
    
    // Check target type
    EXPECT_EQ(block.target_type.type_name.name, "String");
    EXPECT_FALSE(block.target_type.is_nullable);
    
    // Check methods
    ASSERT_EQ(block.methods.size(), 1);
    EXPECT_EQ(block.methods[0].name.name, "shout");
    EXPECT_TRUE(block.methods[0].has_return_type);
    EXPECT_EQ(block.methods[0].return_type.type_name.name, "String");
    EXPECT_EQ(block.methods[0].parameters.size(), 0);
}

// Test parsing extension method with parameters
TEST(ExtensionTest, ParseExtensionMethodWithParameters) {
    std::string input = R"(
        extend String {
            fn repeat(count: Int) -> String {
                val result = "repeated"
            }
        }
    )";
    
    Parser parser;
    std::vector<expression> result;
    
    ASSERT_TRUE(parser.parse_file(input, result)) << "Parse error: " << parser.error_message();
    ASSERT_EQ(result.size(), 1);
    
    auto* ext_block = boost::get<x3::forward_ast<extension_block>>(&result[0]);
    ASSERT_NE(ext_block, nullptr);
    
    const extension_block& block = ext_block->get();
    
    // Check method parameters
    ASSERT_EQ(block.methods.size(), 1);
    EXPECT_EQ(block.methods[0].name.name, "repeat");
    ASSERT_EQ(block.methods[0].parameters.size(), 1);
    EXPECT_EQ(block.methods[0].parameters[0].name.name, "count");
    EXPECT_EQ(block.methods[0].parameters[0].type.type_name.name, "Int");
}

// Test parsing multiple extension methods
TEST(ExtensionTest, ParseMultipleExtensionMethods) {
    std::string input = R"(
        extend String {
            fn shout() -> String {
                val result = "HELLO"
            }
            
            fn whisper() -> String {
                val result = "hello"
            }
            
            fn repeat(count: Int) -> String {
                val result = "repeated"
            }
        }
    )";
    
    Parser parser;
    std::vector<expression> result;
    
    ASSERT_TRUE(parser.parse_file(input, result)) << "Parse error: " << parser.error_message();
    ASSERT_EQ(result.size(), 1);
    
    auto* ext_block = boost::get<x3::forward_ast<extension_block>>(&result[0]);
    ASSERT_NE(ext_block, nullptr);
    
    const extension_block& block = ext_block->get();
    
    // Check that we have 3 methods
    ASSERT_EQ(block.methods.size(), 3);
    EXPECT_EQ(block.methods[0].name.name, "shout");
    EXPECT_EQ(block.methods[1].name.name, "whisper");
    EXPECT_EQ(block.methods[2].name.name, "repeat");
}

// Test parsing extension method with named return values
TEST(ExtensionTest, ParseExtensionMethodWithNamedReturns) {
    std::string input = R"(
        extend String {
            fn split(delimiter: String) -> (parts: List, count: Int) {
                parts = ["a", "b"]
                count = 2
            }
        }
    )";
    
    Parser parser;
    std::vector<expression> result;
    
    ASSERT_TRUE(parser.parse_file(input, result)) << "Parse error: " << parser.error_message();
    ASSERT_EQ(result.size(), 1);
    
    auto* ext_block = boost::get<x3::forward_ast<extension_block>>(&result[0]);
    ASSERT_NE(ext_block, nullptr);
    
    const extension_block& block = ext_block->get();
    
    // Check named return values
    ASSERT_EQ(block.methods.size(), 1);
    EXPECT_TRUE(block.methods[0].has_named_returns);
    ASSERT_EQ(block.methods[0].named_returns.size(), 2);
    EXPECT_EQ(block.methods[0].named_returns[0].name.name, "parts");
    EXPECT_EQ(block.methods[0].named_returns[0].type.type_name.name, "List");
    EXPECT_EQ(block.methods[0].named_returns[1].name.name, "count");
    EXPECT_EQ(block.methods[0].named_returns[1].type.type_name.name, "Int");
}

// Test parsing extension method with parameter decorators
TEST(ExtensionTest, ParseExtensionMethodWithDecorators) {
    std::string input = R"(
        extend Array {
            fn append(@const item: Int) -> Unit {
                val x = item
            }
        }
    )";
    
    Parser parser;
    std::vector<expression> result;
    
    ASSERT_TRUE(parser.parse_file(input, result)) << "Parse error: " << parser.error_message();
    ASSERT_EQ(result.size(), 1);
    
    auto* ext_block = boost::get<x3::forward_ast<extension_block>>(&result[0]);
    ASSERT_NE(ext_block, nullptr);
    
    const extension_block& block = ext_block->get();
    
    // Check parameter decorator
    ASSERT_EQ(block.methods.size(), 1);
    ASSERT_EQ(block.methods[0].parameters.size(), 1);
    EXPECT_EQ(block.methods[0].parameters[0].decorator, ParameterDecorator::CONST);
}

// Test extension registry
TEST(ExtensionTest, RegisterAndLookupExtension) {
    ExtensionRegistry& registry = ExtensionRegistry::instance();
    registry.clear();
    
    // Register an extension method
    auto shout_impl = [](const std::vector<Value>& args) -> Value {
        // Dummy implementation
        return Value();
    };
    
    registry.register_extension("String", "shout", shout_impl, {}, "String");
    
    // Look up the extension
    EXPECT_TRUE(registry.has_extension("String", "shout"));
    
    const ExtensionMethod* method = registry.lookup_extension("String", "shout");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->method_name, "shout");
    EXPECT_EQ(method->target_type_name, "String");
    EXPECT_EQ(method->return_type, "String");
}

// Test extension registry with multiple methods
TEST(ExtensionTest, RegisterMultipleExtensions) {
    ExtensionRegistry& registry = ExtensionRegistry::instance();
    registry.clear();
    
    auto shout_impl = [](const std::vector<Value>& args) -> Value { return Value(); };
    auto whisper_impl = [](const std::vector<Value>& args) -> Value { return Value(); };
    
    registry.register_extension("String", "shout", shout_impl);
    registry.register_extension("String", "whisper", whisper_impl);
    
    // Check both methods exist
    EXPECT_TRUE(registry.has_extension("String", "shout"));
    EXPECT_TRUE(registry.has_extension("String", "whisper"));
    
    // Get all extensions for String
    auto extensions = registry.get_extensions_for_type("String");
    EXPECT_EQ(extensions.size(), 2);
}

// Test extension registry lookup for non-existent method
TEST(ExtensionTest, LookupNonExistentExtension) {
    ExtensionRegistry& registry = ExtensionRegistry::instance();
    registry.clear();
    
    EXPECT_FALSE(registry.has_extension("String", "nonexistent"));
    EXPECT_EQ(registry.lookup_extension("String", "nonexistent"), nullptr);
}

// Test extension method dispatch priority
// (Instance methods should take precedence over extension methods)
TEST(ExtensionTest, ExtensionMethodDispatchPriority) {
    // This test documents the expected behavior:
    // When both an instance method and an extension method exist with the same name,
    // the instance method should be called.
    // This will be implemented in the method dispatch logic.
    
    ExtensionRegistry& registry = ExtensionRegistry::instance();
    registry.clear();
    
    // Register an extension method
    auto length_impl = [](const std::vector<Value>& args) -> Value { return Value(); };
    registry.register_extension("String", "length", length_impl);
    
    // The extension exists
    EXPECT_TRUE(registry.has_extension("String", "length"));
    
    // In actual dispatch, if String has an instance method "length",
    // that should be called instead of the extension method.
    // This behavior will be tested in integration tests.
}
