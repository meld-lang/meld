#include <gtest/gtest.h>
#include "meld/types/refinement.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

class RefinementTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register built-in refinement types
        RefinementTypes::register_builtin_types();
    }
};

// Test basic refinement type creation
TEST_F(RefinementTypeTest, CreatePositiveInt) {
    auto positive_int = RefinementTypes::create_positive_int();
    
    ASSERT_NE(positive_int, nullptr);
    EXPECT_EQ(positive_int->name(), "positive_int");
    EXPECT_TRUE(positive_int->is_value_type());
    
    // Should be a subtype of int
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    EXPECT_TRUE(positive_int->is_subtype_of(*int_type));
}

// Test uint refinement type
TEST_F(RefinementTypeTest, CreateUint) {
    auto uint_type = RefinementTypes::create_uint();
    
    ASSERT_NE(uint_type, nullptr);
    EXPECT_EQ(uint_type->name(), "uint");
    EXPECT_TRUE(uint_type->is_value_type());
    
    // Should be a subtype of int
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    EXPECT_TRUE(uint_type->is_subtype_of(*int_type));
}

// Test non-empty string refinement type
TEST_F(RefinementTypeTest, CreateNonEmptyString) {
    auto non_empty_string = RefinementTypes::create_non_empty_string();
    
    ASSERT_NE(non_empty_string, nullptr);
    EXPECT_EQ(non_empty_string->name(), "non_empty_string");
    EXPECT_TRUE(non_empty_string->is_value_type());
    
    // Should be a subtype of string
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    EXPECT_TRUE(non_empty_string->is_subtype_of(*string_type));
}

// Test email refinement type
TEST_F(RefinementTypeTest, CreateEmail) {
    auto email_type = RefinementTypes::create_email();
    
    ASSERT_NE(email_type, nullptr);
    EXPECT_EQ(email_type->name(), "email");
    EXPECT_TRUE(email_type->is_value_type());
    
    // Should be a subtype of string
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    EXPECT_TRUE(email_type->is_subtype_of(*string_type));
}

// Test ValidChar refinement type
TEST_F(RefinementTypeTest, CreateValidChar) {
    auto valid_char = RefinementTypes::create_valid_char();
    
    ASSERT_NE(valid_char, nullptr);
    EXPECT_EQ(valid_char->name(), "valid_char");
    EXPECT_TRUE(valid_char->is_value_type());
    
    // Should be a subtype of int (char is stored as int code point)
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    EXPECT_TRUE(valid_char->is_subtype_of(*int_type));
}

// Test positive int validation
TEST_F(RefinementTypeTest, ValidatePositiveInt) {
    // Valid positive integers
    EXPECT_TRUE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(1))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(42))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(1000))).value_or(false));
    
    // Invalid values
    EXPECT_FALSE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(0))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(-1))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_positive_int(Value(std::make_shared<Integer>(-42))).value_or(true));
    
    // Wrong type should return error
    auto result = RefinementTypes::validate_positive_int(Value(std::make_shared<String>("not an int")));
    EXPECT_FALSE(result.has_value());
}

// Test uint validation
TEST_F(RefinementTypeTest, ValidateUint) {
    // Valid unsigned integers
    EXPECT_TRUE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(0))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(1))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(42))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(1000))).value_or(false));
    
    // Invalid values
    EXPECT_FALSE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(-1))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_uint(Value(std::make_shared<Integer>(-42))).value_or(true));
    
    // Wrong type should return error
    auto result = RefinementTypes::validate_uint(Value(std::make_shared<String>("not an int")));
    EXPECT_FALSE(result.has_value());
}

