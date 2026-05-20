#include <gtest/gtest.h>
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"

using namespace meld::kernel;

TEST(SymbolTest, Interning) {
    auto sym1 = SymbolTable::instance().intern("foo");
    auto sym2 = SymbolTable::instance().intern("foo");
    EXPECT_EQ(sym1, sym2);  // Same pointer
}

TEST(SymbolTest, Gensym) {
    auto sym1 = SymbolTable::instance().gensym("test");
    auto sym2 = SymbolTable::instance().gensym("test");
    EXPECT_NE(sym1, sym2);  // Different pointers
    EXPECT_NE(sym1->name(), sym2->name());  // Different names
}

TEST(SymbolTest, ToString) {
    auto sym = SymbolTable::instance().intern("hello");
    EXPECT_EQ(sym->to_string(), ":hello");
}

TEST(IntegerTest, Value) {
    Integer num(42);
    EXPECT_EQ(num.value(), 42);
    EXPECT_EQ(num.to_string(), "42");
}

TEST(IntegerTest, Equality) {
    Integer num1(42);
    Integer num2(42);
    Integer num3(99);
    EXPECT_TRUE(num1 == num2);
    EXPECT_FALSE(num1 == num3);
}

TEST(BooleanTest, Singletons) {
    auto t1 = Boolean::true_value();
    auto t2 = Boolean::true_value();
    auto f1 = Boolean::false_value();
    auto f2 = Boolean::false_value();
    
    EXPECT_EQ(t1, t2);
    EXPECT_EQ(f1, f2);
    EXPECT_NE(t1, f1);
}

TEST(BooleanTest, From) {
    EXPECT_EQ(Boolean::from(true), Boolean::true_value());
    EXPECT_EQ(Boolean::from(false), Boolean::false_value());
}

TEST(StringTest, Value) {
    String str("hello");
    EXPECT_EQ(str.value(), "hello");
    EXPECT_EQ(str.to_string(), "\"hello\"");
}

TEST(EmptyTest, Singleton) {
    auto e1 = Empty::instance();
    auto e2 = Empty::instance();
    EXPECT_EQ(e1, e2);
}

TEST(OptionalTest, Some) {
    auto opt = Optional<Value>::some(Value(std::make_shared<Integer>(42)));
    EXPECT_TRUE(opt->is_some());
    EXPECT_FALSE(opt->is_empty());
}

TEST(OptionalTest, None) {
    auto opt = Optional<Value>::none();
    EXPECT_TRUE(opt->is_empty());
    EXPECT_FALSE(opt->is_some());
}

TEST(OptionalTest, Get) {
    auto opt = Optional<Value>::some(Value(std::make_shared<Integer>(42)));
    auto result = opt->get();
    EXPECT_TRUE(result.has_value());
    
    auto none = Optional<Value>::none();
    auto none_result = none->get();
    EXPECT_FALSE(none_result.has_value());
}

TEST(OptionalTest, GetOrElse) {
    auto some = Optional<Value>::some(Value(std::make_shared<Integer>(42)));
    auto none = Optional<Value>::none();
    auto default_val = Value(std::make_shared<Integer>(99));
    
    EXPECT_TRUE(some->get_or_else(default_val).is<Integer>());
    EXPECT_TRUE(none->get_or_else(default_val).is<Integer>());
}

TEST(ConsTest, CarCdr) {
    auto car_val = Value(std::make_shared<Integer>(1));
    auto cdr_val = Value(std::make_shared<Integer>(2));
    Cons cell(car_val, cdr_val);
    
    EXPECT_TRUE(cell.car().is<Integer>());
    EXPECT_TRUE(cell.cdr().is<Integer>());
}

TEST(ValueTest, IsTruthy) {
    EXPECT_TRUE(Value(Boolean::true_value()).is_truthy());
    EXPECT_FALSE(Value(Boolean::false_value()).is_truthy());
    EXPECT_FALSE(Value(Empty::instance()).is_truthy());
    EXPECT_TRUE(Value(std::make_shared<Integer>(0)).is_truthy());
}

// ============================================================================
// INTEROP PRIMITIVES TESTS
// ============================================================================

TEST(NativeCallTest, LogicalShiftRight) {
    // Test unsigned right shift
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(-1)),  // All bits set
        Value(std::make_shared<Integer>(1))    // Shift by 1
    };
    
    Value result = native_call("logical_shift_right", args);
    EXPECT_TRUE(result.is<Integer>());
    
    // -1 as unsigned is 0xFFFFFFFFFFFFFFFF
    // Shifted right by 1 should be 0x7FFFFFFFFFFFFFFF
    EXPECT_EQ(result.as<Integer>()->value(), 0x7FFFFFFFFFFFFFFF);
}

TEST(NativeCallTest, BitwiseAnd) {
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(0b1100)),
        Value(std::make_shared<Integer>(0b1010))
    };
    
    Value result = native_call("bitwise_and", args);
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0b1000);
}

TEST(NativeCallTest, BitwiseOr) {
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(0b1100)),
        Value(std::make_shared<Integer>(0b1010))
    };
    
    Value result = native_call("bitwise_or", args);
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0b1110);
}

TEST(NativeCallTest, BitwiseXor) {
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(0b1100)),
        Value(std::make_shared<Integer>(0b1010))
    };
    
    Value result = native_call("bitwise_xor", args);
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0b0110);
}

TEST(NativeCallTest, UnknownFunction) {
    std::vector<Value> args;
    EXPECT_THROW(native_call("unknown_function", args), std::runtime_error);
}

TEST(NativeCallTest, WrongArgumentCount) {
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(42))
    };
    EXPECT_THROW(native_call("logical_shift_right", args), std::runtime_error);
}

TEST(NativeCallTest, WrongArgumentType) {
    std::vector<Value> args = {
        Value(std::make_shared<String>("not a number")),
        Value(std::make_shared<Integer>(1))
    };
    EXPECT_THROW(native_call("logical_shift_right", args), std::runtime_error);
}

TEST(NativeLoadTest, InvalidPath) {
    // Test loading a non-existent library
    EXPECT_THROW(native_load("/nonexistent/library.so"), std::runtime_error);
}

TEST(NativeHandleTest, ToString) {
    // We can't easily test actual library loading in a unit test,
    // but we can test the NativeHandle class directly
    void* dummy_handle = reinterpret_cast<void*>(0x12345678);
    NativeHandle handle(dummy_handle, "/path/to/lib.so");
    
    EXPECT_EQ(handle.handle(), dummy_handle);
    EXPECT_EQ(handle.path(), "/path/to/lib.so");
    EXPECT_EQ(handle.to_string(), "<native-handle:/path/to/lib.so>");
}

TEST(NativeFunctionTest, ToString) {
    void* dummy_ptr = reinterpret_cast<void*>(0x87654321);
    NativeFunction func(dummy_ptr, "int(int, int)", "add");
    
    EXPECT_EQ(func.ptr(), dummy_ptr);
    EXPECT_EQ(func.signature(), "int(int, int)");
    EXPECT_EQ(func.name(), "add");
    EXPECT_EQ(func.to_string(), "<native-function:add:int(int, int)>");
}

TEST(NativeFunctionTest, ToStringNoName) {
    void* dummy_ptr = reinterpret_cast<void*>(0x87654321);
    NativeFunction func(dummy_ptr, "void()", "");
    
    EXPECT_EQ(func.to_string(), "<native-function:void()>");
}
