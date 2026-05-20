#pragma once

#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include <expected>
#include <string>

namespace meld::types {

// Refinement type utilities and built-in refinement types
class RefinementTypes {
public:
    // Built-in refinement types
    static std::shared_ptr<meta::RefinementMetaType> create_positive_int();
    static std::shared_ptr<meta::RefinementMetaType> create_non_empty_string();
    static std::shared_ptr<meta::RefinementMetaType> create_email();
    static std::shared_ptr<meta::RefinementMetaType> create_uint();
    static std::shared_ptr<meta::RefinementMetaType> create_valid_char();
    
    // Additional common refinement types
    static std::shared_ptr<meta::RefinementMetaType> create_negative_int();
    static std::shared_ptr<meta::RefinementMetaType> create_non_zero_int();
    static std::shared_ptr<meta::RefinementMetaType> create_even_int();
    static std::shared_ptr<meta::RefinementMetaType> create_odd_int();
    static std::shared_ptr<meta::RefinementMetaType> create_percentage();
    static std::shared_ptr<meta::RefinementMetaType> create_normalized_float();
    static std::shared_ptr<meta::RefinementMetaType> create_positive_float();
    static std::shared_ptr<meta::RefinementMetaType> create_url();
    static std::shared_ptr<meta::RefinementMetaType> create_phone_number();
    static std::shared_ptr<meta::RefinementMetaType> create_alphanumeric_string();
    static std::shared_ptr<meta::RefinementMetaType> create_uppercase_string();
    static std::shared_ptr<meta::RefinementMetaType> create_lowercase_string();
    static std::shared_ptr<meta::RefinementMetaType> create_trimmed_string();
    static std::shared_ptr<meta::RefinementMetaType> create_hex_string();
    static std::shared_ptr<meta::RefinementMetaType> create_base64_string();
    
    // Composition and inheritance factory methods
    static std::shared_ptr<meta::RefinementMetaType> create_inherited_refinement(
        const std::string& name,
        std::shared_ptr<meta::RefinementMetaType> parent,
        std::function<std::expected<bool, std::string>(const kernel::Value&)> additional_validator
    );
    
    static std::shared_ptr<meta::RefinementMetaType> create_composed_and_refinement(
        const std::string& name,
        std::shared_ptr<meta::MetaType> base_type,
        const std::vector<std::shared_ptr<meta::RefinementMetaType>>& refinements
    );
    
    static std::shared_ptr<meta::RefinementMetaType> create_composed_or_refinement(
        const std::string& name,
        std::shared_ptr<meta::MetaType> base_type,
        const std::vector<std::shared_ptr<meta::RefinementMetaType>>& refinements
    );
    
    // Example composed refinement types
    static std::shared_ptr<meta::RefinementMetaType> create_positive_even_int();  // Inherits from PositiveInt
    static std::shared_ptr<meta::RefinementMetaType> create_valid_email_or_phone(); // Composes Email OR Phone
    static std::shared_ptr<meta::RefinementMetaType> create_short_non_empty_string(); // Composes NonEmptyString AND ShortString
    
    // Validation utilities
    static std::expected<bool, std::string> validate_positive_int(const kernel::Value& value);
    static std::expected<bool, std::string> validate_non_empty_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_email(const kernel::Value& value);
    static std::expected<bool, std::string> validate_uint(const kernel::Value& value);
    static std::expected<bool, std::string> validate_char(const kernel::Value& value);
    
    // Additional validators for new refinement types
    static std::expected<bool, std::string> validate_negative_int(const kernel::Value& value);
    static std::expected<bool, std::string> validate_non_zero_int(const kernel::Value& value);
    static std::expected<bool, std::string> validate_percentage(const kernel::Value& value);
    static std::expected<bool, std::string> validate_normalized_float(const kernel::Value& value);
    static std::expected<bool, std::string> validate_positive_float(const kernel::Value& value);
    static std::expected<bool, std::string> validate_url(const kernel::Value& value);
    static std::expected<bool, std::string> validate_phone_number(const kernel::Value& value);
    static std::expected<bool, std::string> validate_alphanumeric_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_uppercase_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_lowercase_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_trimmed_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_hex_string(const kernel::Value& value);
    static std::expected<bool, std::string> validate_base64_string(const kernel::Value& value);
    
    // Additional validators for composition examples
    static std::expected<bool, std::string> validate_even_int(const kernel::Value& value);
    static std::expected<bool, std::string> validate_phone(const kernel::Value& value);
    static std::expected<bool, std::string> validate_short_string(const kernel::Value& value);
    
    // Register built-in refinement types with the type registry
    static void register_builtin_types();
    
private:
    // Helper to create predicate functions
    static kernel::Value create_predicate_function(std::function<std::expected<bool, std::string>(const kernel::Value&)> validator);
};

// Refinement type validation error
class RefinementValidationError : public std::exception {
public:
    explicit RefinementValidationError(std::string message)
        : message_(std::move(message)) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    const std::string& message() const { return message_; }
    
private:
    std::string message_;
};

} // namespace meld::types