#include "meld/std/numeric_wrappers.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/types/result.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <map>

// Selective using-declarations to avoid shadowing ::std
using ::meld::meta::TypeRegistry;
using ::meld::meta::Field;
using ::meld::meta::StructMetaType;
using ::meld::meta::RefinementMetaType;
using ::meld::kernel::Value;
using ::meld::kernel::Integer;
using ::meld::kernel::Boolean;
using ::meld::kernel::String;
using ::meld::kernel::Function;
using ::meld::kernel::Symbol;
using ::meld::types::Result;
using ::meld::types::ValidationError;
using ::meld::types::ValidationErrorType;

namespace meld::stdx {

// === Refinement Types (Logical Layer) ===

::std::shared_ptr<RefinementMetaType> NumericWrappers::create_uint_refinement() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();

    Function::NativeImpl uint_pred = [](const ::std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            return Value(::std::make_shared<String>("uint predicate expects exactly one argument"));
        }
        if (!args[0].is<Integer>()) {
            return Value(Boolean::false_value());
        }
        return Value(args[0].as<Integer>()->value() >= 0
                     ? Boolean::true_value() : Boolean::false_value());
    };
    auto predicate = Value(::std::make_shared<Function>(
        ::std::vector<::std::shared_ptr<Symbol>>{},
        Value(),
        uint_pred,
        ::std::string("uint_predicate")));

    return ::std::make_shared<RefinementMetaType>("uint", int_type, predicate);
}

Result<Value, ValidationError> NumericWrappers::validate_uint(const Value& value) {
    if (!value.is<Integer>()) {
        return Result<Value, ValidationError>::error(ValidationError(
            ValidationErrorType::TYPE_MISMATCH,
            "Expected integer value for uint", "int", "non-integer"));
    }
    int64_t iv = value.as<Integer>()->value();
    if (iv < 0) {
        return Result<Value, ValidationError>::error(ValidationError(
            ValidationErrorType::CONSTRAINT_VIOLATION,
            "uint value must be non-negative", "uint (>= 0)", ::std::to_string(iv)));
    }
    return Result<Value, ValidationError>::success(value);
}

Result<Value, ValidationError> NumericWrappers::validate_uint(int64_t value) {
    if (value < 0) {
        return Result<Value, ValidationError>::error(ValidationError(
            ValidationErrorType::CONSTRAINT_VIOLATION,
            "uint value must be non-negative", "uint (>= 0)", ::std::to_string(value)));
    }
    return Result<Value, ValidationError>::success(
        Value(::std::make_shared<Integer>(value)));
}

// === Struct Wrappers (Machine Layer) ===

::std::shared_ptr<StructMetaType> NumericWrappers::create_wrapper_struct(
    const ::std::string& name,
    const ::std::string& cpp_type,
    const ::std::string& java_type,
    const ::std::string& go_type)
{
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();

    ::std::vector<Field> fields = { Field("bits", int_type, false) };
    auto st = ::std::make_shared<StructMetaType>(name, fields);

    st->add_annotation("transpile_as", ::std::map<::std::string, ::std::string>{
        {"cpp", cpp_type}, {"java", java_type}, {"go", go_type}});
    st->add_annotation("value", ::std::map<::std::string, ::std::string>{});
    return st;
}

