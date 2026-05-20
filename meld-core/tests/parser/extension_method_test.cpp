#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/kernel/extension_registry.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::parser;
using namespace meld::compiler;
using namespace meld::kernel;
using namespace meld::meta;

class ExtensionMethodTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& registry = TypeRegistry::instance();
        type_checker = std::make_unique<TypeChecker>(registry);
        
        // Clear extension registry for clean tests
        ExtensionRegistry::instance().clear();
    }
    
    void TearDown() override {
        ExtensionRegistry::instance().clear();
    }
    
    std::unique_ptr<TypeChecker> type_checker;
};

TEST_F(ExtensionMethodTest, ParseSimpleExtension) {
    std::string input = R"(
        extend String {
            fnc length() -> int {
                rtn 42
            }
        }
    )";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got an extension block
    auto* ext_block = boost::get<boost::spirit::x3::forward_ast<ast::extension_block>>(&result->get());
    ASSERT_NE(ext_block, nullptr);
    
    const auto& extension = ext_block->get();
    EXPECT_EQ(extension.target_type.type_name.name, "String");
    EXPECT_EQ(extension.methods.size(), 1);
    EXPECT_EQ(extension.methods[0].name.name, "length");
    EXPECT_TRUE(extension.methods[0].has_return_type);
    EXPECT_EQ(extension.methods[0].return_type.type_name.name, "int");
}

TEST_F(ExtensionMethodTest, ParseExtensionWithParameters) {
    std::string input = R"(
        extend List {
            fnc contains(item: T) -> bool {
                rtn false
            }
        }
    )";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got an extension block
    auto* ext_block = boost::get<boost::spirit::x3::forward_ast<ast::extension_block>>(&result->get());
    ASSERT_NE(ext_block, nullptr);
    
    const auto& extension = ext_block->get();
    EXPECT_EQ(extension.target_type.type_name.name, "List");
    EXPECT_EQ(extension.methods.size(), 1);
    
    const auto& method = extension.methods[0];
    EXPECT_EQ(method.name.name, "contains");
    EXPECT_EQ(method.parameters.size(), 1);
    EXPECT_EQ(method.parameters[0].name.name, "item");
    EXPECT_EQ(method.parameters[0].type.type_name.name, "T");
    EXPECT_TRUE(method.has_return_type);
    EXPECT_EQ(method.return_type.type_name.name, "bool");
}

TEST_F(ExtensionMethodTest, ParseMultipleExtensionMethods) {
    std::string input = R"(
        extend String {
            fnc length() -> int {
                rtn 0
            }
            
            fnc isEmpty() -> bool {
                rtn true
            }
            
            fnc charAt(index: int) -> char {
                rtn 'a'
            }
        }
    )";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got an extension block
    auto* ext_block = boost::get<boost::spirit::x3::forward_ast<ast::extension_block>>(&result->get());
    ASSERT_NE(ext_block, nullptr);
    
    const auto& extension = ext_block->get();
    EXPECT_EQ(extension.target_type.type_name.name, "String");
    EXPECT_EQ(extension.methods.size(), 3);
    
    // Check first method
    EXPECT_EQ(extension.methods[0].name.name, "length");
    EXPECT_EQ(extension.methods[0].parameters.size(), 0);
    
    // Check second method
    EXPECT_EQ(extension.methods[1].name.name, "isEmpty");
    EXPECT_EQ(extension.methods[1].parameters.size(), 0);
    
    // Check third method
    EXPECT_EQ(extension.methods[2].name.name, "charAt");
    EXPECT_EQ(extension.methods[2].parameters.size(), 1);
    EXPECT_EQ(extension.methods[2].parameters[0].name.name, "index");
    EXPECT_EQ(extension.methods[2].parameters[0].type.type_name.name, "int");
}

TEST_F(ExtensionMethodTest, ExtensionRegistryBasicOperations) {
    auto& registry = ExtensionRegistry::instance();
    
    // Register an extension method
    auto implementation = [](const std::vector<Value>&) -> Value {
        return Value(); // Placeholder
    };
    
    registry.register_extension(
        "String",
        "length",
        implementation,
        {},  // No parameters
        "int"
    );
    
    // Check that the extension was registered
    EXPECT_TRUE(registry.has_extension("String", "length"));
    EXPECT_FALSE(registry.has_extension("String", "nonexistent"));
    EXPECT_FALSE(registry.has_extension("Int", "length"));
    
    // Look up the extension
    const auto* ext_method = registry.lookup_extension("String", "length");
    ASSERT_NE(ext_method, nullptr);
    EXPECT_EQ(ext_method->method_name, "length");
    EXPECT_EQ(ext_method->target_type_name, "String");
    EXPECT_EQ(ext_method->return_type, "int");
}

TEST_F(ExtensionMethodTest, ExtensionRegistryMultipleMethods) {
    auto& registry = ExtensionRegistry::instance();
    
    auto impl1 = [](const std::vector<Value>&) -> Value { return Value(); };
    auto impl2 = [](const std::vector<Value>&) -> Value { return Value(); };
    
    // Register multiple extension methods for the same type
    registry.register_extension("String", "length", impl1, {}, "int");
    registry.register_extension("String", "isEmpty", impl2, {}, "bool");
    
    // Check both methods are registered
    EXPECT_TRUE(registry.has_extension("String", "length"));
    EXPECT_TRUE(registry.has_extension("String", "isEmpty"));
    
    // Get all extensions for String
    auto extensions = registry.get_extensions_for_type("String");
    EXPECT_EQ(extensions.size(), 2);
    
    // Check that we can find both methods
    bool found_length = false;
    bool found_isEmpty = false;
    for (const auto* ext : extensions) {
        if (ext->method_name == "length") found_length = true;
        if (ext->method_name == "isEmpty") found_isEmpty = true;
    }
    EXPECT_TRUE(found_length);
    EXPECT_TRUE(found_isEmpty);
}

TEST_F(ExtensionMethodTest, ExtensionRegistryWithParameters) {
    auto& registry = ExtensionRegistry::instance();
    
    auto implementation = [](const std::vector<Value>&) -> Value {
        return Value(); // Placeholder
    };
    
    // Register extension method with parameters
    registry.register_extension(
        "List",
        "contains",
        implementation,
        {"T"},  // One parameter of type T
        "bool"
    );
    
    // Check that the extension was registered
    EXPECT_TRUE(registry.has_extension("List", "contains"));
    
    // Look up the extension and check parameters
    const auto* ext_method = registry.lookup_extension("List", "contains");
    ASSERT_NE(ext_method, nullptr);
    EXPECT_EQ(ext_method->parameter_types.size(), 1);
    EXPECT_EQ(ext_method->parameter_types[0], "T");
    EXPECT_EQ(ext_method->return_type, "bool");
}
