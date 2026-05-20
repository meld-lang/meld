#include <gtest/gtest.h>
#include "meld/types/refinement.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

class RefinementCompositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register built-in refinement types
        RefinementTypes::register_builtin_types();
    }
};

// Test refinement type inheritance
TEST_F(RefinementCompositionTest, InheritanceBasic) {
    auto positive_even_int = RefinementTypes::create_positive_even_int();
    
    ASSERT_NE(positive_even_int, nullptr);
    EXPECT_EQ(positive_even_int->name(), "positive_even_int");
    EXPECT_TRUE(positive_even_int->is_inherited());
    EXPECT_FALSE(positive_even_int->is_composed());
    
    // Should have PositiveInt as parent
    auto parent = positive_even_int->parent_refinement();
    ASSERT_NE(parent, nullptr);
    EXPECT_EQ(parent->name(), "positive_int");
}

// Test inheritance validation - valid values
TEST_F(RefinementCompositionTest, InheritanceValidationValid) {
    auto positive_even_int = RefinementTypes::create_positive_even_int();
    
    // Test valid positive even integers
    auto result1 = positive_even_int->validate_value(Value(std::make_shared<Integer>(2)));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value());
    
    auto result2 = positive_even_int->validate_value(Value(std::make_shared<Integer>(10)));
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2.value());
    
    auto result3 = positive_even_int->validate_value(Value(std::make_shared<Integer>(100)));
    ASSERT_TRUE(result3.has_value());
    EXPECT_TRUE(result3.value());
}

// Test inheritance validation - invalid values
TEST_F(RefinementCompositionTest, InheritanceValidationInvalid) {
    auto positive_even_int = RefinementTypes::create_positive_even_int();
    
    // Test negative even integers (fails parent constraint)
    auto result1 = positive_even_int->validate_value(Value(std::make_shared<Integer>(-2)));
    ASSERT_TRUE(result1.has_value());
    EXPECT_FALSE(result1.value());
    
    // Test positive odd integers (fails additional constraint)
    auto result2 = positive_even_int->validate_value(Value(std::make_shared<Integer>(3)));
    ASSERT_TRUE(result2.has_value());
    EXPECT_FALSE(result2.value());
    
    // Test negative odd integers (fails both constraints)
    auto result3 = positive_even_int->validate_value(Value(std::make_shared<Integer>(-3)));
    ASSERT_TRUE(result3.has_value());
    EXPECT_FALSE(result3.value());
    
    // Test zero (fails parent constraint - not positive)
    auto result4 = positive_even_int->validate_value(Value(std::make_shared<Integer>(0)));
    ASSERT_TRUE(result4.has_value());
    EXPECT_FALSE(result4.value());
}

// Test OR composition
TEST_F(RefinementCompositionTest, CompositionOrBasic) {
    auto email_or_phone = RefinementTypes::create_valid_email_or_phone();
    
    ASSERT_NE(email_or_phone, nullptr);
    EXPECT_EQ(email_or_phone->name(), "valid_email_or_phone");
    EXPECT_FALSE(email_or_phone->is_inherited());
    EXPECT_TRUE(email_or_phone->is_composed());
    
    // Should have 2 composed refinements (Email and Phone)
    auto composed = email_or_phone->composed_refinements();
    EXPECT_EQ(composed.size(), 2);
}

// Test OR composition validation - valid values
TEST_F(RefinementCompositionTest, CompositionOrValidationValid) {
    auto email_or_phone = RefinementTypes::create_valid_email_or_phone();
    
    // Test valid email
    auto result1 = email_or_phone->validate_value(Value(std::make_shared<String>("test@example.com")));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value());
    
    // Test valid phone number
    auto result2 = email_or_phone->validate_value(Value(std::make_shared<String>("(555) 123-4567")));
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2.value());
    
    // Test another valid phone format
    auto result3 = email_or_phone->validate_value(Value(std::make_shared<String>("+1-555-123-4567")));
    ASSERT_TRUE(result3.has_value());
    EXPECT_TRUE(result3.value());
}

