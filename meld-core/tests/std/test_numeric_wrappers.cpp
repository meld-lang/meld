#include <gtest/gtest.h>
#include "meld/std/numeric_wrappers.hpp"
#include "meld/macro/literal_suffixes.hpp"
#include "meld/types/refinement.hpp"
#include "meld/types/instance.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::stdx;
using namespace meld::types;
using namespace meld::kernel;

class NumericWrappersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register built-in types
        RefinementTypes::register_builtin_types();
        NumericWrappers::register_all_types();
    }
};

// Test 22.5.1: Refinement types for unsigned integers
TEST_F(NumericWrappersTest, UintRefinementTypeValidation) {
    // Test valid uint values
    auto valid_result = NumericWrappers::validate_uint(42);
    ASSERT_TRUE(valid_result.is_ok());
    EXPECT_TRUE(valid_result.unwrap().is<Integer>());
    EXPECT_EQ(valid_result.unwrap().as<Integer>()->value(), 42);
    
    // Test zero (boundary case)
    auto zero_result = NumericWrappers::validate_uint(0);
    ASSERT_TRUE(zero_result.is_ok());
    EXPECT_EQ(zero_result.unwrap().as<Integer>()->value(), 0);
    
    // Test negative values (should fail)
    auto negative_result = NumericWrappers::validate_uint(-1);
    ASSERT_TRUE(negative_result.is_err());
    EXPECT_EQ(negative_result.unwrap_err().type, ValidationErrorType::CONSTRAINT_VIOLATION);
    
    // Test with Value object
    Value positive_value(std::make_shared<Integer>(100));
    auto value_result = NumericWrappers::validate_uint(positive_value);
    ASSERT_TRUE(value_result.is_ok());
    
    Value negative_value(std::make_shared<Integer>(-50));
    auto negative_value_result = NumericWrappers::validate_uint(negative_value);
    ASSERT_TRUE(negative_value_result.is_err());
}

TEST_F(NumericWrappersTest, UintRefinementTypeCreation) {
    auto uint_type = NumericWrappers::create_uint_refinement();
    ASSERT_NE(uint_type, nullptr);
    EXPECT_EQ(uint_type->name(), "uint");
    
    // Verify it's based on int type
    auto& registry = meta::TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    EXPECT_EQ(uint_type->base_type(), int_type);
}

// Test compile-time validation (static analysis would be done by compiler)
TEST_F(NumericWrappersTest, CompileTimeValidation) {
    // These would be caught at compile-time in a full implementation
    // For now, we test the runtime validation logic
    
    // Statically determinable positive value
    auto static_positive = NumericWrappers::validate_uint(123);
    EXPECT_TRUE(static_positive.is_ok());
    
    // Statically determinable negative value
    auto static_negative = NumericWrappers::validate_uint(-456);
    EXPECT_TRUE(static_negative.is_err());
    EXPECT_EQ(static_negative.unwrap_err().type, ValidationErrorType::CONSTRAINT_VIOLATION);
}

TEST_F(NumericWrappersTest, ErrorMessages) {
    auto result = NumericWrappers::validate_uint(-42);
    ASSERT_TRUE(result.is_err());
    
    auto error = result.unwrap_err();
    EXPECT_EQ(error.type, ValidationErrorType::CONSTRAINT_VIOLATION);
    EXPECT_FALSE(error.message.empty());
    EXPECT_EQ(error.expected_type, "uint (>= 0)");
    EXPECT_EQ(error.actual_value, "-42");
}

// Test type mismatch errors
TEST_F(NumericWrappersTest, TypeMismatchValidation) {
    Value string_value(std::make_shared<String>("not a number"));
    auto result = NumericWrappers::validate_uint(string_value);
    
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().type, ValidationErrorType::TYPE_MISMATCH);
}

// Test 22.5.2: Bit-width integer wrappers
TEST_F(NumericWrappersTest, U8StructCreation) {
    auto u8_type = NumericWrappers::create_u8_struct();
    ASSERT_NE(u8_type, nullptr);
    EXPECT_EQ(u8_type->name(), "u8");
    
    // Check @transpile_as annotations
    auto annotations = u8_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
    
    // Check field structure
    auto fields = u8_type->fields();
    EXPECT_EQ(fields.size(), 1);
    EXPECT_EQ(fields[0].name, "bits");
}

