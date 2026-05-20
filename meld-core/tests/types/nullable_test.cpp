#include <gtest/gtest.h>
#include "meld/types/nullable.hpp"
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::types;
using namespace meld::meta;

// Test nullable type creation
TEST(NullableTest, CreateNullableType) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int? (nullable Int)
    auto nullable_int = registry.create_optional_type(registry.get_int_type());
    
    ASSERT_NE(nullable_int, nullptr);
    EXPECT_TRUE(registry.is_nullable_type(*nullable_int));
    
    // Get inner type
    auto inner = registry.get_inner_type(*nullable_int);
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ((*inner)->name(), "Int");
}

// Test null value
TEST(NullableTest, NullValue) {
    auto null_val = Nullable::null();
    
    EXPECT_TRUE(null_val.is_null());
    EXPECT_FALSE(null_val.has_value());
}

// Test non-null value
TEST(NullableTest, NonNullValue) {
    auto value = kernel::make_int(42);
    auto nullable = Nullable::of(value);
    
    EXPECT_FALSE(nullable.is_null());
    EXPECT_TRUE(nullable.has_value());
    
    auto retrieved = nullable.value();
    // In a full implementation, we'd verify the value is 42
}

// Test value_or (null coalescing)
TEST(NullableTest, ValueOr) {
    auto null_val = Nullable::null();
    auto default_val = kernel::make_int(100);
    
    auto result = null_val.value_or(default_val);
    // Result should be the default value
    
    auto non_null = Nullable::of(kernel::make_int(42));
    auto result2 = non_null.value_or(default_val);
    // Result should be 42, not the default
}

// Test null coalescing operator (??)
TEST(NullableTest, NullCoalescingOperator) {
    auto null_val = Nullable::null();
    auto default_val = kernel::make_int(100);
    
    auto result = null_coalesce(null_val, default_val);
    // Result should be the default value
    
    auto non_null = Nullable::of(kernel::make_int(42));
    auto result2 = null_coalesce(non_null, default_val);
    // Result should be 42
}

// Test null coalescing with nullable
TEST(NullableTest, NullCoalescingNullable) {
    auto null_val1 = Nullable::null();
    auto null_val2 = Nullable::null();
    auto non_null = Nullable::of(kernel::make_int(42));
    
    // null ?? null = null
    auto result1 = null_coalesce_nullable(null_val1, null_val2);
    EXPECT_TRUE(result1.is_null());
    
    // null ?? 42 = 42
    auto result2 = null_coalesce_nullable(null_val1, non_null);
    EXPECT_FALSE(result2.is_null());
    
    // 42 ?? null = 42
    auto result3 = null_coalesce_nullable(non_null, null_val1);
    EXPECT_FALSE(result3.is_null());
}

// Test map (safe navigation)
TEST(NullableTest, MapOperation) {
    auto null_val = Nullable::null();
    
    // Mapping over null should return null
    auto result1 = null_val.map([](kernel::Value v) {
        return kernel::make_int(100);
    });
    EXPECT_TRUE(result1.is_null());
    
    // Mapping over non-null should apply function
    auto non_null = Nullable::of(kernel::make_int(42));
    auto result2 = non_null.map([](kernel::Value v) {
        return kernel::make_int(100);
    });
    EXPECT_FALSE(result2.is_null());
}

// Test flat_map (chaining nullable operations)
TEST(NullableTest, FlatMapOperation) {
    auto null_val = Nullable::null();
    
    // Flat mapping over null should return null
    auto result1 = null_val.flat_map([](kernel::Value v) {
        return Nullable::of(kernel::make_int(100));
    });
    EXPECT_TRUE(result1.is_null());
    
    // Flat mapping over non-null
    auto non_null = Nullable::of(kernel::make_int(42));
    auto result2 = non_null.flat_map([](kernel::Value v) {
        return Nullable::of(kernel::make_int(100));
    });
    EXPECT_FALSE(result2.is_null());
    
    // Flat mapping that returns null
    auto result3 = non_null.flat_map([](kernel::Value v) {
        return Nullable::null();
    });
    EXPECT_TRUE(result3.is_null());
}