// Test OR composition validation - invalid values
TEST_F(RefinementCompositionTest, CompositionOrValidationInvalid) {
    auto email_or_phone = RefinementTypes::create_valid_email_or_phone();
    
    // Test invalid string (neither email nor phone)
    auto result1 = email_or_phone->validate_value(Value(std::make_shared<String>("not-valid")));
    ASSERT_TRUE(result1.has_value());
    EXPECT_FALSE(result1.value());
    
    // Test empty string
    auto result2 = email_or_phone->validate_value(Value(std::make_shared<String>("")));
    ASSERT_TRUE(result2.has_value());
    EXPECT_FALSE(result2.value());
    
    // Test malformed email
    auto result3 = email_or_phone->validate_value(Value(std::make_shared<String>("invalid-email")));
    ASSERT_TRUE(result3.has_value());
    EXPECT_FALSE(result3.value());
    
    // Test malformed phone
    auto result4 = email_or_phone->validate_value(Value(std::make_shared<String>("123")));
    ASSERT_TRUE(result4.has_value());
    EXPECT_FALSE(result4.value());
}

// Test AND composition
TEST_F(RefinementCompositionTest, CompositionAndBasic) {
    auto short_non_empty = RefinementTypes::create_short_non_empty_string();
    
    ASSERT_NE(short_non_empty, nullptr);
    EXPECT_EQ(short_non_empty->name(), "short_non_empty_string");
    EXPECT_FALSE(short_non_empty->is_inherited());
    EXPECT_TRUE(short_non_empty->is_composed());
    
    // Should have 2 composed refinements (NonEmptyString and ShortString)
    auto composed = short_non_empty->composed_refinements();
    EXPECT_EQ(composed.size(), 2);
}

// Test AND composition validation - valid values
TEST_F(RefinementCompositionTest, CompositionAndValidationValid) {
    auto short_non_empty = RefinementTypes::create_short_non_empty_string();
    
    // Test valid short non-empty strings
    auto result1 = short_non_empty->validate_value(Value(std::make_shared<String>("hello")));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value());
    
    auto result2 = short_non_empty->validate_value(Value(std::make_shared<String>("a")));
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2.value());
    
    // Test string at the boundary (50 characters)
    std::string boundary_string(50, 'x');
    auto result3 = short_non_empty->validate_value(Value(std::make_shared<String>(boundary_string)));
    ASSERT_TRUE(result3.has_value());
    EXPECT_TRUE(result3.value());
}

// Test AND composition validation - invalid values
TEST_F(RefinementCompositionTest, CompositionAndValidationInvalid) {
    auto short_non_empty = RefinementTypes::create_short_non_empty_string();
    
    // Test empty string (fails non-empty constraint)
    auto result1 = short_non_empty->validate_value(Value(std::make_shared<String>("")));
    ASSERT_TRUE(result1.has_value());
    EXPECT_FALSE(result1.value());
    
    // Test long string (fails short constraint)
    std::string long_string(100, 'x');
    auto result2 = short_non_empty->validate_value(Value(std::make_shared<String>(long_string)));
    ASSERT_TRUE(result2.has_value());
    EXPECT_FALSE(result2.value());
    
    // Test string just over the boundary (51 characters)
    std::string over_boundary_string(51, 'x');
    auto result3 = short_non_empty->validate_value(Value(std::make_shared<String>(over_boundary_string)));
    ASSERT_TRUE(result3.has_value());
    EXPECT_FALSE(result3.value());
}