TEST_F(NumericWrappersTest, U16StructCreation) {
    auto u16_type = NumericWrappers::create_u16_struct();
    ASSERT_NE(u16_type, nullptr);
    EXPECT_EQ(u16_type->name(), "u16");
    
    auto annotations = u16_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, U32StructCreation) {
    auto u32_type = NumericWrappers::create_u32_struct();
    ASSERT_NE(u32_type, nullptr);
    EXPECT_EQ(u32_type->name(), "u32");
    
    auto annotations = u32_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, U64StructCreation) {
    auto u64_type = NumericWrappers::create_u64_struct();
    ASSERT_NE(u64_type, nullptr);
    EXPECT_EQ(u64_type->name(), "u64");
    
    auto annotations = u64_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, I8StructCreation) {
    auto i8_type = NumericWrappers::create_i8_struct();
    ASSERT_NE(i8_type, nullptr);
    EXPECT_EQ(i8_type->name(), "i8");
    
    auto annotations = i8_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, I16StructCreation) {
    auto i16_type = NumericWrappers::create_i16_struct();
    ASSERT_NE(i16_type, nullptr);
    EXPECT_EQ(i16_type->name(), "i16");
    
    auto annotations = i16_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, ValueCreationU8) {
    // Valid u8 values
    auto valid_u8 = NumericWrappers::create_u8(255);
    EXPECT_TRUE(valid_u8.is<StructInstance>());
    
    auto zero_u8 = NumericWrappers::create_u8(0);
    EXPECT_TRUE(zero_u8.is<StructInstance>());
    
    // Invalid u8 values (out of range)
    auto invalid_u8_high = NumericWrappers::create_u8(256);
    EXPECT_TRUE(invalid_u8_high.is<String>()); // Error message
    
    auto invalid_u8_low = NumericWrappers::create_u8(-1);
    EXPECT_TRUE(invalid_u8_low.is<String>()); // Error message
}

TEST_F(NumericWrappersTest, ValueCreationU16) {
    // Valid u16 values
    auto valid_u16 = NumericWrappers::create_u16(65535);
    EXPECT_TRUE(valid_u16.is<StructInstance>());
    
    // Invalid u16 values
    auto invalid_u16 = NumericWrappers::create_u16(65536);
    EXPECT_TRUE(invalid_u16.is<String>()); // Error message
}

TEST_F(NumericWrappersTest, ValueCreationI8) {
    // Valid i8 values
    auto valid_i8_pos = NumericWrappers::create_i8(127);
    EXPECT_TRUE(valid_i8_pos.is<StructInstance>());
    
    auto valid_i8_neg = NumericWrappers::create_i8(-128);
    EXPECT_TRUE(valid_i8_neg.is<StructInstance>());
    
    // Invalid i8 values
    auto invalid_i8_high = NumericWrappers::create_i8(128);
    EXPECT_TRUE(invalid_i8_high.is<String>()); // Error message
    
    auto invalid_i8_low = NumericWrappers::create_i8(-129);
    EXPECT_TRUE(invalid_i8_low.is<String>()); // Error message
}

TEST_F(NumericWrappersTest, ValueCreationI16) {
    // Valid i16 values
    auto valid_i16_pos = NumericWrappers::create_i16(32767);
    EXPECT_TRUE(valid_i16_pos.is<StructInstance>());
    
    auto valid_i16_neg = NumericWrappers::create_i16(-32768);
    EXPECT_TRUE(valid_i16_neg.is<StructInstance>());
    
    // Invalid i16 values
    auto invalid_i16_high = NumericWrappers::create_i16(32768);
    EXPECT_TRUE(invalid_i16_high.is<String>()); // Error message
    
    auto invalid_i16_low = NumericWrappers::create_i16(-32769);
    EXPECT_TRUE(invalid_i16_low.is<String>()); // Error message
}

