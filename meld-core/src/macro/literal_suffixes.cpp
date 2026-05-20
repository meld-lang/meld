#include "meld/macro/literal_suffixes.hpp"
#include "meld/std/numeric_wrappers.hpp"
#include "meld/kernel/primitives.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace meld::macro;
using namespace meld::kernel;
using namespace meld::stdx;

// === Suffix Detection ===

bool LiteralSuffixMacros::has_u8_suffix(const std::string& token) {
    return ends_with(token, "u8");
}

bool LiteralSuffixMacros::has_u16_suffix(const std::string& token) {
    return ends_with(token, "u16");
}

bool LiteralSuffixMacros::has_u32_suffix(const std::string& token) {
    return ends_with(token, "u32");
}

bool LiteralSuffixMacros::has_u64_suffix(const std::string& token) {
    return ends_with(token, "u64");
}

bool LiteralSuffixMacros::has_i8_suffix(const std::string& token) {
    return ends_with(token, "i8");
}

bool LiteralSuffixMacros::has_i16_suffix(const std::string& token) {
    return ends_with(token, "i16");
}

// === Literal Expansion ===

Value LiteralSuffixMacros::expand_u8_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "u8");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_u8_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for u8"));
        }
        
        return NumericWrappers::create_u8(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing u8 literal: " + std::string(e.what())));
    }
}

Value LiteralSuffixMacros::expand_u16_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "u16");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_u16_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for u16"));
        }
        
        return NumericWrappers::create_u16(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing u16 literal: " + std::string(e.what())));
    }
}

Value LiteralSuffixMacros::expand_u32_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "u32");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_u32_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for u32"));
        }
        
        return NumericWrappers::create_u32(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing u32 literal: " + std::string(e.what())));
    }
}

Value LiteralSuffixMacros::expand_u64_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "u64");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_u64_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for u64"));
        }
        
        return NumericWrappers::create_u64(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing u64 literal: " + std::string(e.what())));
    }
}

Value LiteralSuffixMacros::expand_i8_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "i8");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_i8_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for i8"));
        }
        
        return NumericWrappers::create_i8(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing i8 literal: " + std::string(e.what())));
    }
}

Value LiteralSuffixMacros::expand_i16_literal(const std::string& literal_text) {
    try {
        std::string numeric_part = extract_numeric_part(literal_text, "i16");
        int64_t value = parse_numeric_literal(numeric_part);
        
        if (!validate_i16_range(value)) {
            return Value(std::make_shared<String>("Error: value " + std::to_string(value) + " out of range for i16"));
        }
        
        return NumericWrappers::create_i16(value);
    } catch (const std::exception& e) {
        return Value(std::make_shared<String>("Error parsing i16 literal: " + std::string(e.what())));
    }
}

// === Helper Functions ===

std::string LiteralSuffixMacros::extract_numeric_part(const std::string& token, const std::string& suffix) {
    if (token.length() <= suffix.length()) {
        throw std::invalid_argument("Token too short to contain suffix");
    }
    
    return token.substr(0, token.length() - suffix.length());
}

int64_t LiteralSuffixMacros::parse_numeric_literal(const std::string& numeric_part) {
    std::string clean_part = remove_underscores(numeric_part);
    
    if (clean_part.empty()) {
        throw std::invalid_argument("Empty numeric literal");
    }
    
    // Determine the base and parse accordingly
    if (clean_part.length() >= 2 && clean_part[0] == '0') {
        char second_char = std::tolower(clean_part[1]);
        
        if (second_char == 'x') {
            // Hexadecimal: 0x...
            return parse_hexadecimal(clean_part);
        } else if (second_char == 'b') {
            // Binary: 0b...
            return parse_binary(clean_part);
        } else if (std::isdigit(second_char)) {
            // Octal: 0...
            return parse_octal(clean_part);
        }
    }
    
    // Default to decimal
    return parse_decimal(clean_part);
}

int64_t LiteralSuffixMacros::parse_decimal(const std::string& str) {
    try {
        return std::stoll(str, nullptr, 10);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid decimal literal: " + str);
    }
}

int64_t LiteralSuffixMacros::parse_hexadecimal(const std::string& str) {
    try {
        // Skip the "0x" prefix
        return std::stoll(str.substr(2), nullptr, 16);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid hexadecimal literal: " + str);
    }
}

int64_t LiteralSuffixMacros::parse_binary(const std::string& str) {
    try {
        // Skip the "0b" prefix
        return std::stoll(str.substr(2), nullptr, 2);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid binary literal: " + str);
    }
}

int64_t LiteralSuffixMacros::parse_octal(const std::string& str) {
    try {
        return std::stoll(str, nullptr, 8);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid octal literal: " + str);
    }
}

std::string LiteralSuffixMacros::remove_underscores(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (char c : str) {
        if (c != '_') {
            result += c;
        }
    }
    
    return result;
}

bool LiteralSuffixMacros::ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }
    
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// === Range Validation ===

bool LiteralSuffixMacros::validate_u8_range(int64_t value) {
    return value >= 0 && value <= 255;
}

bool LiteralSuffixMacros::validate_u16_range(int64_t value) {
    return value >= 0 && value <= 65535;
}

bool LiteralSuffixMacros::validate_u32_range(int64_t value) {
    return value >= 0 && value <= 4294967295LL;
}

bool LiteralSuffixMacros::validate_u64_range(int64_t value) {
    return value >= 0; // For u64, we can't represent the full range in int64_t
}

bool LiteralSuffixMacros::validate_i8_range(int64_t value) {
    return value >= -128 && value <= 127;
}

bool LiteralSuffixMacros::validate_i16_range(int64_t value) {
    return value >= -32768 && value <= 32767;
}

// === Registration ===

void LiteralSuffixMacros::register_all_macros() {
    // TODO: Register with the actual macro system when it's available
    // For now, this is a placeholder that would integrate with the MMS
    
    // The macro system would register these patterns:
    // - Pattern: /\d+u8$/ -> expand_u8_literal
    // - Pattern: /\d+u16$/ -> expand_u16_literal
    // - Pattern: /\d+u32$/ -> expand_u32_literal
    // - Pattern: /\d+u64$/ -> expand_u64_literal
    // - Pattern: /\d+i8$/ -> expand_i8_literal
    // - Pattern: /\d+i16$/ -> expand_i16_literal
}

// === Macro Expansion Functions ===

namespace meld::macro::literal_macros {

Value expand_u8(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: u8 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_u8_literal(literal_text);
}

Value expand_u16(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: u16 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_u16_literal(literal_text);
}

Value expand_u32(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: u32 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_u32_literal(literal_text);
}

Value expand_u64(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: u64 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_u64_literal(literal_text);
}

Value expand_i8(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: i8 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_i8_literal(literal_text);
}

Value expand_i16(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].is<String>()) {
        return Value(std::make_shared<String>("Error: i16 macro expects one string argument"));
    }
    
    std::string literal_text = args[0].as<String>()->value();
    return LiteralSuffixMacros::expand_i16_literal(literal_text);
}

} // namespace meld::macro::literal_macros