// Test from_value method with inheritance
TEST_F(RefinementCompositionTest, InheritanceFromValue) {
    auto positive_even_int = RefinementTypes::create_positive_even_int();
    
    // Test valid value
    auto result1 = positive_even_int->from_value(Value(std::make_shared<Integer>(4)));
    ASSERT_TRUE(result1.has_value());
    
    // Test invalid value
    auto result2 = positive_even_int->from_value(Value(std::make_shared<Integer>(3)));
    EXPECT_FALSE(result2.has_value());
}

// Test from_value method with composition
TEST_F(RefinementCompositionTest, CompositionFromValue) {
    auto email_or_phone = RefinementTypes::create_valid_email_or_phone();
    
    // Test valid email
    auto result1 = email_or_phone->from_value(Value(std::make_shared<String>("test@example.com")));
    ASSERT_TRUE(result1.has_value());
    
    // Test valid phone
    auto result2 = email_or_phone->from_value(Value(std::make_shared<String>("555-123-4567")));
    ASSERT_TRUE(result2.has_value());
    
    // Test invalid value
    auto result3 = email_or_phone->from_value(Value(std::make_shared<String>("invalid")));
    EXPECT_FALSE(result3.has_value());
}

// Test type compatibility with inheritance
TEST_F(RefinementCompositionTest, InheritanceTypeCompatibility) {
    auto& registry = TypeRegistry::instance();
    auto positive_int = registry.get_type("positive_int");
    auto positive_even_int = registry.get_type("positive_even_int");
    
    ASSERT_TRUE(positive_int.has_value());
    ASSERT_TRUE(positive_even_int.has_value());
    
    // PositiveEvenInt should be a subtype of PositiveInt
    EXPECT_TRUE(positive_even_int.value()->is_subtype_of(*positive_int.value()));
    
    // PositiveInt should be assignable from PositiveEvenInt
    EXPECT_TRUE(positive_int.value()->is_assignable_from(*positive_even_int.value()));
    
    // But not the other way around
    EXPECT_FALSE(positive_int.value()->is_subtype_of(*positive_even_int.value()));
    EXPECT_FALSE(positive_even_int.value()->is_assignable_from(*positive_int.value()));
}

// Test error handling for invalid composition
TEST_F(RefinementCompositionTest, InvalidComposition) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    auto string_type = registry.get_string_type();
    
    // Create refinements with incompatible base types
    auto positive_int = RefinementTypes::create_positive_int();  // int-based
    auto email = RefinementTypes::create_email();               // string-based
    
    // This should throw because base types are incompatible
    EXPECT_THROW(
        RefinementTypes::create_composed_and_refinement("Invalid", int_type, {positive_int, email}),
        std::invalid_argument
    );
}

// Test custom inheritance using factory method
TEST_F(RefinementCompositionTest, CustomInheritance) {
    auto positive_int = RefinementTypes::create_positive_int();
    
    // Create a custom inherited type: positive multiples of 3
    auto positive_multiple_of_3 = RefinementTypes::create_inherited_refinement(
        "PositiveMultipleOf3",
        positive_int,
        [](const Value& value) -> std::expected<bool, std::string> {
            if (!value.is<Integer>()) {
                return std::unexpected("Expected integer value");
            }
            return value.as<Integer>()->value() % 3 == 0;
        }
    );
    
    ASSERT_NE(positive_multiple_of_3, nullptr);
    EXPECT_EQ(positive_multiple_of_3->name(), "PositiveMultipleOf3");
    EXPECT_TRUE(positive_multiple_of_3->is_inherited());
    
    // Test validation
    auto result1 = positive_multiple_of_3->validate_value(Value(std::make_shared<Integer>(6)));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value());
    
    auto result2 = positive_multiple_of_3->validate_value(Value(std::make_shared<Integer>(7)));
    ASSERT_TRUE(result2.has_value());
    EXPECT_FALSE(result2.value());
    
    auto result3 = positive_multiple_of_3->validate_value(Value(std::make_shared<Integer>(-3)));
    ASSERT_TRUE(result3.has_value());
    EXPECT_FALSE(result3.value());
}