// Test 22.5.3: Bitwise operators for unsigned types
TEST_F(NumericWrappersTest, LogicalRightShift) {
    Value value(std::make_shared<Integer>(-1)); // All bits set
    auto result = NumericWrappers::logical_right_shift(value, 1);
    
    ASSERT_TRUE(result.is<Integer>());
    // Logical right shift should treat as unsigned
    uint64_t expected = static_cast<uint64_t>(-1) >> 1;
    EXPECT_EQ(result.as<Integer>()->value(), static_cast<int64_t>(expected));
}

TEST_F(NumericWrappersTest, BitwiseAnd) {
    Value left(std::make_shared<Integer>(0xFF));
    Value right(std::make_shared<Integer>(0x0F));
    
    auto result = NumericWrappers::bitwise_and(left, right);
    ASSERT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0x0F);
}

TEST_F(NumericWrappersTest, BitwiseOr) {
    Value left(std::make_shared<Integer>(0xF0));
    Value right(std::make_shared<Integer>(0x0F));
    
    auto result = NumericWrappers::bitwise_or(left, right);
    ASSERT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0xFF);
}

TEST_F(NumericWrappersTest, BitwiseXor) {
    Value left(std::make_shared<Integer>(0xFF));
    Value right(std::make_shared<Integer>(0x0F));
    
    auto result = NumericWrappers::bitwise_xor(left, right);
    ASSERT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 0xF0);
}

TEST_F(NumericWrappersTest, BitwiseNot) {
    Value value(std::make_shared<Integer>(0));
    
    auto result = NumericWrappers::bitwise_not(value);
    ASSERT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), ~0);
}

TEST_F(NumericWrappersTest, BitwiseOperatorErrorHandling) {
    Value int_value(std::make_shared<Integer>(42));
    Value string_value(std::make_shared<String>("not a number"));
    
    // Test error handling for non-integer inputs
    auto and_result = NumericWrappers::bitwise_and(int_value, string_value);
    EXPECT_TRUE(and_result.is<String>()); // Error message
    
    auto or_result = NumericWrappers::bitwise_or(string_value, int_value);
    EXPECT_TRUE(or_result.is<String>()); // Error message
    
    auto xor_result = NumericWrappers::bitwise_xor(string_value, string_value);
    EXPECT_TRUE(xor_result.is<String>()); // Error message
    
    auto not_result = NumericWrappers::bitwise_not(string_value);
    EXPECT_TRUE(not_result.is<String>()); // Error message
    
    auto shift_result = NumericWrappers::logical_right_shift(string_value, 1);
    EXPECT_TRUE(shift_result.is<String>()); // Error message
}

// Test 22.5.4: Literal suffix macros
TEST_F(NumericWrappersTest, LiteralSuffixDetection) {
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_u8_suffix("255u8"));
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_u16_suffix("1000u16"));
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_u32_suffix("0xDEADBEEFu32"));
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_u64_suffix("123456789u64"));
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_i8_suffix("100i8"));
    EXPECT_TRUE(meld::macro::LiteralSuffixMacros::has_i16_suffix("30000i16"));
    
    // Negative cases
    EXPECT_FALSE(meld::macro::LiteralSuffixMacros::has_u8_suffix("255"));
    EXPECT_FALSE(meld::macro::LiteralSuffixMacros::has_u8_suffix("255u16"));
    EXPECT_FALSE(meld::macro::LiteralSuffixMacros::has_u16_suffix("255u8"));
}

TEST_F(NumericWrappersTest, LiteralSuffixExpansion) {
    // Test u8 literal expansion
    auto u8_result = meld::macro::LiteralSuffixMacros::expand_u8_literal("255u8");
    EXPECT_TRUE(u8_result.is<StructInstance>());
    
    // Test u16 literal expansion
    auto u16_result = meld::macro::LiteralSuffixMacros::expand_u16_literal("1000u16");
    EXPECT_TRUE(u16_result.is<StructInstance>());
    
    // Test i8 literal expansion
    auto i8_result = meld::macro::LiteralSuffixMacros::expand_i8_literal("100i8");
    EXPECT_TRUE(i8_result.is<StructInstance>());
    
    // Test i16 literal expansion
    auto i16_result = meld::macro::LiteralSuffixMacros::expand_i16_literal("30000i16");
    EXPECT_TRUE(i16_result.is<StructInstance>());
}

