#pragma once

#include "meld/types/refinement.hpp"
#include "meld/types/refinement_runtime.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <memory>
#include <cstdint>
#include <string>

namespace meld::stdx {

/**
 * Numeric wrapper types for Meld standard library.
 * 
 * This implements Requirement 47: Unsigned Types and Numeric Wrappers (Library-Based)
 * 
 * NOTE: This file lives in namespace meld::stdx which shadows ::std.
 * All C++ standard library references MUST use ::std:: prefix.
 * 
 * The approach uses two layers:
 * 1. Refinement Types: For logical safety (AI-facing) - e.g., uint -> int where { it >= 0 }
 * 2. Struct Wrappers: For machine representation (Transpiler-facing) - e.g., struct u8 { bits: int }
 */

// Use fully-qualified names to avoid shadowing by meld::std
using ::meld::kernel::Value;
using ::meld::meta::StructMetaType;
using ::meld::meta::RefinementMetaType;

class NumericWrappers {
public:
    // === Refinement Types (Logical Layer) ===
    
    /**
     * Create uint refinement type: type uint -> int where { it >= 0 }
     * Validates: Requirements 47.2, 47.9
     */
    static ::std::shared_ptr<RefinementMetaType> create_uint_refinement();
    
    /**
     * Validate unsigned integer constraint at compile-time and runtime
     */
    static ::meld::types::Result<Value, ::meld::types::ValidationError> validate_uint(const Value& value);
    static ::meld::types::Result<Value, ::meld::types::ValidationError> validate_uint(int64_t value);
    
    // === Struct Wrappers (Machine Layer) ===
    
    /**
     * Create bit-width integer wrapper structs
     * Validates: Requirements 47.3, 47.4, 47.8
     */
    static ::std::shared_ptr<StructMetaType> create_u8_struct();
    static ::std::shared_ptr<StructMetaType> create_u16_struct();
    static ::std::shared_ptr<StructMetaType> create_u32_struct();
    static ::std::shared_ptr<StructMetaType> create_u64_struct();
    static ::std::shared_ptr<StructMetaType> create_i8_struct();
    static ::std::shared_ptr<StructMetaType> create_i16_struct();
    
    /**
     * Create char struct for Unicode scalar values
     * Validates: Requirements 47.7
     */
    static ::std::shared_ptr<StructMetaType> create_char_struct();
    static ::std::shared_ptr<RefinementMetaType> create_valid_char_refinement();
    
    /**
     * Create Buffer struct for packed byte arrays
     * Validates: Requirements 47.10
     */
    static ::std::shared_ptr<StructMetaType> create_buffer_struct();
    
    // === Bitwise Operations (Library Methods) ===
    
    /**
     * Implement bitwise operators for unsigned types
     * Validates: Requirements 47.5
     */
    static Value logical_right_shift(const Value& value, int shift_amount);
    static Value bitwise_and(const Value& left, const Value& right);
    static Value bitwise_or(const Value& left, const Value& right);
    static Value bitwise_xor(const Value& left, const Value& right);
    static Value bitwise_not(const Value& value);
    
    // === Literal Suffix Support ===
    
    /**
     * Create typed integer values from literals
     * Validates: Requirements 47.6
     */
    static Value create_u8(int64_t value);
    static Value create_u16(int64_t value);
    static Value create_u32(int64_t value);
    static Value create_u64(int64_t value);
    static Value create_i8(int64_t value);
    static Value create_i16(int64_t value);
    
    /**
     * Create char from code point or literal
     */
    static Value create_char(int64_t code_point);
    static Value create_char(char ascii_char);
    
    // === Char Utility Methods ===
    
    /**
     * Character utility methods
     */
    static bool is_digit(const Value& char_value);
    static bool is_alpha(const Value& char_value);
    static bool is_alphanumeric(const Value& char_value);
    static bool is_whitespace(const Value& char_value);
    static Value to_upper(const Value& char_value);
    static Value to_lower(const Value& char_value);
    
    // === Registration ===
    
    /**
     * Register all numeric wrapper types with the type registry
     */
    static void register_all_types();
    
    /**
     * Register bitwise operators with the operator registry
     */
    static void register_bitwise_operators();
    
private:
    // Helper methods for validation
    static bool is_valid_u8(int64_t value);
    static bool is_valid_u16(int64_t value);
    static bool is_valid_u32(int64_t value);
    static bool is_valid_u64(int64_t value);
    static bool is_valid_i8(int64_t value);
    static bool is_valid_i16(int64_t value);
    static bool is_valid_unicode_scalar(int64_t code_point);
    
    // Helper to create struct with @transpile_as and @value annotations
    static ::std::shared_ptr<StructMetaType> create_wrapper_struct(
        const ::std::string& name,
        const ::std::string& cpp_type,
        const ::std::string& java_type,
        const ::std::string& go_type
    );
};

// Convenience functions for creating typed values
namespace numeric {
    
    // Unsigned types
    inline Value u8(int64_t value) { return NumericWrappers::create_u8(value); }
    inline Value u16(int64_t value) { return NumericWrappers::create_u16(value); }
    inline Value u32(int64_t value) { return NumericWrappers::create_u32(value); }
    inline Value u64(int64_t value) { return NumericWrappers::create_u64(value); }
    
    // Signed types
    inline Value i8(int64_t value) { return NumericWrappers::create_i8(value); }
    inline Value i16(int64_t value) { return NumericWrappers::create_i16(value); }
    
    // Character type
    inline Value char_from_code(int64_t code_point) { return NumericWrappers::create_char(code_point); }
    inline Value char_from_ascii(char ascii_char) { return NumericWrappers::create_char(ascii_char); }
    
    // Validation functions
    ::meld::types::Result<Value, ::meld::types::ValidationError> validate_uint(const Value& value);
    ::meld::types::Result<Value, ::meld::types::ValidationError> validate_uint(int64_t value);
}

} // namespace meld::stdx