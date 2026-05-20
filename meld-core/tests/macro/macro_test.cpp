#include <gtest/gtest.h>
#include "meld/macro/macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"

using namespace meld;
using namespace meld::macro;
using namespace meld::kernel;

class MacroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        MacroRegistry::instance().clear();
    }
    
    void TearDown() override {
        // Clear registry after each test
        MacroRegistry::instance().clear();
    }
};

TEST_F(MacroTest, RegisterAndLookupMacro) {
    // Create a simple macro
    auto macro = make_simple_macro(
        "test-macro",
        {"x"},
        [](const std::vector<Value>& args) {
            return args[0];
        }
    );
    
    // Register it
    MacroRegistry::instance().register_macro(macro);
    
    // Look it up
    auto result = MacroRegistry::instance().get_macro("test-macro");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name(), "test-macro");
}

TEST_F(MacroTest, LookupNonexistentMacro) {
    auto result = MacroRegistry::instance().get_macro("nonexistent");
    ASSERT_FALSE(result.has_value());
}

TEST_F(MacroTest, HasMacro) {
    auto macro = make_simple_macro(
        "test-macro",
        {"x"},
        [](const std::vector<Value>& args) {
            return args[0];
        }
    );
    
    MacroRegistry::instance().register_macro(macro);
    
    EXPECT_TRUE(MacroRegistry::instance().has_macro("test-macro"));
    EXPECT_FALSE(MacroRegistry::instance().has_macro("nonexistent"));
}

