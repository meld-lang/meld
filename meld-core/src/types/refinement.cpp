#include "meld/types/refinement.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include <regex>
#include <functional>
#include <cctype>

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Helper to create predicate functions
Value RefinementTypes::create_predicate_function(std::function<std::expected<bool, std::string>(const Value&)> validator) {
    // For now, we'll store the validator as a lambda wrapped in a Value
    // In a full implementation, this would be a proper Meld function
    Function::NativeImpl impl = [validator](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            // Return error as a string for now - in full implementation would be proper error type
            return Value(std::make_shared<String>("Predicate function expects exactly one argument"));
        }
        
        auto result = validator(args[0]);
        if (!result.has_value()) {
            return Value(std::make_shared<String>(result.error()));
        }
        
        return Value(result.value() ? Boolean::true_value() : Boolean::false_value());
    };
    return Value(std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        impl,
        std::string("predicate")));
}

// Built-in refinement types
std::shared_ptr<RefinementMetaType> RefinementTypes::create_positive_int() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function(validate_positive_int);
    
    return std::make_shared<RefinementMetaType>("positive_int", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_non_empty_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_non_empty_string);
    
    return std::make_shared<RefinementMetaType>("non_empty_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_email() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_email);
    
    return std::make_shared<RefinementMetaType>("email", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_uint() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function(validate_uint);
    
    return std::make_shared<RefinementMetaType>("uint", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_valid_char() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();  // char is stored as int (code point)
    
    auto predicate = create_predicate_function(validate_char);
    
    return std::make_shared<RefinementMetaType>("valid_char", int_type, predicate);
}

// Additional common refinement types
std::shared_ptr<RefinementMetaType> RefinementTypes::create_negative_int() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function(validate_negative_int);
    
    return std::make_shared<RefinementMetaType>("negative_int", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_non_zero_int() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function(validate_non_zero_int);
    
    return std::make_shared<RefinementMetaType>("non_zero_int", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_even_int() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function(validate_even_int);
    
    return std::make_shared<RefinementMetaType>("even_int", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_odd_int() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto predicate = create_predicate_function([](const Value& value) -> std::expected<bool, std::string> {
        if (!value.is<Integer>()) {
            return std::unexpected("Expected integer value");
        }
        return value.as<Integer>()->value() % 2 != 0;
    });
    
    return std::make_shared<RefinementMetaType>("odd_int", int_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_percentage() {
    auto& registry = TypeRegistry::instance();
    auto float_type = registry.get_float_type();
    
    auto predicate = create_predicate_function(validate_percentage);
    
    return std::make_shared<RefinementMetaType>("percentage", float_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_normalized_float() {
    auto& registry = TypeRegistry::instance();
    auto float_type = registry.get_float_type();
    
    auto predicate = create_predicate_function(validate_normalized_float);
    
    return std::make_shared<RefinementMetaType>("normalized_float", float_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_positive_float() {
    auto& registry = TypeRegistry::instance();
    auto float_type = registry.get_float_type();
    
    auto predicate = create_predicate_function(validate_positive_float);
    
    return std::make_shared<RefinementMetaType>("positive_float", float_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_url() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_url);
    
    return std::make_shared<RefinementMetaType>("url", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_phone_number() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_phone_number);
    
    return std::make_shared<RefinementMetaType>("phone_number", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_alphanumeric_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_alphanumeric_string);
    
    return std::make_shared<RefinementMetaType>("alphanumeric_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_uppercase_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_uppercase_string);
    
    return std::make_shared<RefinementMetaType>("uppercase_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_lowercase_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_lowercase_string);
    
    return std::make_shared<RefinementMetaType>("lowercase_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_trimmed_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_trimmed_string);
    
    return std::make_shared<RefinementMetaType>("trimmed_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_hex_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_hex_string);
    
    return std::make_shared<RefinementMetaType>("hex_string", string_type, predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_base64_string() {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto predicate = create_predicate_function(validate_base64_string);
    
    return std::make_shared<RefinementMetaType>("base64_string", string_type, predicate);
}

// Validation utilities
std::expected<bool, std::string> RefinementTypes::validate_positive_int(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value");
    }
    
    return value.as<Integer>()->value() > 0;
}

std::expected<bool, std::string> RefinementTypes::validate_non_empty_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    return !value.as<String>()->value().empty();
}

std::expected<bool, std::string> RefinementTypes::validate_email(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& email = value.as<String>()->value();
    
    // Simple email validation regex
    std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    
    return std::regex_match(email, email_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_uint(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value");
    }
    
    return value.as<Integer>()->value() >= 0;
}

std::expected<bool, std::string> RefinementTypes::validate_char(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value (Unicode code point)");
    }
    
    int64_t code_point = value.as<Integer>()->value();
    
    // Valid Unicode scalar values are 0 to 0x10FFFF, excluding surrogates (0xD800-0xDFFF)
    if (code_point < 0 || code_point > 0x10FFFF) {
        return false;
    }
    
    // Exclude surrogate pairs
    if (code_point >= 0xD800 && code_point <= 0xDFFF) {
        return false;
    }
    
    return true;
}

// Additional validators for new refinement types
std::expected<bool, std::string> RefinementTypes::validate_negative_int(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value");
    }
    
    return value.as<Integer>()->value() < 0;
}

std::expected<bool, std::string> RefinementTypes::validate_non_zero_int(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value");
    }
    
    return value.as<Integer>()->value() != 0;
}

std::expected<bool, std::string> RefinementTypes::validate_percentage(const Value& value) {
    if (!value.is<Float>()) {
        return std::unexpected("Expected float value");
    }
    
    double val = value.as<Float>()->value();
    return val >= 0.0 && val <= 100.0;
}

std::expected<bool, std::string> RefinementTypes::validate_normalized_float(const Value& value) {
    if (!value.is<Float>()) {
        return std::unexpected("Expected float value");
    }
    
    double val = value.as<Float>()->value();
    return val >= 0.0 && val <= 1.0;
}

std::expected<bool, std::string> RefinementTypes::validate_positive_float(const Value& value) {
    if (!value.is<Float>()) {
        return std::unexpected("Expected float value");
    }
    
    return value.as<Float>()->value() > 0.0;
}

std::expected<bool, std::string> RefinementTypes::validate_url(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& url = value.as<String>()->value();
    
    // Basic URL validation - must start with http:// or https://
    std::regex url_regex(R"(^https?://[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}(/.*)?$)");
    
    return std::regex_match(url, url_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_phone_number(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& phone = value.as<String>()->value();
    
    // Enhanced phone validation - digits, spaces, hyphens, parentheses, plus sign
    std::regex phone_regex(R"(^[\+]?[\d\s\-\(\)]{10,15}$)");
    
    return std::regex_match(phone, phone_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_alphanumeric_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Only letters and digits allowed
    std::regex alphanumeric_regex(R"(^[a-zA-Z0-9]*$)");
    
    return std::regex_match(str, alphanumeric_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_uppercase_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Check if all alphabetic characters are uppercase
    for (char c : str) {
        if (std::isalpha(c) && !std::isupper(c)) {
            return false;
        }
    }
    
    return true;
}

std::expected<bool, std::string> RefinementTypes::validate_lowercase_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Check if all alphabetic characters are lowercase
    for (char c : str) {
        if (std::isalpha(c) && !std::islower(c)) {
            return false;
        }
    }
    
    return true;
}

std::expected<bool, std::string> RefinementTypes::validate_trimmed_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Check if string has no leading or trailing whitespace
    if (str.empty()) {
        return true;  // Empty string is considered trimmed
    }
    
    return !std::isspace(str.front()) && !std::isspace(str.back());
}

std::expected<bool, std::string> RefinementTypes::validate_hex_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Only hexadecimal characters allowed (0-9, a-f, A-F)
    std::regex hex_regex(R"(^[0-9a-fA-F]*$)");
    
    return std::regex_match(str, hex_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_base64_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Base64 characters: A-Z, a-z, 0-9, +, /, with optional = padding
    std::regex base64_regex(R"(^[A-Za-z0-9+/]*={0,2}$)");
    
    // Check basic format
    if (!std::regex_match(str, base64_regex)) {
        return false;
    }
    
    // Check length is multiple of 4 (base64 requirement)
    return str.length() % 4 == 0;
}

// Register built-in refinement types
void RefinementTypes::register_builtin_types() {
    auto& registry = TypeRegistry::instance();
    
    // Original built-in types
    registry.register_type("positive_int", create_positive_int());
    registry.register_type("non_empty_string", create_non_empty_string());
    registry.register_type("email", create_email());
    registry.register_type("uint", create_uint());
    registry.register_type("valid_char", create_valid_char());
    
    // Additional common refinement types
    registry.register_type("negative_int", create_negative_int());
    registry.register_type("non_zero_int", create_non_zero_int());
    registry.register_type("even_int", create_even_int());
    registry.register_type("odd_int", create_odd_int());
    registry.register_type("percentage", create_percentage());
    registry.register_type("normalized_float", create_normalized_float());
    registry.register_type("positive_float", create_positive_float());
    registry.register_type("url", create_url());
    registry.register_type("phone_number", create_phone_number());
    registry.register_type("alphanumeric_string", create_alphanumeric_string());
    registry.register_type("uppercase_string", create_uppercase_string());
    registry.register_type("lowercase_string", create_lowercase_string());
    registry.register_type("trimmed_string", create_trimmed_string());
    registry.register_type("hex_string", create_hex_string());
    registry.register_type("base64_string", create_base64_string());
    
    // Register composed and inherited types
    registry.register_type("positive_even_int", create_positive_even_int());
    registry.register_type("valid_email_or_phone", create_valid_email_or_phone());
    registry.register_type("short_non_empty_string", create_short_non_empty_string());
}

// Composition and inheritance factory methods
std::shared_ptr<RefinementMetaType> RefinementTypes::create_inherited_refinement(
    const std::string& name,
    std::shared_ptr<RefinementMetaType> parent,
    std::function<std::expected<bool, std::string>(const kernel::Value&)> additional_validator) {
    
    auto additional_predicate = create_predicate_function(additional_validator);
    return RefinementMetaType::create_inherited(name, parent, additional_predicate);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_composed_and_refinement(
    const std::string& name,
    std::shared_ptr<MetaType> base_type,
    const std::vector<std::shared_ptr<RefinementMetaType>>& refinements) {
    
    return RefinementMetaType::create_composed_and(name, base_type, refinements);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_composed_or_refinement(
    const std::string& name,
    std::shared_ptr<MetaType> base_type,
    const std::vector<std::shared_ptr<RefinementMetaType>>& refinements) {
    
    return RefinementMetaType::create_composed_or(name, base_type, refinements);
}

// Example composed refinement types
std::shared_ptr<RefinementMetaType> RefinementTypes::create_positive_even_int() {
    // Inherits from PositiveInt and adds even constraint
    auto positive_int = create_positive_int();
    return create_inherited_refinement("positive_even_int", positive_int, validate_even_int);
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_valid_email_or_phone() {
    // Composes Email OR Phone (both are string-based)
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto email = create_email();
    
    // Create a phone refinement type
    auto phone_predicate = create_predicate_function(validate_phone);
    auto phone = std::make_shared<RefinementMetaType>("Phone", string_type, phone_predicate);
    
    return create_composed_or_refinement("valid_email_or_phone", string_type, {email, phone});
}

std::shared_ptr<RefinementMetaType> RefinementTypes::create_short_non_empty_string() {
    // Composes NonEmptyString AND ShortString
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto non_empty_string = create_non_empty_string();
    
    // Create a short string refinement type
    auto short_predicate = create_predicate_function(validate_short_string);
    auto short_string = std::make_shared<RefinementMetaType>("ShortString", string_type, short_predicate);
    
    return create_composed_and_refinement("short_non_empty_string", string_type, {non_empty_string, short_string});
}

// Additional validators for composition examples
std::expected<bool, std::string> RefinementTypes::validate_even_int(const Value& value) {
    if (!value.is<Integer>()) {
        return std::unexpected("Expected integer value");
    }
    
    return value.as<Integer>()->value() % 2 == 0;
}

std::expected<bool, std::string> RefinementTypes::validate_phone(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& phone = value.as<String>()->value();
    
    // Simple phone validation - digits, spaces, hyphens, parentheses, plus sign
    std::regex phone_regex(R"(^[\+]?[\d\s\-\(\)]{10,15}$)");
    
    return std::regex_match(phone, phone_regex);
}

std::expected<bool, std::string> RefinementTypes::validate_short_string(const Value& value) {
    if (!value.is<String>()) {
        return std::unexpected("Expected string value");
    }
    
    const std::string& str = value.as<String>()->value();
    
    // Short string is defined as <= 50 characters
    return str.length() <= 50;
}