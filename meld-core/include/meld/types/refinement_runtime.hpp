#pragma once

#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include "result.hpp"
#include <expected>
#include <string>
#include <memory>

namespace meld::types {

// Runtime validation error types
enum class ValidationErrorType {
    TYPE_MISMATCH,      // Value is not compatible with base type
    CONSTRAINT_VIOLATION, // Value violates refinement constraint
    PREDICATE_ERROR,    // Error evaluating predicate function
    INVALID_REFINEMENT  // Refinement type is malformed
};

struct ValidationError {
    ValidationErrorType type;
    std::string message;
    std::string expected_type;
    std::string actual_value;
    
    ValidationError(ValidationErrorType t, std::string msg, 
                   std::string expected = "", std::string actual = "")
        : type(t), message(std::move(msg)), 
          expected_type(std::move(expected)), actual_value(std::move(actual)) {}
    
    std::string to_string() const {
        std::string result = message;
        if (!expected_type.empty()) {
            result += " (expected: " + expected_type;
            if (!actual_value.empty()) {
                result += ", got: " + actual_value;
            }
            result += ")";
        }
        return result;
    }
};

// Runtime refinement type validator
class RefinementValidator {
public:
    // Validate a value against a refinement type at runtime
    static Result<kernel::Value, ValidationError> validate(
        std::shared_ptr<meta::RefinementMetaType> refinement_type,
        const kernel::Value& value
    );
    
    // Create a validated instance using the .from() pattern
    static Result<kernel::Value, ValidationError> from(
        std::shared_ptr<meta::RefinementMetaType> refinement_type,
        const kernel::Value& value
    );
    
    // Batch validation for multiple values
    static Result<std::vector<kernel::Value>, ValidationError> validate_batch(
        std::shared_ptr<meta::RefinementMetaType> refinement_type,
        const std::vector<kernel::Value>& values
    );
    
    // Check if a value can be safely cast to a refinement type
    static bool can_cast_to(
        const kernel::Value& value,
        std::shared_ptr<meta::RefinementMetaType> refinement_type
    );
    
    // Attempt to cast a value to a refinement type (returns null on failure)
    static std::optional<kernel::Value> try_cast(
        const kernel::Value& value,
        std::shared_ptr<meta::RefinementMetaType> refinement_type
    );
    
private:
    // Helper methods
    static ValidationError create_type_mismatch_error(
        const kernel::Value& value,
        std::shared_ptr<meta::MetaType> expected_base_type
    );
    
    static ValidationError create_constraint_violation_error(
        const kernel::Value& value,
        std::shared_ptr<meta::RefinementMetaType> refinement_type
    );
    
    static ValidationError create_predicate_error(
        const std::string& error_message,
        std::shared_ptr<meta::RefinementMetaType> refinement_type
    );
};

// Convenience functions for built-in refinement types
namespace refinement_runtime {
    
    // PositiveInt validation
    Result<kernel::Value, ValidationError> create_positive_int(const kernel::Value& value);
    Result<kernel::Value, ValidationError> create_positive_int(int64_t value);
    
    // uint validation  
    Result<kernel::Value, ValidationError> create_uint(const kernel::Value& value);
    Result<kernel::Value, ValidationError> create_uint(int64_t value);
    
    // NonEmptyString validation
    Result<kernel::Value, ValidationError> create_non_empty_string(const kernel::Value& value);
    Result<kernel::Value, ValidationError> create_non_empty_string(const std::string& value);
    
    // Email validation
    Result<kernel::Value, ValidationError> create_email(const kernel::Value& value);
    Result<kernel::Value, ValidationError> create_email(const std::string& value);
    
    // ValidChar validation
    Result<kernel::Value, ValidationError> create_valid_char(const kernel::Value& value);
    Result<kernel::Value, ValidationError> create_valid_char(int64_t code_point);
    
    // Generic refinement type creation
    template<typename T>
    Result<kernel::Value, ValidationError> create_refinement(
        const std::string& refinement_type_name,
        const T& value
    ) {
        auto& registry = meta::TypeRegistry::instance();
        auto type_result = registry.get_type(refinement_type_name);
        
        if (!type_result.has_value()) {
            return Result<kernel::Value, ValidationError>::error(ValidationError(
                ValidationErrorType::INVALID_REFINEMENT,
                "Refinement type '" + refinement_type_name + "' not found"
            ));
        }
        
        auto refinement_type = std::dynamic_pointer_cast<meta::RefinementMetaType>(*type_result);
        if (!refinement_type) {
            return Result<kernel::Value, ValidationError>::error(ValidationError(
                ValidationErrorType::INVALID_REFINEMENT,
                "Type '" + refinement_type_name + "' is not a refinement type"
            ));
        }
        
        // Convert value to kernel::Value
        kernel::Value kernel_value;
        if constexpr (std::is_same_v<T, int64_t>) {
            kernel_value = kernel::Value(std::make_shared<kernel::Integer>(value));
        } else if constexpr (std::is_same_v<T, std::string>) {
            kernel_value = kernel::Value(std::make_shared<kernel::String>(value));
        } else if constexpr (std::is_same_v<T, bool>) {
            kernel_value = kernel::Value(value ? kernel::Boolean::true_value() : kernel::Boolean::false_value());
        } else {
            kernel_value = value; // Assume it's already a kernel::Value
        }
        
        return RefinementValidator::from(refinement_type, kernel_value);
    }
}

} // namespace meld::types