// Test non-empty string validation
TEST_F(RefinementTypeTest, ValidateNonEmptyString) {
    // Valid non-empty strings
    EXPECT_TRUE(RefinementTypes::validate_non_empty_string(Value(std::make_shared<String>("hello"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_non_empty_string(Value(std::make_shared<String>("a"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_non_empty_string(Value(std::make_shared<String>("   "))).value_or(false)); // whitespace is not empty
    
    // Invalid values
    EXPECT_FALSE(RefinementTypes::validate_non_empty_string(Value(std::make_shared<String>(""))).value_or(true));
    
    // Wrong type should return error
    auto result = RefinementTypes::validate_non_empty_string(Value(std::make_shared<Integer>(42)));
    EXPECT_FALSE(result.has_value());
}

// Test email validation
TEST_F(RefinementTypeTest, ValidateEmail) {
    // Valid emails
    EXPECT_TRUE(RefinementTypes::validate_email(Value(std::make_shared<String>("test@example.com"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_email(Value(std::make_shared<String>("user.name@domain.org"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_email(Value(std::make_shared<String>("alice+tag@company.co.uk"))).value_or(false));
    
    // Invalid emails
    EXPECT_FALSE(RefinementTypes::validate_email(Value(std::make_shared<String>("not-an-email"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_email(Value(std::make_shared<String>("@example.com"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_email(Value(std::make_shared<String>("test@"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_email(Value(std::make_shared<String>("test@.com"))).value_or(true));
    
    // Wrong type should return error
    auto result = RefinementTypes::validate_email(Value(std::make_shared<Integer>(42)));
    EXPECT_FALSE(result.has_value());
}

// Test char validation
TEST_F(RefinementTypeTest, ValidateChar) {
    // Valid Unicode code points
    EXPECT_TRUE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0))).value_or(false));      // NULL
    EXPECT_TRUE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(65))).value_or(false));     // 'A'
    EXPECT_TRUE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0x1F600))).value_or(false)); // 😀 emoji
    EXPECT_TRUE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0x10FFFF))).value_or(false)); // Max valid code point
    
    // Invalid code points
    EXPECT_FALSE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(-1))).value_or(true));      // Negative
    EXPECT_FALSE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0x110000))).value_or(true)); // Too large
    EXPECT_FALSE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0xD800))).value_or(true));  // Surrogate
    EXPECT_FALSE(RefinementTypes::validate_char(Value(std::make_shared<Integer>(0xDFFF))).value_or(true));  // Surrogate
    
    // Wrong type should return error
    auto result = RefinementTypes::validate_char(Value(std::make_shared<String>("not an int")));
    EXPECT_FALSE(result.has_value());
}

// Test refinement type registration
TEST_F(RefinementTypeTest, BuiltinTypesRegistered) {
    auto& registry = TypeRegistry::instance();
    
    // Check that original built-in refinement types are registered
    EXPECT_TRUE(registry.get_type("positive_int").has_value());
    EXPECT_TRUE(registry.get_type("non_empty_string").has_value());
    EXPECT_TRUE(registry.get_type("email").has_value());
    EXPECT_TRUE(registry.get_type("uint").has_value());
    EXPECT_TRUE(registry.get_type("valid_char").has_value());
    
    // Check that new built-in refinement types are registered
    EXPECT_TRUE(registry.get_type("negative_int").has_value());
    EXPECT_TRUE(registry.get_type("non_zero_int").has_value());
    EXPECT_TRUE(registry.get_type("even_int").has_value());
    EXPECT_TRUE(registry.get_type("odd_int").has_value());
    EXPECT_TRUE(registry.get_type("percentage").has_value());
    EXPECT_TRUE(registry.get_type("normalized_float").has_value());
    EXPECT_TRUE(registry.get_type("positive_float").has_value());
    EXPECT_TRUE(registry.get_type("url").has_value());
    EXPECT_TRUE(registry.get_type("phone_number").has_value());
    EXPECT_TRUE(registry.get_type("alphanumeric_string").has_value());
    EXPECT_TRUE(registry.get_type("uppercase_string").has_value());
    EXPECT_TRUE(registry.get_type("lowercase_string").has_value());
    EXPECT_TRUE(registry.get_type("trimmed_string").has_value());
    EXPECT_TRUE(registry.get_type("hex_string").has_value());
    EXPECT_TRUE(registry.get_type("base64_string").has_value());
}

