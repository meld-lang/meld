/**
 * Tests for interpolation modifier functions (__interpolate-debug__, __interpolate-pretty__).
 *
 * Validates that the interpreter registers built-in interpolation functions
 * and that ${:debug expr} / ${:pretty expr} produce correct structural output.
 *
 * Task 55.2, 55.6 — Interpolation Modifiers (Library-Extensible Formatting)
 */

#include <gtest/gtest.h>
#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/kernel/primitives.hpp>

using namespace meld::interpreter;
using namespace meld::kernel;

class InterpolationModifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = std::make_shared<Environment>();
        interp_ = std::make_unique<AstInterpreter>(env_);
    }

    std::shared_ptr<Environment> env_;
    std::unique_ptr<AstInterpreter> interp_;
};

// ---------------------------------------------------------------------------
// Built-in registration
// ---------------------------------------------------------------------------

TEST_F(InterpolationModifierTest, DefaultFunctionRegistered) {
    EXPECT_NO_THROW(env_->lookup("__interpolate-default__"));
    auto fn = env_->lookup("__interpolate-default__");
    EXPECT_TRUE(fn.is<Function>());
}

TEST_F(InterpolationModifierTest, DebugFunctionRegistered) {
    EXPECT_NO_THROW(env_->lookup("__interpolate-debug__"));
    auto fn = env_->lookup("__interpolate-debug__");
    EXPECT_TRUE(fn.is<Function>());
}

TEST_F(InterpolationModifierTest, PrettyFunctionRegistered) {
    EXPECT_NO_THROW(env_->lookup("__interpolate-pretty__"));
    auto fn = env_->lookup("__interpolate-pretty__");
    EXPECT_TRUE(fn.is<Function>());
}

// ---------------------------------------------------------------------------
// value_to_debug_string — scalar types
// ---------------------------------------------------------------------------

TEST_F(InterpolationModifierTest, DebugInteger) {
    Value val(std::make_shared<Integer>(42));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "int(42)");
}

TEST_F(InterpolationModifierTest, DebugNegativeInteger) {
    Value val(std::make_shared<Integer>(-7));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "int(-7)");
}

TEST_F(InterpolationModifierTest, DebugFloat) {
    Value val(std::make_shared<Float>(3.14));
    auto result = AstInterpreter::value_to_debug_string(val);
    EXPECT_TRUE(result.starts_with("float("));
    EXPECT_TRUE(result.ends_with(")"));
}

TEST_F(InterpolationModifierTest, DebugBoolTrue) {
    Value val(Boolean::from(true));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "bool(true)");
}

TEST_F(InterpolationModifierTest, DebugBoolFalse) {
    Value val(Boolean::from(false));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "bool(false)");
}

TEST_F(InterpolationModifierTest, DebugString) {
    Value val(std::make_shared<String>("hello"));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "string(\"hello\")");
}

TEST_F(InterpolationModifierTest, DebugEmptyString) {
    Value val(std::make_shared<String>(""));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "string(\"\")");
}

TEST_F(InterpolationModifierTest, DebugSymbol) {
    Value val(std::make_shared<Symbol>("foo"));
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "symbol(:foo)");
}

TEST_F(InterpolationModifierTest, DebugNil) {
    Value val(Empty::instance());
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "nil");
}

TEST_F(InterpolationModifierTest, DebugPlaceholder) {
    Value val(Placeholder::instance());
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "placeholder(_)");
}

// ---------------------------------------------------------------------------
// value_to_debug_string — compound types
// ---------------------------------------------------------------------------

TEST_F(InterpolationModifierTest, DebugEmptyVec) {
    Value val(std::make_shared<Vec>());
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "vec([])");
}

TEST_F(InterpolationModifierTest, DebugVecOfInts) {
    auto vec = std::make_shared<Vec>();
    vec->push_back(Value(std::make_shared<Integer>(1)));
    vec->push_back(Value(std::make_shared<Integer>(2)));
    vec->push_back(Value(std::make_shared<Integer>(3)));
    Value val(vec);
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val),
              "vec([int(1), int(2), int(3)])");
}