TEST_F(NumericWrappersTest, HexadecimalLiteralSuffixes) {
    // Test hexadecimal literals with suffixes
    auto hex_u32 = meld::macro::LiteralSuffixMacros::expand_u32_literal("0xDEADBEEFu32");
    EXPECT_TRUE(hex_u32.is<StructInstance>());
    
    // Test with underscores
    auto hex_with_underscores = meld::macro::LiteralSuffixMacros::expand_u32_literal("0xDEAD_BEEF_u32");
    EXPECT_TRUE(hex_with_underscores.is<StructInstance>());
}

TEST_F(NumericWrappersTest, LiteralSuffixRangeValidation) {
    // Test out-of-range values
    auto out_of_range_u8 = meld::macro::LiteralSuffixMacros::expand_u8_literal("256u8");
    EXPECT_TRUE(out_of_range_u8.is<String>()); // Error message
    
    auto out_of_range_i8_high = meld::macro::LiteralSuffixMacros::expand_i8_literal("128i8");
    EXPECT_TRUE(out_of_range_i8_high.is<String>()); // Error message
    
    auto out_of_range_i8_low = meld::macro::LiteralSuffixMacros::expand_i8_literal("-129i8");
    EXPECT_TRUE(out_of_range_i8_low.is<String>()); // Error message
}

TEST_F(NumericWrappersTest, NumericLiteralParsing) {
    // Test different number formats
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("255"), 255);
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("0xFF"), 255);
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("0b11111111"), 255);
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("0377"), 255);
    
    // Test with underscores
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("1_000_000"), 1000000);
    EXPECT_EQ(meld::macro::LiteralSuffixMacros::parse_numeric_literal("0xFF_FF"), 65535);
}

// Test 22.5.5: char as Unicode scalar value
TEST_F(NumericWrappersTest, CharStructCreation) {
    auto char_type = NumericWrappers::create_char_struct();
    ASSERT_NE(char_type, nullptr);
    EXPECT_EQ(char_type->name(), "char");
    
    // Check @transpile_as annotations
    auto annotations = char_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
}

TEST_F(NumericWrappersTest, ValidCharRefinementType) {
    auto valid_char_type = NumericWrappers::create_valid_char_refinement();
    ASSERT_NE(valid_char_type, nullptr);
    EXPECT_EQ(valid_char_type->name(), "valid_char");
}

TEST_F(NumericWrappersTest, CharCreationFromCodePoint) {
    // Valid Unicode code points
    auto ascii_char = NumericWrappers::create_char(65); // 'A'
    EXPECT_TRUE(ascii_char.is<StructInstance>());
    
    auto emoji_char = NumericWrappers::create_char(0x1F680); // 🚀
    EXPECT_TRUE(emoji_char.is<StructInstance>());
    
    // Invalid code points
    auto invalid_high = NumericWrappers::create_char(0x110000); // Too high
    EXPECT_TRUE(invalid_high.is<String>()); // Error message
    
    auto invalid_surrogate = NumericWrappers::create_char(0xD800); // Surrogate
    EXPECT_TRUE(invalid_surrogate.is<String>()); // Error message
    
    auto invalid_negative = NumericWrappers::create_char(-1);
    EXPECT_TRUE(invalid_negative.is<String>()); // Error message
}

TEST_F(NumericWrappersTest, CharCreationFromAscii) {
    auto char_a = NumericWrappers::create_char('A');
    EXPECT_TRUE(char_a.is<StructInstance>());
    
    auto char_space = NumericWrappers::create_char(' ');
    EXPECT_TRUE(char_space.is<StructInstance>());
    
    auto char_newline = NumericWrappers::create_char('\n');
    EXPECT_TRUE(char_newline.is<StructInstance>());
}