// Test refinement type factory method
TEST_F(RefinementTypeTest, CreateRefinementType) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a simple predicate function
    auto predicate = Value(std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(Empty::instance()),
        Function::NativeImpl([](const std::vector<Value>& args) -> Value {
        if (args.size() != 1 || !args[0].is<Integer>()) {
            return Value(Boolean::false_value());
        }
        return Value(args[0].as<Integer>()->value() % 2 == 0 ? Boolean::true_value() : Boolean::false_value());
    })));
    
    auto even_int = MetaType::create_refinement("even_int", int_type, predicate);
    
    ASSERT_NE(even_int, nullptr);
    EXPECT_EQ(even_int->name(), "even_int");
    EXPECT_TRUE(even_int->is_subtype_of(*int_type));
    
    // Test validation
    auto refinement = std::dynamic_pointer_cast<RefinementMetaType>(even_int);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_TRUE(refinement->validate_value(Value(std::make_shared<Integer>(2))).value_or(false));
    EXPECT_TRUE(refinement->validate_value(Value(std::make_shared<Integer>(42))).value_or(false));
    EXPECT_FALSE(refinement->validate_value(Value(std::make_shared<Integer>(1))).value_or(true));
    EXPECT_FALSE(refinement->validate_value(Value(std::make_shared<Integer>(43))).value_or(true));
}

// Test refinement type from_value method
TEST_F(RefinementTypeTest, FromValue) {
    auto uint_type = RefinementTypes::create_uint();
    
    // Valid values should pass through
    auto result1 = uint_type->from_value(Value(std::make_shared<Integer>(42)));
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1->as<Integer>()->value(), 42);
    
    auto result2 = uint_type->from_value(Value(std::make_shared<Integer>(0)));
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2->as<Integer>()->value(), 0);
    
    // Invalid values should be rejected
    auto result3 = uint_type->from_value(Value(std::make_shared<Integer>(-1)));
    EXPECT_FALSE(result3.has_value());
    EXPECT_TRUE(result3.error().find("refinement constraint") != std::string::npos);
}

// Test type compatibility
TEST_F(RefinementTypeTest, TypeCompatibility) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    auto uint_type = RefinementTypes::create_uint();
    auto positive_int = RefinementTypes::create_positive_int();
    
    // Refinement types should be assignable from their base type
    EXPECT_TRUE(uint_type->is_assignable_from(*int_type));
    EXPECT_TRUE(positive_int->is_assignable_from(*int_type));
    
    // Base type should not be assignable from refinement type (would be unsafe)
    // Actually, this depends on the implementation - in some systems it might be allowed
    // For now, let's test that refinement types are subtypes of their base
    EXPECT_TRUE(uint_type->is_subtype_of(*int_type));
    EXPECT_TRUE(positive_int->is_subtype_of(*int_type));
    
    // Refinement types with the same base should be compatible
    EXPECT_TRUE(uint_type->is_assignable_from(*positive_int));
    EXPECT_TRUE(positive_int->is_assignable_from(*uint_type));
}

