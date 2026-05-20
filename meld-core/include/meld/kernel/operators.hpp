#pragma once

#include "primitives.hpp"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <expected>

namespace meld::kernel {

// Operator types
enum class OperatorType {
    PREFIX,   // Unary prefix: -x, !x
    INFIX,    // Binary infix: x + y, x * y
    POSTFIX   // Unary postfix: x++, x!
};

// Operator associativity
enum class Associativity {
    LEFT,     // Left-to-right: a + b + c = (a + b) + c
    RIGHT,    // Right-to-left: a = b = c means a = (b = c)
    NONE      // Non-associative: a < b < c is invalid
};

// Operator metadata
struct OperatorInfo {
    std::string symbol;
    OperatorType type;
    int precedence;              // Higher = tighter binding
    Associativity associativity;
    
    OperatorInfo() : type(OperatorType::INFIX), precedence(0), associativity(Associativity::LEFT) {}
    
    OperatorInfo(std::string sym, OperatorType t, int prec, Associativity assoc)
        : symbol(std::move(sym)), type(t), precedence(prec), associativity(assoc) {}
};

// Operator function signature
using OperatorFunction = std::function<std::expected<Value, std::string>(const std::vector<Value>&)>;

// Operator registry for managing operator definitions
class OperatorRegistry {
public:
    static OperatorRegistry& instance();
    
    // Register a built-in operator
    void register_builtin_operator(const OperatorInfo& info, OperatorFunction func);
    
    // Register an operator overload for a specific type
    void register_operator_overload(
        const std::string& symbol,
        const std::string& type_name,
        OperatorFunction func
    );
    
    // Register an operator overload with full type signature
    void register_operator_overload(
        const std::string& symbol,
        const std::vector<std::string>& type_signature,
        OperatorFunction func
    );
    
    // Register a custom operator (user-defined)
    void register_custom_operator(const OperatorInfo& info);
    
    // Lookup operator information
    std::expected<OperatorInfo, std::string> get_operator_info(const std::string& symbol) const;
    
    // Dispatch operator call
    std::expected<Value, std::string> dispatch_operator(
        const std::string& symbol,
        const std::vector<Value>& operands
    );
    
    // Check if an operator exists
    bool has_operator(const std::string& symbol) const;
    
    // Get all registered operators
    std::vector<OperatorInfo> get_all_operators() const;
    
private:
    OperatorRegistry();
    
    // Built-in operators (symbol -> info)
    std::map<std::string, OperatorInfo> builtin_operators_;
    
    // Built-in operator implementations (symbol -> function)
    std::map<std::string, OperatorFunction> builtin_functions_;
    
    // Operator overloads (symbol -> type_name -> function)
    std::map<std::string, std::map<std::string, OperatorFunction>> overloads_;
    
    // Operator overloads with full type signatures (symbol -> signature -> function)
    std::map<std::string, std::map<std::vector<std::string>, OperatorFunction>> signature_overloads_;
    
    // Custom operators (symbol -> info)
    std::map<std::string, OperatorInfo> custom_operators_;
    
    // Initialize built-in operators
    void init_builtin_operators();
    
    // Helper to get type name from value
    std::string get_type_name(const Value& value) const;
};

// Built-in operator implementations
namespace operators {
    // Arithmetic operators
    std::expected<Value, std::string> add(const std::vector<Value>& args);
    std::expected<Value, std::string> subtract(const std::vector<Value>& args);
    std::expected<Value, std::string> multiply(const std::vector<Value>& args);
    std::expected<Value, std::string> divide(const std::vector<Value>& args);
    std::expected<Value, std::string> modulo(const std::vector<Value>& args);
    std::expected<Value, std::string> negate(const std::vector<Value>& args);
    
    // Comparison operators
    std::expected<Value, std::string> less_than(const std::vector<Value>& args);
    std::expected<Value, std::string> less_equal(const std::vector<Value>& args);
    std::expected<Value, std::string> greater_than(const std::vector<Value>& args);
    std::expected<Value, std::string> greater_equal(const std::vector<Value>& args);
    std::expected<Value, std::string> equals(const std::vector<Value>& args);
    std::expected<Value, std::string> not_equals(const std::vector<Value>& args);
    
    // Logical operators
    std::expected<Value, std::string> logical_and(const std::vector<Value>& args);
    std::expected<Value, std::string> logical_or(const std::vector<Value>& args);
    std::expected<Value, std::string> logical_not(const std::vector<Value>& args);
}

} // namespace meld::kernel
