#include <gtest/gtest.h>
#include "meld/macro/handle_macro.hpp"
#include "meld/macro/macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/stdlib/effects.hpp"

using namespace meld;
using namespace meld::macro;
using namespace meld::kernel;

class HandleMacroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register the handle macro
        register_handle_macro();
        
        // Clear any existing state
        MacroRegistry::instance().clear();
        register_handle_macro();
    }
    
    void TearDown() override {
        MacroRegistry::instance().clear();
    }
};

TEST_F(HandleMacroTest, MacroRegistration) {
    // Test that the handle macro is properly registered
    EXPECT_TRUE(MacroRegistry::instance().has_macro("handle"));
    
    auto macro_result = MacroRegistry::instance().get_macro("handle");
    ASSERT_TRUE(macro_result.has_value());
    
    auto macro = *macro_result;
    EXPECT_EQ(macro->name(), "handle");
}

TEST_F(HandleMacroTest, BasicExpansion) {
    // Test basic handle macro expansion
    // Input: (handle { body } FileSystem { (read (path) { return "mock" }) })
    
    // Create the body
    auto body = list({
        Value(std::make_shared<Symbol>("perform")),
        Value(std::make_shared<String>("FileSystem")),
        Value(std::make_shared<String>("read")),
        list({Value(std::make_shared<String>("/test.txt"))})
    });
    
    // Create the effect name
    auto effect_name = Value(std::make_shared<Symbol>("FileSystem"));
    
    // Create handler definitions
    auto handler_def = list({
        Value(std::make_shared<Symbol>("read")),
        list({Value(std::make_shared<Symbol>("path"))}),
        Value(std::make_shared<String>("mock content"))
    });
    
    auto handlers = list({handler_def});
    
    // Create the macro call
    auto macro_call = list({
        Value(std::make_shared<Symbol>("handle")),
        body,
        effect_name,
        handlers
    });
    
    // Expand the macro
    MacroExpander expander;
    auto result = expand_handle_macro(macro_call, expander);
    
    ASSERT_TRUE(result.has_value()) << "Macro expansion failed: " << result.error();
    
    // Verify the result is a block expression
    EXPECT_TRUE(result->is<Cons>());
    
    auto result_list = list_to_array(*result);
    ASSERT_TRUE(result_list.has_value());
    
    // Should start with "block"
    EXPECT_TRUE((*result_list)[0].is<Symbol>());
    EXPECT_EQ((*result_list)[0].as<Symbol>()->name(), "block");
}

TEST_F(HandleMacroTest, HandlerDefinitionParsing) {
    // Test parsing of handler definitions
    
    // Create a handler definition: (read (path) { return "content" })
    auto handler_def = list({
        Value(std::make_shared<Symbol>("read")),
        list({Value(std::make_shared<Symbol>("path"))}),
        Value(std::make_shared<String>("return content"))
    });
    
    auto parsed = parse_single_handler(handler_def);
    ASSERT_TRUE(parsed.has_value()) << "Handler parsing failed: " << parsed.error();
    
    EXPECT_EQ(parsed->operation_name, "read");
    EXPECT_EQ(parsed->parameters.size(), 1);
    EXPECT_EQ(parsed->parameters[0], "path");
    EXPECT_TRUE(parsed->body.is<String>());
}

TEST_F(HandleMacroTest, MultipleHandlers) {
    // Test parsing multiple handler definitions
    
    auto read_handler = list({
        Value(std::make_shared<Symbol>("read")),
        list({Value(std::make_shared<Symbol>("path"))}),
        Value(std::make_shared<String>("read implementation"))
    });
    
    auto write_handler = list({
        Value(std::make_shared<Symbol>("write")),
        list({
            Value(std::make_shared<Symbol>("path")),
            Value(std::make_shared<Symbol>("content"))
        }),
        Value(std::make_shared<String>("write implementation"))
    });
    
    auto handlers_list = list({read_handler, write_handler});
    
    auto parsed = parse_handler_definitions(handlers_list);
    ASSERT_TRUE(parsed.has_value()) << "Multiple handlers parsing failed: " << parsed.error();
    
    EXPECT_EQ(parsed->size(), 2);
    
    // Check first handler (read)
    EXPECT_EQ((*parsed)[0].operation_name, "read");
    EXPECT_EQ((*parsed)[0].parameters.size(), 1);
    EXPECT_EQ((*parsed)[0].parameters[0], "path");
    
    // Check second handler (write)
    EXPECT_EQ((*parsed)[1].operation_name, "write");
    EXPECT_EQ((*parsed)[1].parameters.size(), 2);
    EXPECT_EQ((*parsed)[1].parameters[0], "path");
    EXPECT_EQ((*parsed)[1].parameters[1], "content");
}

TEST_F(HandleMacroTest, ErrorHandling) {
    // Test error handling for invalid inputs
    
    MacroExpander expander;
    
    // Test with wrong number of arguments
    auto invalid_call = list({
        Value(std::make_shared<Symbol>("handle")),
        Value(std::make_shared<String>("only_one_arg"))
    });
    
    auto result = expand_handle_macro(invalid_call, expander);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("expects 3 arguments") != std::string::npos);
    
    // Test with non-symbol effect name
    auto invalid_effect_call = list({
        Value(std::make_shared<Symbol>("handle")),
        Value(std::make_shared<String>("body")),
        Value(std::make_shared<String>("NotASymbol")), // Should be a symbol
        list({})
    });
    
    result = expand_handle_macro(invalid_effect_call, expander);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("effect name must be a symbol") != std::string::npos);
}

TEST_F(HandleMacroTest, EmptyHandlers) {
    // Test with empty handler list
    
    auto body = Value(std::make_shared<String>("test body"));
    auto effect_name = Value(std::make_shared<Symbol>("TestEffect"));
    auto empty_handlers = list({});
    
    auto macro_call = list({
        Value(std::make_shared<Symbol>("handle")),
        body,
        effect_name,
        empty_handlers
    });
    
    MacroExpander expander;
    auto result = expand_handle_macro(macro_call, expander);
    
    // Should succeed with empty handlers
    ASSERT_TRUE(result.has_value()) << "Empty handlers expansion failed: " << result.error();
}

TEST_F(HandleMacroTest, ComplexHandlerBody) {
    // Test with complex handler body (nested expressions)
    
    auto complex_body = list({
        Value(std::make_shared<Symbol>("begin")),
        list({
            Value(std::make_shared<Symbol>("val")),
            Value(std::make_shared<Symbol>("result")),
            Value(std::make_shared<String>("computed value"))
        }),
        Value(std::make_shared<Symbol>("result"))
    });
    
    auto handler_def = list({
        Value(std::make_shared<Symbol>("compute")),
        list({}), // No parameters
        complex_body
    });
    
    auto parsed = parse_single_handler(handler_def);
    ASSERT_TRUE(parsed.has_value()) << "Complex handler parsing failed: " << parsed.error();
    
    EXPECT_EQ(parsed->operation_name, "compute");
    EXPECT_EQ(parsed->parameters.size(), 0);
    EXPECT_TRUE(parsed->body.is<Cons>());
}