// Test additional integer refinement types
TEST_F(RefinementTypeTest, AdditionalIntegerTypes) {
    // Test NegativeInt
    EXPECT_TRUE(RefinementTypes::validate_negative_int(Value(std::make_shared<Integer>(-1))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_negative_int(Value(std::make_shared<Integer>(-100))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_negative_int(Value(std::make_shared<Integer>(0))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_negative_int(Value(std::make_shared<Integer>(1))).value_or(true));
    
    // Test NonZeroInt
    EXPECT_TRUE(RefinementTypes::validate_non_zero_int(Value(std::make_shared<Integer>(1))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_non_zero_int(Value(std::make_shared<Integer>(-1))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_non_zero_int(Value(std::make_shared<Integer>(0))).value_or(true));
    
    // Test EvenInt
    EXPECT_TRUE(RefinementTypes::validate_even_int(Value(std::make_shared<Integer>(0))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_even_int(Value(std::make_shared<Integer>(2))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_even_int(Value(std::make_shared<Integer>(-4))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_even_int(Value(std::make_shared<Integer>(1))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_even_int(Value(std::make_shared<Integer>(-3))).value_or(true));
}

// Test float refinement types
TEST_F(RefinementTypeTest, FloatRefinementTypes) {
    // Test Percentage (0.0 to 100.0)
    EXPECT_TRUE(RefinementTypes::validate_percentage(Value(std::make_shared<Float>(0.0))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_percentage(Value(std::make_shared<Float>(50.5))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_percentage(Value(std::make_shared<Float>(100.0))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_percentage(Value(std::make_shared<Float>(-0.1))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_percentage(Value(std::make_shared<Float>(100.1))).value_or(true));
    
    // Test NormalizedFloat (0.0 to 1.0)
    EXPECT_TRUE(RefinementTypes::validate_normalized_float(Value(std::make_shared<Float>(0.0))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_normalized_float(Value(std::make_shared<Float>(0.5))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_normalized_float(Value(std::make_shared<Float>(1.0))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_normalized_float(Value(std::make_shared<Float>(-0.1))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_normalized_float(Value(std::make_shared<Float>(1.1))).value_or(true));
    
    // Test PositiveFloat
    EXPECT_TRUE(RefinementTypes::validate_positive_float(Value(std::make_shared<Float>(0.1))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_positive_float(Value(std::make_shared<Float>(3.14))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_positive_float(Value(std::make_shared<Float>(0.0))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_positive_float(Value(std::make_shared<Float>(-1.0))).value_or(true));
}

// Test string refinement types
TEST_F(RefinementTypeTest, StringRefinementTypes) {
    // Test URL
    EXPECT_TRUE(RefinementTypes::validate_url(Value(std::make_shared<String>("https://example.com"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_url(Value(std::make_shared<String>("http://test.org/path"))).value_or(false));
    EXPECT_FALSE(RefinementTypes::validate_url(Value(std::make_shared<String>("ftp://example.com"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_url(Value(std::make_shared<String>("not-a-url"))).value_or(true));
    
    // Test AlphanumericString
    EXPECT_TRUE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>("abc123"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>("ABC"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>("123"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>(""))).value_or(false)); // empty is valid
    EXPECT_FALSE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>("abc-123"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_alphanumeric_string(Value(std::make_shared<String>("hello world"))).value_or(true));
    
    // Test UppercaseString
    EXPECT_TRUE(RefinementTypes::validate_uppercase_string(Value(std::make_shared<String>("HELLO"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_uppercase_string(Value(std::make_shared<String>("ABC123"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_uppercase_string(Value(std::make_shared<String>("123"))).value_or(false)); // no letters is valid
    EXPECT_FALSE(RefinementTypes::validate_uppercase_string(Value(std::make_shared<String>("Hello"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_uppercase_string(Value(std::make_shared<String>("hello"))).value_or(true));
    
    // Test LowercaseString
    EXPECT_TRUE(RefinementTypes::validate_lowercase_string(Value(std::make_shared<String>("hello"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_lowercase_string(Value(std::make_shared<String>("abc123"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_lowercase_string(Value(std::make_shared<String>("123"))).value_or(false)); // no letters is valid
    EXPECT_FALSE(RefinementTypes::validate_lowercase_string(Value(std::make_shared<String>("Hello"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_lowercase_string(Value(std::make_shared<String>("HELLO"))).value_or(true));
    
    // Test TrimmedString
    EXPECT_TRUE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>("hello"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>("hello world"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>(""))).value_or(false)); // empty is trimmed
    EXPECT_FALSE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>(" hello"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>("hello "))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_trimmed_string(Value(std::make_shared<String>(" hello "))).value_or(true));
    
    // Test HexString
    EXPECT_TRUE(RefinementTypes::validate_hex_string(Value(std::make_shared<String>("deadbeef"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_hex_string(Value(std::make_shared<String>("123ABC"))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_hex_string(Value(std::make_shared<String>(""))).value_or(false)); // empty is valid
    EXPECT_FALSE(RefinementTypes::validate_hex_string(Value(std::make_shared<String>("xyz123"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_hex_string(Value(std::make_shared<String>("hello"))).value_or(true));
    
    // Test Base64String
    EXPECT_TRUE(RefinementTypes::validate_base64_string(Value(std::make_shared<String>("SGVsbG8="))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_base64_string(Value(std::make_shared<String>("YWJjZA=="))).value_or(false));
    EXPECT_TRUE(RefinementTypes::validate_base64_string(Value(std::make_shared<String>(""))).value_or(false)); // empty is valid
    EXPECT_FALSE(RefinementTypes::validate_base64_string(Value(std::make_shared<String>("invalid@base64"))).value_or(true));
    EXPECT_FALSE(RefinementTypes::validate_base64_string(Value(std::make_shared<String>("SGVsbG8"))).value_or(true)); // wrong length
}