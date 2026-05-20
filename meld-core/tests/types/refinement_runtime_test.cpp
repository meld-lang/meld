#include <gtest/gtest.h>
#include "meld/types/refinement_runtime.hpp"
#include "meld/types/refinement.hpp"

using namespace meld::types;
using namespace meld::types::refinement_runtime;
using namespace meld::kernel;
using namespace meld::meta;

class RefinementRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        RefinementTypes::register_builtin_types();
    }
};

// Test runtime validation of positive integers
TEST_F(RefinementRuntimeTest, ValidatePositiveInt) {
    // Valid positive integer
    auto result1 = create_positive_int(42);
    EXPECT_TRUE(result1.is_success());
    EXPECT_TRUE(result1.value().is<Integer>());
    EXPECT_EQ(result1.value().as<Integer>()->value(), 42);
    
    // Valid positive integer from Value
    auto result2 = create_positive_int(Value(std::make_shared<Integer>(100)));
    EXPECT_TRUE(result2.is_success());
    EXPECT_EQ(result2.value().as<Integer>()->value(), 100);
    
    // Invalid zero
    auto result3 = create_positive_int(0);
    EXPECT_TRUE(result3.is_error());
    EXPECT_EQ(result3.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
    EXPECT_TRUE(result3.error().message.find("constraint") != std::string::npos);
    
    // Invalid negative
    auto result4 = create_positive_int(-5);
    EXPECT_TRUE(result4.is_error());
    EXPECT_EQ(result4.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
}

// Test runtime validation of uint
TEST_F(RefinementRuntimeTest, ValidateUint) {
    // Valid unsigned integers
    auto result1 = create_uint(0);
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value().as<Integer>()->value(), 0);
    
    auto result2 = create_uint(42);
    EXPECT_TRUE(result2.is_success());
    EXPECT_EQ(result2.value().as<Integer>()->value(), 42);
    
    // Invalid negative
    auto result3 = create_uint(-1);
    EXPECT_TRUE(result3.is_error());
    EXPECT_EQ(result3.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
}

// Test runtime validation of non-empty strings
TEST_F(RefinementRuntimeTest, ValidateNonEmptyString) {
    // Valid non-empty string
    auto result1 = create_non_empty_string("hello");
    EXPECT_TRUE(result1.is_success());
    EXPECT_TRUE(result1.value().is<String>());
    EXPECT_EQ(result1.value().as<String>()->value(), "hello");
    
    // Valid non-empty string from Value
    auto result2 = create_non_empty_string(Value(std::make_shared<String>("world")));
    EXPECT_TRUE(result2.is_success());
    EXPECT_EQ(result2.value().as<String>()->value(), "world");
    
    // Invalid empty string
    auto result3 = create_non_empty_string("");
    EXPECT_TRUE(result3.is_error());
    EXPECT_EQ(result3.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
}

// Test runtime validation of email addresses
TEST_F(RefinementRuntimeTest, ValidateEmail) {
    // Valid emails
    auto result1 = create_email("test@example.com");
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value().as<String>()->value(), "test@example.com");
    
    auto result2 = create_email("user.name@domain.org");
    EXPECT_TRUE(result2.is_success());
    
    // Invalid emails
    auto result3 = create_email("not-an-email");
    EXPECT_TRUE(result3.is_error());
    EXPECT_EQ(result3.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
    
    auto result4 = create_email("@example.com");
    EXPECT_TRUE(result4.is_error());
    
    auto result5 = create_email("test@");
    EXPECT_TRUE(result5.is_error());
}

// Test runtime validation of Unicode characters
TEST_F(RefinementRuntimeTest, ValidateChar) {
    // Valid Unicode code points
    auto result1 = create_valid_char(65); // 'A'
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value().as<Integer>()->value(), 65);
    
    auto result2 = create_valid_char(0x1F600); // 😀 emoji
    EXPECT_TRUE(result2.is_success());
    
    auto result3 = create_valid_char(0); // NULL
    EXPECT_TRUE(result3.is_success());
    
    auto result4 = create_valid_char(0x10FFFF); // Max valid code point
    EXPECT_TRUE(result4.is_success());
    
    // Invalid code points
    auto result5 = create_valid_char(-1); // Negative
    EXPECT_TRUE(result5.is_error());
    EXPECT_EQ(result5.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
    
    auto result6 = create_valid_char(0x110000); // Too large
    EXPECT_TRUE(result6.is_error());
    
    auto result7 = create_valid_char(0xD800); // Surrogate
    EXPECT_TRUE(result7.is_error());
    
    auto result8 = create_valid_char(0xDFFF); // Surrogate
    EXPECT_TRUE(result8.is_error());
}

// Test RefinementValidator methods
TEST_F(RefinementRuntimeTest, ValidatorMethods) {
    auto uint_type = RefinementTypes::create_uint();
    
    // Test validate method
    auto valid_value = Value(std::make_shared<Integer>(42));
    auto result1 = RefinementValidator::validate(uint_type, valid_value);
    EXPECT_TRUE(result1.is_success());
    
    auto invalid_value = Value(std::make_shared<Integer>(-1));
    auto result2 = RefinementValidator::validate(uint_type, invalid_value);
    EXPECT_TRUE(result2.is_error());
    
    // Test can_cast_to method
    EXPECT_TRUE(RefinementValidator::can_cast_to(valid_value, uint_type));
    EXPECT_FALSE(RefinementValidator::can_cast_to(invalid_value, uint_type));
    
    // Test try_cast method
    auto cast_result1 = RefinementValidator::try_cast(valid_value, uint_type);
    EXPECT_TRUE(cast_result1.has_value());
    EXPECT_EQ(cast_result1->as<Integer>()->value(), 42);
    
    auto cast_result2 = RefinementValidator::try_cast(invalid_value, uint_type);
    EXPECT_FALSE(cast_result2.has_value());
}

// Test batch validation
TEST_F(RefinementRuntimeTest, BatchValidation) {
    auto uint_type = RefinementTypes::create_uint();
    
    // Valid batch
    std::vector<Value> valid_values = {
        Value(std::make_shared<Integer>(0)),
        Value(std::make_shared<Integer>(42)),
        Value(std::make_shared<Integer>(100))
    };
    
    auto result1 = RefinementValidator::validate_batch(uint_type, valid_values);
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value().size(), 3);
    
    // Invalid batch (contains negative number)
    std::vector<Value> invalid_values = {
        Value(std::make_shared<Integer>(0)),
        Value(std::make_shared<Integer>(-1)), // Invalid
        Value(std::make_shared<Integer>(100))
    };
    
    auto result2 = RefinementValidator::validate_batch(uint_type, invalid_values);
    EXPECT_TRUE(result2.is_error());
    EXPECT_EQ(result2.error().type, ValidationErrorType::CONSTRAINT_VIOLATION);
}

// Test generic refinement creation
TEST_F(RefinementRuntimeTest, GenericRefinementCreation) {
    // Test with int64_t
    auto result1 = create_refinement<int64_t>("uint", 42);
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value().as<Integer>()->value(), 42);
    
    auto result2 = create_refinement<int64_t>("uint", -1);
    EXPECT_TRUE(result2.is_error());
    
    // Test with string
    auto result3 = create_refinement<std::string>("non_empty_string", "hello");
    EXPECT_TRUE(result3.is_success());
    EXPECT_EQ(result3.value().as<String>()->value(), "hello");
    
    auto result4 = create_refinement<std::string>("non_empty_string", "");
    EXPECT_TRUE(result4.is_error());
    
    // Test with non-existent type
    auto result5 = create_refinement<int64_t>("NonExistentType", 42);
    EXPECT_TRUE(result5.is_error());
    EXPECT_EQ(result5.error().type, ValidationErrorType::INVALID_REFINEMENT);
}

// Test error message formatting
TEST_F(RefinementRuntimeTest, ErrorMessages) {
    auto result = create_positive_int(-5);
    EXPECT_TRUE(result.is_error());
    
    const auto& error = result.error();
    EXPECT_EQ(error.type, ValidationErrorType::CONSTRAINT_VIOLATION);
    
    std::string error_str = error.to_string();
    EXPECT_TRUE(error_str.find("positive_int") != std::string::npos);
    EXPECT_TRUE(error_str.find("-5") != std::string::npos);
}

// Test null refinement type handling
TEST_F(RefinementRuntimeTest, NullRefinementType) {
    std::shared_ptr<RefinementMetaType> null_type = nullptr;
    auto value = Value(std::make_shared<Integer>(42));
    
    auto result = RefinementValidator::validate(null_type, value);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().type, ValidationErrorType::INVALID_REFINEMENT);
    EXPECT_TRUE(result.error().message.find("null") != std::string::npos);
}