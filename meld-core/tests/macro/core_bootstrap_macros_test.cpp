// ============================================================
// Core Bootstrapping Macros — Unit Tests
// ============================================================
//
// Tests the `if` and `while` macro expansions, plus verifies
// the existing `class` macro's constructor/method generation.

#include <gtest/gtest.h>
#include "meld/macro/bootstrap.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::kernel;
using namespace meld::macro;

// ============================================================
// if / while macro creators (same as in the demo .cpp)
// ============================================================

static std::shared_ptr<Macro> create_if_macro() {
    auto transformer = [](const Value& ast_node, MacroExpander& expander)
        -> std::expected<Value, std::string> {

        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) return std::unexpected(args_result.error());

        auto& args = *args_result;
        if (args.size() < 2 || args.size() > 3)
            return std::unexpected("if: requires 2 or 3 args");

        Value cond       = args[0];
        Value then_block = args[1];
        Value else_block = (args.size() == 3)
            ? args[2]
            : Value(Empty::instance());

        auto& st = SymbolTable::instance();

        auto make_thunk = [&](const Value& body) -> Value {
            return list({Value(st.intern("lambda")), list({}), body});
        };

        return list({
            Value(st.intern("__intrinsic_if")),
            cond,
            make_thunk(then_block),
            make_thunk(else_block)
        });
    };
    return make_macro("if", {"cond", "then", "else"}, transformer);
}

static std::shared_ptr<Macro> create_while_macro() {
    auto transformer = [](const Value& ast_node, MacroExpander& expander)
        -> std::expected<Value, std::string> {

        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) return std::unexpected(args_result.error());

        auto& args = *args_result;
        if (args.size() != 2)
            return std::unexpected("while: requires 2 args");

        Value cond = args[0];
        Value body = args[1];
        auto& st   = SymbolTable::instance();

        auto loop_sym  = expander.gensym("__loop");
        Value loop_call = list({Value(loop_sym)});

        Value true_branch = list({
            Value(st.intern("do")), body, loop_call
        });

        Value if_node = list({
            Value(st.intern("if")), cond, true_branch,
            Value(Empty::instance())
        });

        Value loop_fn = list({
            Value(st.intern("fnc")), Value(loop_sym), list({}), if_node
        });

        return list({
            Value(st.intern("begin")), loop_fn, loop_call
        });
    };
    return make_macro("while", {"cond", "body"}, transformer);
}

// ============================================================
// Helpers
// ============================================================

static std::string head_sym(const Value& ast) {
    auto r = car(ast);
    if (r && r->is<Symbol>()) return r->as<Symbol>()->name();
    return "";
}

static std::vector<Value> to_vec(const Value& ast) {
    auto r = list_to_array(ast);
    return r ? *r : std::vector<Value>{};
}

// ============================================================
// Test fixture
// ============================================================

class CoreBootstrapMacrosTest : public ::testing::Test {
protected:
    void SetUp() override {
        MacroRegistry::instance().clear();
        MacroRegistry::instance().register_macro(create_if_macro());
        MacroRegistry::instance().register_macro(create_while_macro());
        register_bootstrap_macros();  // registers class, struct, trait, effect
    }
    void TearDown() override {
        MacroRegistry::instance().clear();
    }
};

// ============================================================
// if macro tests
// ============================================================

