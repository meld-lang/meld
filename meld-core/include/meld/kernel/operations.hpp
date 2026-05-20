#pragma once

#include "primitives.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <expected>
#include <ranges>
#include <memory>

// Forward declaration
namespace meld::meta {
    class MetaType;
}

namespace meld::kernel {

// C++23: Use std::expected for error handling
using OperationResult = std::expected<Value, std::string>;

// Cons operations (C++23 with std::expected)
OperationResult car(const Value& cell);
OperationResult cdr(const Value& cell);
Value cons(Value car, Value cdr);

// C++23: Use ranges for list construction
Value list(std::ranges::input_range auto&& values) {
    Value result = Value(Empty::instance());
    for (auto&& val : values | std::views::reverse) {
        result = cons(val, result);
    }
    return result;
}

Value list(const std::vector<Value>& values);
std::expected<std::vector<Value>, std::string> list_to_array(const Value& lst);

// Function application
OperationResult apply(const Value& fn, const std::vector<Value>& args);

// Symbol operations
std::shared_ptr<Symbol> gensym(const std::string& prefix = "G");
OperationResult symbol_name(const Value& sym);

// Type query - returns MetaType instances
std::shared_ptr<meta::MetaType> type_of(const Value& value);

// Equality (C++23 with spaceship operator support)
bool eq(const Value& a, const Value& b);
bool equal(const Value& a, const Value& b);

// AST construction helpers (C++23 with std::format)
Value make_call(std::string_view func_name, std::ranges::input_range auto&& args) {
    std::vector<Value> elements;
    elements.push_back(Value(SymbolTable::instance().intern(std::string(func_name))));
    std::ranges::copy(args, std::back_inserter(elements));
    return list(elements);
}

Value make_call(const std::string& func_name, const std::vector<Value>& args);
Value make_val_decl(std::string_view name, const Value& value);
Value make_var_decl(std::string_view name, const Value& value);
Value make_int_literal(int64_t value);
Value make_string_literal(std::string_view value);
Value make_bool_literal(bool value);

} // namespace meld::kernel
