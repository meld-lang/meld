#include "meld/types/refinement_runtime.hpp"
#include "meld/types/refinement.hpp"
#include "meld/meta/metatype.hpp"
#include <format>

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Validate a value against a refinement type at runtime
Result<Value, ValidationError> RefinementValidator::validate(
    std::shared_ptr<RefinementMetaType> refinement_type,
    const Value& value) {
    
    if (!refinement_type) {
        return Result<Value, ValidationError>::error(ValidationError(
            ValidationErrorType::INVALID_REFINEMENT,
            "Refinement type is null"
        ));
    }
    
    // First check base type compatibility
    // This is a simplified check - in a full implementation we'd use proper type checking
    
    // Validate against the refinement constraint
    auto validation_result = refinement_type->validate_value(value);
    if (!validation_result.has_value()) {
        return Result<Value, ValidationError>::error(
            create_predicate_error(validation_result.error(), refinement_type));
    }
    
    if (!validation_result.value()) {
        return Result<Value, ValidationError>::error(
            create_constraint_violation_error(value, refinement_type));
    }
    
    // Validation passed - return the value
    return Result<Value, ValidationError>::success(value);
}

// Create a validated instance using the .from() pattern
Result<Value, ValidationError> RefinementValidator::from(
    std::shared_ptr<RefinementMetaType> refinement_type,
    const Value& value) {
    
    return validate(refinement_type, value);
}

// Batch validation for multiple values
Result<std::vector<Value>, ValidationError> RefinementValidator::validate_batch(
    std::shared_ptr<RefinementMetaType> refinement_type,
    const std::vector<Value>& values) {
    
    std::vector<Value> validated_values;
    validated_values.reserve(values.size());
    
    for (const auto& value : values) {
        auto result = validate(refinement_type, value);
        if (!result.is_success()) {
            return Result<std::vector<Value>, ValidationError>::error(result.error());
        }
        validated_values.push_back(result.value());
    }
    
    return Result<std::vector<Value>, ValidationError>::success(std::move(validated_values));
}

// Check if a value can be safely cast to a refinement type
bool RefinementValidator::can_cast_to(
    const Value& value,
    std::shared_ptr<RefinementMetaType> refinement_type) {
    
    auto result = validate(refinement_type, value);
    return result.is_success();
}

// Attempt to cast a value to a refinement type (returns null on failure)
std::optional<Value> RefinementValidator::try_cast(
    const Value& value,
    std::shared_ptr<RefinementMetaType> refinement_type) {
    
    auto result = validate(refinement_type, value);
    if (result.is_success()) {
        return result.value();
    }
    return std::nullopt;
}

// Helper methods
ValidationError RefinementValidator::create_type_mismatch_error(
    const Value& value,
    std::shared_ptr<MetaType> expected_base_type) {
    
    return ValidationError(
        ValidationErrorType::TYPE_MISMATCH,
        "Type mismatch",
        expected_base_type->name(),
        value.to_string()
    );
}

ValidationError RefinementValidator::create_constraint_violation_error(
    const Value& value,
    std::shared_ptr<RefinementMetaType> refinement_type) {
    
    return ValidationError(
        ValidationErrorType::CONSTRAINT_VIOLATION,
        std::format("Value '{}' does not satisfy constraint for type '{}'", 
                   value.to_string(), refinement_type->name()),
        refinement_type->name(),
        value.to_string()
    );
}

ValidationError RefinementValidator::create_predicate_error(
    const std::string& error_message,
    std::shared_ptr<RefinementMetaType> refinement_type) {
    
    return ValidationError(
        ValidationErrorType::PREDICATE_ERROR,
        std::format("Error evaluating predicate for type '{}': {}", 
                   refinement_type->name(), error_message),
        refinement_type->name()
    );
}

// Convenience functions for built-in refinement types
namespace meld::types::refinement_runtime {

Result<Value, ValidationError> create_positive_int(const Value& value) {
    auto positive_int = RefinementTypes::create_positive_int();
    return RefinementValidator::from(positive_int, value);
}

Result<Value, ValidationError> create_positive_int(int64_t value) {
    return create_positive_int(Value(std::make_shared<Integer>(value)));
}

Result<Value, ValidationError> create_uint(const Value& value) {
    auto uint_type = RefinementTypes::create_uint();
    return RefinementValidator::from(uint_type, value);
}

Result<Value, ValidationError> create_uint(int64_t value) {
    return create_uint(Value(std::make_shared<Integer>(value)));
}

Result<Value, ValidationError> create_non_empty_string(const Value& value) {
    auto non_empty_string = RefinementTypes::create_non_empty_string();
    return RefinementValidator::from(non_empty_string, value);
}

Result<Value, ValidationError> create_non_empty_string(const std::string& value) {
    return create_non_empty_string(Value(std::make_shared<String>(value)));
}

Result<Value, ValidationError> create_email(const Value& value) {
    auto email_type = RefinementTypes::create_email();
    return RefinementValidator::from(email_type, value);
}

Result<Value, ValidationError> create_email(const std::string& value) {
    return create_email(Value(std::make_shared<String>(value)));
}

Result<Value, ValidationError> create_valid_char(const Value& value) {
    auto valid_char = RefinementTypes::create_valid_char();
    return RefinementValidator::from(valid_char, value);
}

Result<Value, ValidationError> create_valid_char(int64_t code_point) {
    return create_valid_char(Value(std::make_shared<Integer>(code_point)));
}

} // namespace meld::types::refinement_runtime