TEST_F(InterpolationModifierTest, DebugNestedVec) {
    auto inner = std::make_shared<Vec>();
    inner->push_back(Value(std::make_shared<Integer>(10)));
    auto outer = std::make_shared<Vec>();
    outer->push_back(Value(inner));
    outer->push_back(Value(std::make_shared<String>("x")));
    Value val(outer);
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val),
              "vec([vec([int(10)]), string(\"x\")])");
}

TEST_F(InterpolationModifierTest, DebugCons) {
    auto cons = std::make_shared<Cons>(
        Value(std::make_shared<Integer>(1)),
        Value(std::make_shared<Integer>(2)));
    Value val(cons);
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val),
              "cons(int(1), int(2))");
}

TEST_F(InterpolationModifierTest, DebugFunction) {
    std::vector<std::shared_ptr<Symbol>> params;
    auto fn = std::make_shared<Function>(
        std::move(params), Value{}, std::nullopt, "my-func");
    Value val(fn);
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "function(my-func)");
}

TEST_F(InterpolationModifierTest, DebugLambda) {
    std::vector<std::shared_ptr<Symbol>> params;
    auto fn = std::make_shared<Function>(std::move(params), Value{});
    Value val(fn);
    EXPECT_EQ(AstInterpreter::value_to_debug_string(val), "function(<lambda>)");
}

// ---------------------------------------------------------------------------
// value_to_pretty_string
// ---------------------------------------------------------------------------

TEST_F(InterpolationModifierTest, PrettyScalar) {
    Value val(std::make_shared<Integer>(42));
    EXPECT_EQ(AstInterpreter::value_to_pretty_string(val), "int(42)");
}

TEST_F(InterpolationModifierTest, PrettyEmptyVec) {
    Value val(std::make_shared<Vec>());
    EXPECT_EQ(AstInterpreter::value_to_pretty_string(val), "vec([])");
}

TEST_F(InterpolationModifierTest, PrettyVecOfInts) {
    auto vec = std::make_shared<Vec>();
    vec->push_back(Value(std::make_shared<Integer>(1)));
    vec->push_back(Value(std::make_shared<Integer>(2)));
    Value val(vec);
    std::string expected =
        "vec([\n"
        "  int(1),\n"
        "  int(2)\n"
        "])";
    EXPECT_EQ(AstInterpreter::value_to_pretty_string(val), expected);
}

TEST_F(InterpolationModifierTest, PrettyNestedVec) {
    auto inner = std::make_shared<Vec>();
    inner->push_back(Value(std::make_shared<Integer>(10)));
    auto outer = std::make_shared<Vec>();
    outer->push_back(Value(inner));
    Value val(outer);
    std::string expected =
        "vec([\n"
        "  vec([\n"
        "    int(10)\n"
        "  ])\n"
        "])";
    EXPECT_EQ(AstInterpreter::value_to_pretty_string(val), expected);
}

TEST_F(InterpolationModifierTest, PrettyCons) {
    auto cons = std::make_shared<Cons>(
        Value(std::make_shared<Integer>(1)),
        Value(Empty::instance()));
    Value val(cons);
    std::string expected =
        "cons(\n"
        "  int(1),\n"
        "  nil\n"
        ")";
    EXPECT_EQ(AstInterpreter::value_to_pretty_string(val), expected);
}

// ---------------------------------------------------------------------------
// __interpolate-default__ behavior
// ---------------------------------------------------------------------------

TEST_F(InterpolationModifierTest, DefaultStringReturnsRawValue) {
    // Default interpolation of a string should return the raw value, not quoted
    auto fn = env_->lookup("__interpolate-default__");
    auto func = fn.as<Function>();
    ASSERT_TRUE(func->impl().has_value());

    Value input(std::make_shared<String>("hello"));
    auto result = (*func->impl())({input});
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "hello");
}

TEST_F(InterpolationModifierTest, DefaultIntegerReturnsToString) {
    auto fn = env_->lookup("__interpolate-default__");
    auto func = fn.as<Function>();

    Value input(std::make_shared<Integer>(42));
    auto result = (*func->impl())({input});
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "42");
}