::std::shared_ptr<StructMetaType> NumericWrappers::create_u8_struct()   { return create_wrapper_struct("u8",  "uint8_t",  "byte",  "uint8"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_u16_struct()  { return create_wrapper_struct("u16", "uint16_t", "short", "uint16"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_u32_struct()  { return create_wrapper_struct("u32", "uint32_t", "int",   "uint32"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_u64_struct()  { return create_wrapper_struct("u64", "uint64_t", "long",  "uint64"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_i8_struct()   { return create_wrapper_struct("i8",  "int8_t",   "byte",  "int8"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_i16_struct()  { return create_wrapper_struct("i16", "int16_t",  "short", "int16"); }
::std::shared_ptr<StructMetaType> NumericWrappers::create_char_struct() { return create_wrapper_struct("char","char32_t", "int",   "rune"); }

::std::shared_ptr<RefinementMetaType> NumericWrappers::create_valid_char_refinement() {
    auto char_struct = create_char_struct();

    // NOTE: StructInstance is not a Value variant alternative, so runtime
    // char validation via Value is not yet possible. This predicate always
    // returns false until a proper tagged-value representation is added.
    Function::NativeImpl char_pred = [](const ::std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            return Value(::std::make_shared<String>("ValidChar predicate expects exactly one argument"));
        }
        // TODO: Implement when StructInstance is representable as Value
        return Value(Boolean::false_value());
    };
    auto predicate = Value(::std::make_shared<Function>(
        ::std::vector<::std::shared_ptr<Symbol>>{},
        Value(),
        char_pred,
        ::std::string("valid_char_predicate")));

    return ::std::make_shared<RefinementMetaType>("ValidChar", char_struct, predicate);
}

::std::shared_ptr<StructMetaType> NumericWrappers::create_buffer_struct() {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();

    ::std::vector<Field> fields = { Field("data", int_type, false) };
    auto st = ::std::make_shared<StructMetaType>("buffer", fields);

    st->add_annotation("transpile_as", ::std::map<::std::string, ::std::string>{
        {"cpp", "std::vector<uint8_t>"}, {"java", "byte[]"}, {"go", "[]byte"}});
    st->add_annotation("value", ::std::map<::std::string, ::std::string>{});
    return st;
}

// === Bitwise Operations ===

Value NumericWrappers::logical_right_shift(const Value& value, int shift_amount) {
    if (!value.is<Integer>())
        return Value(::std::make_shared<String>("Error: logical_right_shift requires integer value"));
    uint64_t uv = static_cast<uint64_t>(value.as<Integer>()->value());
    return Value(::std::make_shared<Integer>(static_cast<int64_t>(uv >> shift_amount)));
}

Value NumericWrappers::bitwise_and(const Value& left, const Value& right) {
    if (!left.is<Integer>() || !right.is<Integer>())
        return Value(::std::make_shared<String>("Error: bitwise_and requires integer values"));
    return Value(::std::make_shared<Integer>(left.as<Integer>()->value() & right.as<Integer>()->value()));
}

Value NumericWrappers::bitwise_or(const Value& left, const Value& right) {
    if (!left.is<Integer>() || !right.is<Integer>())
        return Value(::std::make_shared<String>("Error: bitwise_or requires integer values"));
    return Value(::std::make_shared<Integer>(left.as<Integer>()->value() | right.as<Integer>()->value()));
}

Value NumericWrappers::bitwise_xor(const Value& left, const Value& right) {
    if (!left.is<Integer>() || !right.is<Integer>())
        return Value(::std::make_shared<String>("Error: bitwise_xor requires integer values"));
    return Value(::std::make_shared<Integer>(left.as<Integer>()->value() ^ right.as<Integer>()->value()));
}

Value NumericWrappers::bitwise_not(const Value& value) {
    if (!value.is<Integer>())
        return Value(::std::make_shared<String>("Error: bitwise_not requires integer value"));
    return Value(::std::make_shared<Integer>(~value.as<Integer>()->value()));
}

// === Literal Creation ===
// NOTE: These return Symbol-tagged values representing the struct type,
// since StructInstance is not directly representable in the Value variant.
// The integer value is stored and the type name is encoded in the symbol.

Value NumericWrappers::create_u8(int64_t value) {
    if (!is_valid_u8(value))
        return Value(::std::make_shared<String>("Error: value out of range for u8"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_u16(int64_t value) {
    if (!is_valid_u16(value))
        return Value(::std::make_shared<String>("Error: value out of range for u16"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_u32(int64_t value) {
    if (!is_valid_u32(value))
        return Value(::std::make_shared<String>("Error: value out of range for u32"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_u64(int64_t value) {
    if (!is_valid_u64(value))
        return Value(::std::make_shared<String>("Error: value out of range for u64"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_i8(int64_t value) {
    if (!is_valid_i8(value))
        return Value(::std::make_shared<String>("Error: value out of range for i8"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_i16(int64_t value) {
    if (!is_valid_i16(value))
        return Value(::std::make_shared<String>("Error: value out of range for i16"));
    return Value(::std::make_shared<Integer>(value));
}

Value NumericWrappers::create_char(int64_t code_point) {
    if (!is_valid_unicode_scalar(code_point))
        return Value(::std::make_shared<String>("Error: invalid Unicode code point"));
    return Value(::std::make_shared<Integer>(code_point));
}

Value NumericWrappers::create_char(char ascii_char) {
    return create_char(static_cast<int64_t>(static_cast<unsigned char>(ascii_char)));
}

// === Character Utilities ===
// NOTE: These currently always return false/error because StructInstance
// is not representable in the Value variant. When a tagged-value system
// is added, these should be updated to inspect the struct fields.

bool NumericWrappers::is_digit(const Value& char_value) {
    if (!char_value.is<Integer>()) return false;
    int64_t cp = char_value.as<Integer>()->value();
    return cp >= '0' && cp <= '9';
}

bool NumericWrappers::is_alpha(const Value& char_value) {
    if (!char_value.is<Integer>()) return false;
    int64_t cp = char_value.as<Integer>()->value();
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

bool NumericWrappers::is_alphanumeric(const Value& char_value) {
    return is_alpha(char_value) || is_digit(char_value);
}

bool NumericWrappers::is_whitespace(const Value& char_value) {
    if (!char_value.is<Integer>()) return false;
    int64_t cp = char_value.as<Integer>()->value();
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v';
}

Value NumericWrappers::to_upper(const Value& char_value) {
    if (!char_value.is<Integer>())
        return Value(::std::make_shared<String>("Error: to_upper requires char value"));
    int64_t cp = char_value.as<Integer>()->value();
    if (cp >= 'a' && cp <= 'z') cp = cp - 'a' + 'A';
    return create_char(cp);
}

Value NumericWrappers::to_lower(const Value& char_value) {
    if (!char_value.is<Integer>())
        return Value(::std::make_shared<String>("Error: to_lower requires char value"));
    int64_t cp = char_value.as<Integer>()->value();
    if (cp >= 'A' && cp <= 'Z') cp = cp - 'A' + 'a';
    return create_char(cp);
}

// === Validation Helpers ===

bool NumericWrappers::is_valid_u8(int64_t value)  { return value >= 0 && value <= 255; }
bool NumericWrappers::is_valid_u16(int64_t value) { return value >= 0 && value <= 65535; }
bool NumericWrappers::is_valid_u32(int64_t value) { return value >= 0 && value <= 4294967295LL; }
bool NumericWrappers::is_valid_u64(int64_t value) { return value >= 0; }
bool NumericWrappers::is_valid_i8(int64_t value)  { return value >= -128 && value <= 127; }
bool NumericWrappers::is_valid_i16(int64_t value) { return value >= -32768 && value <= 32767; }

bool NumericWrappers::is_valid_unicode_scalar(int64_t code_point) {
    if (code_point < 0 || code_point > 0x10FFFF) return false;
    if (code_point >= 0xD800 && code_point <= 0xDFFF) return false;
    return true;
}

// === Registration ===

void NumericWrappers::register_all_types() {
    auto& registry = TypeRegistry::instance();
    registry.register_type("uint", create_uint_refinement());
    registry.register_type("ValidChar", create_valid_char_refinement());
    registry.register_type("u8", create_u8_struct());
    registry.register_type("u16", create_u16_struct());
    registry.register_type("u32", create_u32_struct());
    registry.register_type("u64", create_u64_struct());
    registry.register_type("i8", create_i8_struct());
    registry.register_type("i16", create_i16_struct());
    registry.register_type("char", create_char_struct());
    registry.register_type("buffer", create_buffer_struct());
}

void NumericWrappers::register_bitwise_operators() {
    // TODO: Register bitwise operators with the operator registry
}

} // namespace meld::stdx

// === Convenience Functions ===

namespace meld::stdx::numeric {

Result<Value, ValidationError> validate_uint(const Value& value) {
    return NumericWrappers::validate_uint(value);
}

Result<Value, ValidationError> validate_uint(int64_t value) {
    return NumericWrappers::validate_uint(value);
}

} // namespace meld::stdx::numeric