TEST_F(CoreBootstrapMacrosTest, IfExpandsToIntrinsicIf) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("if")),
        Value(st.intern("cond")),
        Value(std::make_shared<String>("yes")),
        Value(std::make_shared<String>("no"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(head_sym(*result), "__intrinsic_if");
}

TEST_F(CoreBootstrapMacrosTest, IfWrapsInThunks) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("if")),
        Value(st.intern("cond")),
        Value(std::make_shared<String>("a")),
        Value(std::make_shared<String>("b"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    auto elems = to_vec(*result);
    ASSERT_EQ(elems.size(), 4u);

    // elems[2] and elems[3] should be (lambda () ...)
    EXPECT_EQ(head_sym(elems[2]), "lambda");
    EXPECT_EQ(head_sym(elems[3]), "lambda");

    // Each thunk has exactly 3 elements: (lambda () body)
    EXPECT_EQ(to_vec(elems[2]).size(), 3u);
    EXPECT_EQ(to_vec(elems[3]).size(), 3u);
}

TEST_F(CoreBootstrapMacrosTest, IfWithoutElseDefaultsToNil) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("if")),
        Value(st.intern("cond")),
        Value(std::make_shared<String>("yes"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    auto elems = to_vec(*result);
    ASSERT_EQ(elems.size(), 4u);

    // else thunk body should be Empty (nil)
    auto else_thunk = to_vec(elems[3]);
    ASSERT_EQ(else_thunk.size(), 3u);
    EXPECT_TRUE(else_thunk[2].is<Empty>());
}

TEST_F(CoreBootstrapMacrosTest, IfPreservesCondition) {
    auto& st = SymbolTable::instance();

    Value cond_expr = list({
        Value(st.intern(">")),
        Value(st.intern("x")),
        Value(std::make_shared<Integer>(0))
    });

    Value ast = list({
        Value(st.intern("if")),
        cond_expr,
        Value(std::make_shared<String>("pos")),
        Value(std::make_shared<String>("neg"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    auto elems = to_vec(*result);
    // elems[1] is the condition — should be the same (> x 0) list
    EXPECT_EQ(head_sym(elems[1]), ">");
}

TEST_F(CoreBootstrapMacrosTest, IfRejectsWrongArgCount) {
    auto& st = SymbolTable::instance();

    // Zero args
    Value ast0 = list({Value(st.intern("if"))});
    MacroExpander exp;
    auto r0 = exp.expand(ast0);
    EXPECT_FALSE(r0.has_value());

    // One arg
    Value ast1 = list({
        Value(st.intern("if")),
        Value(st.intern("cond"))
    });
    auto r1 = exp.expand(ast1);
    EXPECT_FALSE(r1.has_value());

    // Four args
    Value ast4 = list({
        Value(st.intern("if")),
        Value(st.intern("a")),
        Value(st.intern("b")),
        Value(st.intern("c")),
        Value(st.intern("d"))
    });
    auto r4 = exp.expand(ast4);
    EXPECT_FALSE(r4.has_value());
}

// ============================================================
// while macro tests
// ============================================================

TEST_F(CoreBootstrapMacrosTest, WhileExpandsToBeginBlock) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("while")),
        Value(st.intern("cond")),
        Value(st.intern("body"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(head_sym(*result), "begin");
}

TEST_F(CoreBootstrapMacrosTest, WhileContainsFncAndCall) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("while")),
        Value(st.intern("cond")),
        Value(st.intern("body"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    auto elems = to_vec(*result);
    ASSERT_EQ(elems.size(), 3u);  // (begin fnc-def call)

    // elems[1] is the fnc definition
    EXPECT_EQ(head_sym(elems[1]), "fnc");

    // Extract the gensym'd name from the fnc definition
    auto fnc_parts = to_vec(elems[1]);
    ASSERT_GE(fnc_parts.size(), 4u);
    ASSERT_TRUE(fnc_parts[1].is<Symbol>());
    std::string loop_name = fnc_parts[1].as<Symbol>()->name();

    // elems[2] is the call to the loop function
    auto call_parts = to_vec(elems[2]);
    ASSERT_EQ(call_parts.size(), 1u);
    EXPECT_EQ(call_parts[0].as<Symbol>()->name(), loop_name);
}

TEST_F(CoreBootstrapMacrosTest, WhileUsesHygienicSymbol) {
    auto& st = SymbolTable::instance();

    // Expand two while macros — they should get different loop names
    Value ast1 = list({
        Value(st.intern("while")),
        Value(st.intern("c1")),
        Value(st.intern("b1"))
    });
    Value ast2 = list({
        Value(st.intern("while")),
        Value(st.intern("c2")),
        Value(st.intern("b2"))
    });

    MacroExpander exp;
    auto r1 = exp.expand(ast1);
    auto r2 = exp.expand(ast2);
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());

    auto e1 = to_vec(*r1);
    auto e2 = to_vec(*r2);

    auto fnc1 = to_vec(e1[1]);
    auto fnc2 = to_vec(e2[1]);

    std::string name1 = fnc1[1].as<Symbol>()->name();
    std::string name2 = fnc2[1].as<Symbol>()->name();

    // Gensym should produce distinct names
    EXPECT_NE(name1, name2);
    // Both should start with the __loop prefix
    EXPECT_TRUE(name1.find("__loop") != std::string::npos);
    EXPECT_TRUE(name2.find("__loop") != std::string::npos);
}

TEST_F(CoreBootstrapMacrosTest, WhileBodyContainsIfWithRecursiveCall) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("while")),
        Value(st.intern("cond")),
        Value(st.intern("body"))
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    auto elems = to_vec(*result);
    auto fnc_parts = to_vec(elems[1]);

    // fnc_parts[3] is the if node
    EXPECT_EQ(head_sym(fnc_parts[3]), "if");

    auto if_parts = to_vec(fnc_parts[3]);
    ASSERT_EQ(if_parts.size(), 4u);  // (if cond true_branch nil)

    // True branch is (do body (loop_call))
    EXPECT_EQ(head_sym(if_parts[2]), "do");

    // Else branch is nil
    EXPECT_TRUE(if_parts[3].is<Empty>());
}

TEST_F(CoreBootstrapMacrosTest, WhileRejectsWrongArgCount) {
    auto& st = SymbolTable::instance();

    Value ast1 = list({
        Value(st.intern("while")),
        Value(st.intern("cond"))
    });

    MacroExpander exp;
    auto r1 = exp.expand(ast1);
    EXPECT_FALSE(r1.has_value());

    Value ast3 = list({
        Value(st.intern("while")),
        Value(st.intern("a")),
        Value(st.intern("b")),
        Value(st.intern("c"))
    });
    auto r3 = exp.expand(ast3);
    EXPECT_FALSE(r3.has_value());
}

// ============================================================
// class macro tests (existing macro, new verification angles)
// ============================================================

TEST_F(CoreBootstrapMacrosTest, ClassGeneratesConstructorAndMethods) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("class")),
        Value(st.intern("Account")),
        list({
            Value(st.intern("field")),
            Value(st.intern("owner")),
            Value(st.intern("String"))
        }),
        list({
            Value(st.intern("field")),
            Value(st.intern("balance")),
            Value(st.intern("Int"))
        }),
        list({
            Value(st.intern("method")),
            Value(st.intern("deposit")),
            list({Value(st.intern("Int"))}),
            Value(st.intern("Int")),
            Value(st.intern("begin"))
        })
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(head_sym(*result), "begin");

    // Verify type registration
    auto type_r = meta::TypeRegistry::instance().get_type("Account");
    ASSERT_TRUE(type_r.has_value());

    auto cls = std::dynamic_pointer_cast<meta::ClassMetaType>(*type_r);
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->fields().size(), 2u);
    EXPECT_EQ(cls->fields()[0].name, "owner");
    EXPECT_EQ(cls->fields()[1].name, "balance");
    EXPECT_EQ(cls->methods().size(), 1u);
    EXPECT_EQ(cls->methods()[0].name, "deposit");
}

