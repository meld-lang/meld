#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <stdexcept>

namespace meld::kernel {

// Cons operations (C++23 with std::expected)
OperationResult car(const Value& cell) {
    auto cons_result = cell.try_as<Cons>();
    if (!cons_result) {
        return std::unexpected(std::format("car: expected cons cell, got {}", cell.to_string()));
    }
    return (*cons_result)->car();
}

OperationResult cdr(const Value& cell) {
    auto cons_result = cell.try_as<Cons>();
    if (!cons_result) {
        return std::unexpected(std::format("cdr: expected cons cell, got {}", cell.to_string()));
    }
    return (*cons_result)->cdr();
}

Value cons(Value car_val, Value cdr_val) {
    return Value(std::make_shared<Cons>(std::move(car_val), std::move(cdr_val)));
}

Value list(const std::vector<Value>& values) {
    Value result = Value(Empty::instance());
    for (auto it = values.rbegin(); it != values.rend(); ++it) {
        result = cons(*it, result);
    }
    return result;
}

// C++23: Return std::expected
std::expected<std::vector<Value>, std::string> list_to_array(const Value& lst) {
    std::vector<Value> result;
    Value current = lst;
    
    while (current.is<Cons>()) {
        auto cons_ptr = current.as<Cons>();
        result.push_back(cons_ptr->car());
        current = cons_ptr->cdr();
    }
    
    if (!current.is<Empty>()) {
        return std::unexpected(std::format("listToArray: improper list ending with {}", 
            current.to_string()));
    }
    
    return result;
}

// Function application (C++23 with std::expected)
OperationResult apply(const Value& fn, const std::vector<Value>& args) {
    auto func_result = fn.try_as<Function>();
    if (!func_result) {
        return std::unexpected(std::format("apply: expected function, got {}", fn.to_string()));
    }
    
    auto func_ptr = *func_result;
    
    // If it's a native function, call the implementation directly
    if (func_ptr->impl()) {
        return (*func_ptr->impl())(args);
    }
    
    // Otherwise, it's a lambda that needs to be evaluated
    return std::unexpected("apply: lambda evaluation not yet implemented");
}

// Symbol operations
std::shared_ptr<Symbol> gensym(const std::string& prefix) {
    return SymbolTable::instance().gensym(prefix);
}

// C++23: Return std::expected
OperationResult symbol_name(const Value& sym) {
    auto sym_result = sym.try_as<Symbol>();
    if (!sym_result) {
        return std::unexpected(std::format("symbol-name: expected symbol, got {}", sym.to_string()));
    }
    return Value(std::make_shared<String>((*sym_result)->name()));
}

// Equality
bool eq(const Value& a, const Value& b) {
    // For symbols, use reference equality (interned)
    if (a.is<Symbol>() && b.is<Symbol>()) {
        return a.as<Symbol>() == b.as<Symbol>();
    }
    
    // Empty is singleton
    if (a.is<Empty>() && b.is<Empty>()) {
        return true;
    }
    
    // Booleans are singletons
    if (a.is<Boolean>() && b.is<Boolean>()) {
        return a.as<Boolean>() == b.as<Boolean>();
    }
    
    // For other types, use pointer equality
    return false; // Different types or different objects
}

bool equal(const Value& a, const Value& b) {
    // Same reference
    if (a.is<Symbol>() && b.is<Symbol>()) {
        return *a.as<Symbol>() == *b.as<Symbol>();
    }
    
    if (a.is<Empty>() && b.is<Empty>()) {
        return true;
    }
    
    if (a.is<Boolean>() && b.is<Boolean>()) {
        return *a.as<Boolean>() == *b.as<Boolean>();
    }
    
    if (a.is<Integer>() && b.is<Integer>()) {
        return *a.as<Integer>() == *b.as<Integer>();
    }
    
    if (a.is<String>() && b.is<String>()) {
        return *a.as<String>() == *b.as<String>();
    }
    
    // Cons cells (recursive structural equality)
    if (a.is<Cons>() && b.is<Cons>()) {
        auto a_cons = a.as<Cons>();
        auto b_cons = b.as<Cons>();
        return equal(a_cons->car(), b_cons->car()) && equal(a_cons->cdr(), b_cons->cdr());
    }
    
    // Optional
    if (a.is<Optional<Value>>() && b.is<Optional<Value>>()) {
        auto a_opt = a.as<Optional<Value>>();
        auto b_opt = b.as<Optional<Value>>();
        
        if (a_opt->is_empty() && b_opt->is_empty()) {
            return true;
        }
        if (a_opt->is_empty() || b_opt->is_empty()) {
            return false;
        }
        return equal(a_opt->get().value(), b_opt->get().value());
    }
    
    return false;
}

// AST construction helpers
Value make_call(const std::string& func_name, const std::vector<Value>& args) {
    std::vector<Value> elements;
    elements.push_back(Value(SymbolTable::instance().intern(func_name)));
    elements.insert(elements.end(), args.begin(), args.end());
    return list(elements);
}

// C++23: Use std::string_view for efficiency
Value make_val_decl(std::string_view name, const Value& value) {
    return list({
        Value(SymbolTable::instance().intern(std::string(name))),
        Value(SymbolTable::instance().intern("val-decl")),
        value
    });
}

Value make_var_decl(std::string_view name, const Value& value) {
    return list({
        Value(SymbolTable::instance().intern(std::string(name))),
        Value(SymbolTable::instance().intern("var-decl")),
        value
    });
}

Value make_int_literal(int64_t value) {
    return list({
        Value(SymbolTable::instance().intern("int-literal")),
        Value(std::make_shared<Integer>(value))
    });
}

Value make_string_literal(std::string_view value) {
    return list({
        Value(SymbolTable::instance().intern("string-literal")),
        Value(std::make_shared<String>(std::string(value)))
    });
}

Value make_bool_literal(bool value) {
    return list({
        Value(SymbolTable::instance().intern("bool-literal")),
        Value(Boolean::from(value))
    });
}

} // namespace meld::kernel