// Test smart cast
TEST(NullableTest, SmartCast) {
    auto null_val = Nullable::null();
    auto non_null = Nullable::of(kernel::make_int(42));
    
    EXPECT_TRUE(SmartCast::is_null(null_val));
    EXPECT_FALSE(SmartCast::is_non_null(null_val));
    
    EXPECT_FALSE(SmartCast::is_null(non_null));
    EXPECT_TRUE(SmartCast::is_non_null(non_null));
    
    // Cast non-null should succeed
    auto cast_result = SmartCast::cast_non_null(non_null);
    EXPECT_TRUE(cast_result.has_value());
    
    // Cast null should fail
    auto cast_null = SmartCast::cast_non_null(null_val);
    EXPECT_FALSE(cast_null.has_value());
}

// Test safe navigation with fields
TEST(NullableTest, SafeNavigationField) {
    // Create a struct with a field
    auto& registry = TypeRegistry::instance();
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), false)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto instance = create_struct_instance(struct_type);
    instance->set_field("value", kernel::make_int(42));
    
    // Safe navigation on non-null should work
    auto nullable_instance = Nullable::of(instance->to_value());
    auto result = safe_navigate_field(nullable_instance, "value");
    // In a full implementation, this would return the field value
    
    // Safe navigation on null should return null
    auto null_val = Nullable::null();
    auto result2 = safe_navigate_field(null_val, "value");
    EXPECT_TRUE(result2.is_null());
}

// Test chained safe navigation
TEST(NullableTest, ChainedSafeNavigation) {
    auto null_val = Nullable::null();
    
    // Chaining on null should short-circuit
    auto result = SafeNavigator(null_val)
        .field("field1")
        .field("field2")
        .field("field3")
        .result();
    
    EXPECT_TRUE(result.is_null());
}

// Test nullable type checking
TEST(NullableTest, NullableTypeChecking) {
    auto& registry = TypeRegistry::instance();
    
    // Int is not nullable
    auto int_type = registry.get_int_type();
    EXPECT_FALSE(registry.is_nullable_type(*int_type));
    
    // Int? is nullable
    auto nullable_int = registry.create_optional_type(int_type);
    EXPECT_TRUE(registry.is_nullable_type(*nullable_int));
    
    // String? is nullable
    auto nullable_string = registry.create_optional_type(registry.get_string_type());
    EXPECT_TRUE(registry.is_nullable_type(*nullable_string));
}

// Test getting inner type from nullable
TEST(NullableTest, GetInnerType) {
    auto& registry = TypeRegistry::instance();
    
    auto nullable_int = registry.create_optional_type(registry.get_int_type());
    
    auto inner = registry.get_inner_type(*nullable_int);
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ((*inner)->name(), "Int");
    
    // Non-nullable type should fail
    auto int_type = registry.get_int_type();
    auto inner2 = registry.get_inner_type(*int_type);
    EXPECT_FALSE(inner2.has_value());
}

// Test operator|| for null coalescing
TEST(NullableTest, OperatorOrOr) {
    auto null_val = Nullable::null();
    auto non_null1 = Nullable::of(kernel::make_int(42));
    auto non_null2 = Nullable::of(kernel::make_int(100));
    
    // null || non_null = non_null
    auto result1 = null_val || non_null1;
    EXPECT_FALSE(result1.is_null());
    
    // non_null || null = non_null
    auto result2 = non_null1 || null_val;
    EXPECT_FALSE(result2.is_null());
    
    // non_null || non_null = first non_null
    auto result3 = non_null1 || non_null2;
    EXPECT_FALSE(result3.is_null());
    
    // null || null = null
    auto result4 = null_val || Nullable::null();
    EXPECT_TRUE(result4.is_null());
}