TEST_F(CoreBootstrapMacrosTest, ClassMethodsGetSelfParameter) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("class")),
        Value(st.intern("Greeter")),
        list({
            Value(st.intern("field")),
            Value(st.intern("msg")),
            Value(st.intern("String"))
        }),
        list({
            Value(st.intern("method")),
            Value(st.intern("say")),
            list({}),
            Value(st.intern("String")),
            Value(st.intern("begin"))
        })
    });

    MacroExpander exp;
    auto result = exp.expand(ast);
    ASSERT_TRUE(result.has_value());

    // The expanded AST should contain a method fn with "self" as first param
    // Structure: (begin (val Greeter ...) (fn Greeter.new ...) (fn Greeter.say (self) ...))
    auto elems = to_vec(*result);
    ASSERT_GE(elems.size(), 3u);

    // Find the method definition (last element should be the method fn)
    bool found_method = false;
    for (const auto& elem : elems) {
        if (!elem.is<Cons>()) continue;
        auto parts = to_vec(elem);
        if (parts.size() >= 3 && parts[0].is<Symbol>() &&
            parts[0].as<Symbol>()->name() == "fn" &&
            parts[1].is<Symbol>() &&
            parts[1].as<Symbol>()->name() == "Greeter.say") {

            found_method = true;
            // Check params list contains "self"
            auto params = to_vec(parts[2]);
            ASSERT_GE(params.size(), 1u);
            EXPECT_EQ(params[0].as<Symbol>()->name(), "self");
        }
    }
    EXPECT_TRUE(found_method) << "Expected to find Greeter.say method definition";
}

// ============================================================
// Macro composition: while's inner if gets expanded recursively
// ============================================================

TEST_F(CoreBootstrapMacrosTest, WhileInnerIfExpandsRecursively) {
    auto& st = SymbolTable::instance();

    Value ast = list({
        Value(st.intern("while")),
        Value(st.intern("cond")),
        Value(st.intern("body"))
    });

    MacroExpander exp;
    auto result = exp.expand_recursive(ast);
    ASSERT_TRUE(result.has_value());

    // After recursive expansion, the inner `if` should become `__intrinsic_if`
    // Walk the tree to find it
    std::function<bool(const Value&)> contains_intrinsic_if;
    contains_intrinsic_if = [&](const Value& v) -> bool {
        if (v.is<Symbol>() && v.as<Symbol>()->name() == "__intrinsic_if")
            return true;
        if (!v.is<Cons>()) return false;
        auto arr = to_vec(v);
        for (const auto& e : arr) {
            if (contains_intrinsic_if(e)) return true;
        }
        return false;
    };

    EXPECT_TRUE(contains_intrinsic_if(*result))
        << "Recursive expansion should rewrite inner if -> __intrinsic_if";
}