TEST_F(MacroTest, SimpleMacroExpansion) {
    // Create a macro that doubles an integer literal
    auto double_macro = make_simple_macro(
        "double",
        {"x"},
        [](const std::vector<Value>& args) {
            // Extract integer from int-literal form
            if (args[0].is<Cons>()) {
                auto list_result = list_to_array(args[0]);
                if (list_result && list_result->size() == 2) {
                    auto& elements = *list_result;
                    if (elements[1].is<Integer>()) {
                        int64_t val = elements[1].as<Integer>()->value();
                        return make_int_literal(val * 2);
                    }
                }
            }
            return args[0];
        }
    );
    
    MacroRegistry::instance().register_macro(double_macro);
    
    // Create a macro call: (double (int-literal 21))
    auto int_lit = make_int_literal(21);
    auto macro_call = list({
        Value(SymbolTable::instance().intern("double")),
        int_lit
    });
    
    // Expand it
    MacroExpander expander;
    auto result = expander.expand(macro_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that it's (int-literal 42)
    auto expanded_list = list_to_array(*result);
    ASSERT_TRUE(expanded_list.has_value());
    ASSERT_EQ(expanded_list->size(), 2);
    
    auto& elements = *expanded_list;
    ASSERT_TRUE(elements[0].is<Symbol>());
    EXPECT_EQ(elements[0].as<Symbol>()->name(), "int-literal");
    
    ASSERT_TRUE(elements[1].is<Integer>());
    EXPECT_EQ(elements[1].as<Integer>()->value(), 42);
}

TEST_F(MacroTest, IsMacroCall) {
    // Register a macro
    auto macro = make_simple_macro(
        "test",
        {"x"},
        [](const std::vector<Value>& args) {
            return args[0];
        }
    );
    MacroRegistry::instance().register_macro(macro);
    
    MacroExpander expander;
    
    // Create a macro call
    auto macro_call = list({
        Value(SymbolTable::instance().intern("test")),
        Value(std::make_shared<Integer>(42))
    });
    
    EXPECT_TRUE(expander.is_macro_call(macro_call));
    
    // Create a non-macro call
    auto non_macro_call = list({
        Value(SymbolTable::instance().intern("not-a-macro")),
        Value(std::make_shared<Integer>(42))
    });
    
    EXPECT_FALSE(expander.is_macro_call(non_macro_call));
    
    // Non-list is not a macro call
    auto non_list = Value(std::make_shared<Integer>(42));
    EXPECT_FALSE(expander.is_macro_call(non_list));
}

TEST_F(MacroTest, GetMacroName) {
    MacroExpander expander;
    
    auto macro_call = list({
        Value(SymbolTable::instance().intern("test-macro")),
        Value(std::make_shared<Integer>(42))
    });
    
    auto name_result = expander.get_macro_name(macro_call);
    ASSERT_TRUE(name_result.has_value());
    EXPECT_EQ(*name_result, "test-macro");
}

TEST_F(MacroTest, GetMacroArgs) {
    MacroExpander expander;
    
    auto arg1 = Value(std::make_shared<Integer>(42));
    auto arg2 = Value(std::make_shared<String>("hello"));
    
    auto macro_call = list({
        Value(SymbolTable::instance().intern("test-macro")),
        arg1,
        arg2
    });
    
    auto args_result = expander.get_macro_args(macro_call);
    ASSERT_TRUE(args_result.has_value());
    ASSERT_EQ(args_result->size(), 2);
    
    EXPECT_TRUE(equal((*args_result)[0], arg1));
    EXPECT_TRUE(equal((*args_result)[1], arg2));
}

TEST_F(MacroTest, RecursiveMacroExpansion) {
    // Create two macros: inc and double
    auto inc_macro = make_simple_macro(
        "inc",
        {"x"},
        [](const std::vector<Value>& args) {
            if (args[0].is<Cons>()) {
                auto list_result = list_to_array(args[0]);
                if (list_result && list_result->size() == 2) {
                    auto& elements = *list_result;
                    if (elements[1].is<Integer>()) {
                        int64_t val = elements[1].as<Integer>()->value();
                        return make_int_literal(val + 1);
                    }
                }
            }
            return args[0];
        }
    );
    
    auto double_macro = make_simple_macro(
        "double",
        {"x"},
        [](const std::vector<Value>& args) {
            // Return (inc (inc x))
            return list({
                Value(SymbolTable::instance().intern("inc")),
                list({
                    Value(SymbolTable::instance().intern("inc")),
                    args[0]
                })
            });
        }
    );
    
    MacroRegistry::instance().register_macro(inc_macro);
    MacroRegistry::instance().register_macro(double_macro);
    
    // Create: (double (int-literal 10))
    auto macro_call = list({
        Value(SymbolTable::instance().intern("double")),
        make_int_literal(10)
    });
    
    // Expand recursively
    MacroExpander expander;
    auto result = expander.expand_recursive(macro_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Should expand to (int-literal 12)
    auto expanded_list = list_to_array(*result);
    ASSERT_TRUE(expanded_list.has_value());
    ASSERT_EQ(expanded_list->size(), 2);
    
    auto& elements = *expanded_list;
    ASSERT_TRUE(elements[1].is<Integer>());
    EXPECT_EQ(elements[1].as<Integer>()->value(), 12);
}

TEST_F(MacroTest, MaxExpansionDepth) {
    // Create a macro that calls itself
    auto recursive_macro = make_simple_macro(
        "recursive",
        {"x"},
        [](const std::vector<Value>& args) {
            // Return (recursive x)
            return list({
                Value(SymbolTable::instance().intern("recursive")),
                args[0]
            });
        }
    );
    
    MacroRegistry::instance().register_macro(recursive_macro);
    
    auto macro_call = list({
        Value(SymbolTable::instance().intern("recursive")),
        Value(std::make_shared<Integer>(42))
    });
    
    MacroExpander expander;
    auto result = expander.expand(macro_call);
    
    // Should fail due to max depth
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("depth exceeded") != std::string::npos);
}

TEST_F(MacroTest, Gensym) {
    MacroExpander expander;
    
    auto sym1 = expander.gensym("test");
    auto sym2 = expander.gensym("test");
    
    // Should generate unique symbols
    EXPECT_NE(sym1->name(), sym2->name());
    EXPECT_TRUE(sym1->name().find("test") != std::string::npos);
    EXPECT_TRUE(sym2->name().find("test") != std::string::npos);
}

TEST_F(MacroTest, RenameSymbols) {
    MacroExpander expander;
    
    // Create an AST: (let x (+ x 1))
    auto x_sym = SymbolTable::instance().intern("x");
    auto let_sym = SymbolTable::instance().intern("let");
    auto plus_sym = SymbolTable::instance().intern("+");
    
    auto ast = list({
        Value(let_sym),
        Value(x_sym),
        list({
            Value(plus_sym),
            Value(x_sym),
            Value(std::make_shared<Integer>(1))
        })
    });
    
    // Create rename map: x -> x_renamed
    auto x_renamed = SymbolTable::instance().intern("x_renamed");
    std::map<std::string, std::shared_ptr<Symbol>> renames = {
        {"x", x_renamed}
    };
    
    // Rename symbols
    auto renamed = expander.rename_symbols(ast, renames);
    
    // Check that x was renamed to x_renamed
    auto renamed_list = list_to_array(renamed);
    ASSERT_TRUE(renamed_list.has_value());
    ASSERT_EQ(renamed_list->size(), 3);
    
    // Second element should be x_renamed
    ASSERT_TRUE((*renamed_list)[1].is<Symbol>());
    EXPECT_EQ((*renamed_list)[1].as<Symbol>()->name(), "x_renamed");
    
    // Third element is the (+ x 1) expression
    auto inner_list = list_to_array((*renamed_list)[2]);
    ASSERT_TRUE(inner_list.has_value());
    ASSERT_EQ(inner_list->size(), 3);
    
    // x in the expression should also be renamed
    ASSERT_TRUE((*inner_list)[1].is<Symbol>());
    EXPECT_EQ((*inner_list)[1].as<Symbol>()->name(), "x_renamed");
}

TEST_F(MacroTest, UnhygienicAnnotation) {
    MacroExpander expander;
    
    // Mark 'x' as unhygienic
    expander.mark_unhygienic("x");
    
    EXPECT_TRUE(expander.is_unhygienic("x"));
    EXPECT_FALSE(expander.is_unhygienic("y"));
    
    // Create an AST with x and y
    auto x_sym = SymbolTable::instance().intern("x");
    auto y_sym = SymbolTable::instance().intern("y");
    
    auto ast = list({
        Value(x_sym),
        Value(y_sym)
    });
    
    // Try to rename both
    auto x_renamed = SymbolTable::instance().intern("x_renamed");
    auto y_renamed = SymbolTable::instance().intern("y_renamed");
    std::map<std::string, std::shared_ptr<Symbol>> renames = {
        {"x", x_renamed},
        {"y", y_renamed}
    };
    
    auto renamed = expander.rename_symbols(ast, renames);
    
    // Check results
    auto renamed_list = list_to_array(renamed);
    ASSERT_TRUE(renamed_list.has_value());
    ASSERT_EQ(renamed_list->size(), 2);
    
    // x should NOT be renamed (unhygienic)
    ASSERT_TRUE((*renamed_list)[0].is<Symbol>());
    EXPECT_EQ((*renamed_list)[0].as<Symbol>()->name(), "x");
    
    // y should be renamed
    ASSERT_TRUE((*renamed_list)[1].is<Symbol>());
    EXPECT_EQ((*renamed_list)[1].as<Symbol>()->name(), "y_renamed");
}

TEST_F(MacroTest, MacroScopeIsolation) {
    MacroExpander expander;
    
    auto& scope = expander.scope();
    
    // Push a scope and register a symbol
    scope.push_scope();
    auto sym1 = SymbolTable::instance().intern("test");
    scope.register_symbol("test", sym1);
    
    EXPECT_TRUE(scope.has_symbol("test"));
    EXPECT_EQ(scope.lookup_symbol("test"), sym1);
    
    // Push another scope
    scope.push_scope();
    auto sym2 = SymbolTable::instance().intern("test2");
    scope.register_symbol("test2", sym2);
    
    // Both symbols should be visible
    EXPECT_TRUE(scope.has_symbol("test"));
    EXPECT_TRUE(scope.has_symbol("test2"));
    
    // Pop the inner scope
    scope.pop_scope();
    
    // Only outer symbol should be visible
    EXPECT_TRUE(scope.has_symbol("test"));
    EXPECT_FALSE(scope.has_symbol("test2"));
    
    // Pop the outer scope
    scope.pop_scope();
    
    // No symbols should be visible
    EXPECT_FALSE(scope.has_symbol("test"));
}