TEST_F(NumericWrappersTest, CharUtilityMethods) {
    auto digit_char = NumericWrappers::create_char('5');
    auto alpha_char = NumericWrappers::create_char('A');
    auto lower_char = NumericWrappers::create_char('a');
    auto space_char = NumericWrappers::create_char(' ');
    
    // Test is_digit
    EXPECT_TRUE(NumericWrappers::is_digit(digit_char));
    EXPECT_FALSE(NumericWrappers::is_digit(alpha_char));
    
    // Test is_alpha
    EXPECT_TRUE(NumericWrappers::is_alpha(alpha_char));
    EXPECT_TRUE(NumericWrappers::is_alpha(lower_char));
    EXPECT_FALSE(NumericWrappers::is_alpha(digit_char));
    
    // Test is_alphanumeric
    EXPECT_TRUE(NumericWrappers::is_alphanumeric(digit_char));
    EXPECT_TRUE(NumericWrappers::is_alphanumeric(alpha_char));
    EXPECT_FALSE(NumericWrappers::is_alphanumeric(space_char));
    
    // Test is_whitespace
    EXPECT_TRUE(NumericWrappers::is_whitespace(space_char));
    EXPECT_FALSE(NumericWrappers::is_whitespace(alpha_char));
}

TEST_F(NumericWrappersTest, CharCaseConversion) {
    auto lower_a = NumericWrappers::create_char('a');
    auto upper_a = NumericWrappers::create_char('A');
    
    // Test to_upper
    auto upper_result = NumericWrappers::to_upper(lower_a);
    EXPECT_TRUE(upper_result.is<StructInstance>());
    
    // Test to_lower
    auto lower_result = NumericWrappers::to_lower(upper_a);
    EXPECT_TRUE(lower_result.is<StructInstance>());
    
    // Test with non-alphabetic characters (should remain unchanged)
    auto digit_char = NumericWrappers::create_char('5');
    auto upper_digit = NumericWrappers::to_upper(digit_char);
    EXPECT_TRUE(upper_digit.is<StructInstance>());
    
    auto lower_digit = NumericWrappers::to_lower(digit_char);
    EXPECT_TRUE(lower_digit.is<StructInstance>());
}

TEST_F(NumericWrappersTest, CharLiteralSyntax) {
    // Test single quote syntax support (would be handled by parser)
    // For now, we test the underlying creation functions
    
    auto char_A = NumericWrappers::create_char('A');
    EXPECT_TRUE(char_A.is<StructInstance>());
    
    auto char_emoji = NumericWrappers::create_char(0x1F680); // 🚀
    EXPECT_TRUE(char_emoji.is<StructInstance>());
}

// Test 22.5.6: Buffer type for packed arrays
TEST_F(NumericWrappersTest, BufferStructCreation) {
    auto buffer_type = NumericWrappers::create_buffer_struct();
    ASSERT_NE(buffer_type, nullptr);
    EXPECT_EQ(buffer_type->name(), "buffer");
    
    // Check @transpile_as annotations for native byte arrays
    auto annotations = buffer_type->get_annotations();
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    EXPECT_TRUE(annotations.find("value") != annotations.end());
    
    // Check field structure
    auto fields = buffer_type->fields();
    EXPECT_EQ(fields.size(), 1);
    EXPECT_EQ(fields[0].name, "data");
}

TEST_F(NumericWrappersTest, BufferTranspilerMappings) {
    auto buffer_type = NumericWrappers::create_buffer_struct();
    auto annotations = buffer_type->get_annotations();
    
    // Verify transpiler mappings exist
    EXPECT_TRUE(annotations.find("transpile_as") != annotations.end());
    
    // In a full implementation, we would check:
    // - C++: std::vector<uint8_t>
    // - Java: byte[]
    // - Go: []byte
}

TEST_F(NumericWrappersTest, BufferEfficientMemoryLayout) {
    // Buffer should use vec primitive with packing mode
    // This ensures no 64-bit overhead per element
    auto buffer_type = NumericWrappers::create_buffer_struct();
    
    // Verify it uses vec primitive (would be checked in full implementation)
    auto fields = buffer_type->fields();
    EXPECT_EQ(fields.size(), 1);
    EXPECT_EQ(fields[0].name, "data");
    
    // The field should be based on the vec primitive
    // In a full implementation, we would verify:
    // - Field type is vec primitive
    // - Packing mode is enabled for efficient storage
}