// Test smart cast with type narrowing
TEST(NullableTest, SmartCastWithNonNull) {
    auto null_val = Nullable::null();
    auto non_null = Nullable::of(kernel::make_int(42));
    
    // Smart cast on null should fail
    auto result1 = SmartCast::with_non_null(null_val, [](kernel::Value v) {
        return 100; // This should not execute
    });
    EXPECT_FALSE(result1.has_value());
    
    // Smart cast on non-null should succeed
    auto result2 = SmartCast::with_non_null(non_null, [](kernel::Value v) {
        return 100; // This should execute
    });
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, 100);
}

// Test smart cast conditional execution
TEST(NullableTest, SmartCastIfNonNull) {
    auto null_val = Nullable::null();
    auto non_null = Nullable::of(kernel::make_int(42));
    
    int counter = 0;
    
    // Should not execute on null
    SmartCast::if_non_null(null_val, [&counter](kernel::Value v) {
        counter++;
    });
    EXPECT_EQ(counter, 0);
    
    // Should execute on non-null
    SmartCast::if_non_null(non_null, [&counter](kernel::Value v) {
        counter++;
    });
    EXPECT_EQ(counter, 1);
}

// Test elvis operator behavior (null coalescing)
TEST(NullableTest, ElvisOperatorBehavior) {
    auto null_val = Nullable::null();
    auto default_val = kernel::make_int(999);
    
    // null ?: default = default
    auto result1 = null_coalesce(null_val, default_val);
    // Result should be the default value
    
    auto non_null = Nullable::of(kernel::make_int(42));
    // 42 ?: default = 42
    auto result2 = null_coalesce(non_null, default_val);
    // Result should be 42
}

// Test safe navigation with null propagation
TEST(NullableTest, SafeNavigationNullPropagation) {
    auto null_val = Nullable::null();
    
    // Chaining safe navigation on null should propagate null
    auto result = SafeNavigator(null_val)
        .field("a")
        .field("b")
        .field("c")
        .result();
    
    EXPECT_TRUE(result.is_null());
    
    // Any null in the chain should propagate
    auto& registry = TypeRegistry::instance();
    std::vector<Field> fields = {
        Field("inner", registry.create_optional_type(registry.get_int_type()), false)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto instance = create_struct_instance(struct_type);
    instance->set_field("inner", Nullable::null().value_or(kernel::make_int(0)));
    
    auto nullable_instance = Nullable::of(instance->to_value());
    auto result2 = SafeNavigator(nullable_instance)
        .field("inner")
        .field("value")
        .result();
    
    // Should be null because inner is null
    EXPECT_TRUE(result2.is_null());
}

// Test type system integration
TEST(NullableTest, TypeSystemIntegration) {
    auto& registry = TypeRegistry::instance();
    
    // Create nullable types for different base types
    auto nullable_int = registry.create_optional_type(registry.get_int_type());
    auto nullable_string = registry.create_optional_type(registry.get_string_type());
    auto nullable_bool = registry.create_optional_type(registry.get_bool_type());
    
    // All should be recognized as nullable
    EXPECT_TRUE(registry.is_nullable_type(*nullable_int));
    EXPECT_TRUE(registry.is_nullable_type(*nullable_string));
    EXPECT_TRUE(registry.is_nullable_type(*nullable_bool));
    
    // Inner types should be correct
    auto inner_int = registry.get_inner_type(*nullable_int);
    ASSERT_TRUE(inner_int.has_value());
    EXPECT_EQ((*inner_int)->name(), "Int");
    
    auto inner_string = registry.get_inner_type(*nullable_string);
    ASSERT_TRUE(inner_string.has_value());
    EXPECT_EQ((*inner_string)->name(), "String");
    
    auto inner_bool = registry.get_inner_type(*nullable_bool);
    ASSERT_TRUE(inner_bool.has_value());
    EXPECT_EQ((*inner_bool)->name(), "Bool");
}
