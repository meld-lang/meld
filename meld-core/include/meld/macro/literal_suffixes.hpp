#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <string>
#include <memory>

namespace meld::macro {

/**
 * Literal suffix macros for numeric types.
 * 
 * Implements Requirement 47.6: Support literal suffixes for typed integers via macros
 * 
 * Examples:
 * - 255u8 expands to u8(255)
 * - 1000u16 expands to u16(1000)
 * - 0xDEAD_BEEF_u32 expands to u32(0xDEADBEEF)
 * - 100i8 expands to i8(100)
 * - 30000i16 expands to i16(30000)
 */

class LiteralSuffixMacros {
public:
    /**
     * Register all literal suffix macros with the macro system
     */
    static void register_all_macros();
    
    /**
     * Parse and expand literal suffix macros
     */
    static kernel::Value expand_u8_literal(const std::string& literal_text);
    static kernel::Value expand_u16_literal(const std::string& literal_text);
    static kernel::Value expand_u32_literal(const std::string& literal_text);
    static kernel::Value expand_u64_literal(const std::string& literal_text);
    static kernel::Value expand_i8_literal(const std::string& literal_text);
    static kernel::Value expand_i16_literal(const std::string& literal_text);
    
    /**
     * Check if a token has a numeric literal suffix
     */
    static bool has_u8_suffix(const std::string& token);
    static bool has_u16_suffix(const std::string& token);
    static bool has_u32_suffix(const std::string& token);
    static bool has_u64_suffix(const std::string& token);
    static bool has_i8_suffix(const std::string& token);
    static bool has_i16_suffix(const std::string& token);
    
    /**
     * Extract the numeric part from a suffixed literal
     */
    static std::string extract_numeric_part(const std::string& token, const std::string& suffix);
    
    /**
     * Parse numeric literal (supports decimal, hex, binary, octal)
     */
    static int64_t parse_numeric_literal(const std::string& numeric_part);
    
    /**
     * Validate that a literal value fits in the target type
     */
    static bool validate_u8_range(int64_t value);
    static bool validate_u16_range(int64_t value);
    static bool validate_u32_range(int64_t value);
    static bool validate_u64_range(int64_t value);
    static bool validate_i8_range(int64_t value);
    static bool validate_i16_range(int64_t value);
    
private:
    /**
     * Helper functions for parsing different number formats
     */
    static int64_t parse_decimal(const std::string& str);
    static int64_t parse_hexadecimal(const std::string& str);
    static int64_t parse_binary(const std::string& str);
    static int64_t parse_octal(const std::string& str);
    
    /**
     * Remove underscores from numeric literals (e.g., 0xDEAD_BEEF -> 0xDEADBEEF)
     */
    static std::string remove_underscores(const std::string& str);
    
    /**
     * Check if string ends with suffix (case-sensitive)
     */
    static bool ends_with(const std::string& str, const std::string& suffix);
};

/**
 * Macro expansion functions that will be registered with the macro system
 */
namespace literal_macros {
    
    // These functions will be called by the macro expander when it encounters
    // literals with the corresponding suffixes
    
    kernel::Value expand_u8(const std::vector<kernel::Value>& args);
    kernel::Value expand_u16(const std::vector<kernel::Value>& args);
    kernel::Value expand_u32(const std::vector<kernel::Value>& args);
    kernel::Value expand_u64(const std::vector<kernel::Value>& args);
    kernel::Value expand_i8(const std::vector<kernel::Value>& args);
    kernel::Value expand_i16(const std::vector<kernel::Value>& args);
}

} // namespace meld::macro