#include "meld/kernel/operators.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <format>

namespace meld::kernel {

// Singleton instance
OperatorRegistry& OperatorRegistry::instance() {
    static OperatorRegistry registry;
    return registry;
}

OperatorRegistry::OperatorRegistry() {
    init_builtin_operators();
}

void OperatorRegistry::init_builtin_operators() {
    // Arithmetic operators (precedence follows C/C++ conventions)
    register_builtin_operator(
        OperatorInfo("+", OperatorType::INFIX, 60, Associativity::LEFT),
        operators::add
    );
    register_builtin_operator(
        OperatorInfo("-", OperatorType::INFIX, 60, Associativity::LEFT),
        operators::subtract
    );
    register_builtin_operator(
        OperatorInfo("*", OperatorType::INFIX, 70, Associativity::LEFT),
        operators::multiply
    );
    register_builtin_operator(
        OperatorInfo("/", OperatorType::INFIX, 70, Associativity::LEFT),
        operators::divide
    );
    register_builtin_operator(
        OperatorInfo("%", OperatorType::INFIX, 70, Associativity::LEFT),
        operators::modulo
    );
    register_builtin_operator(
        OperatorInfo("-", OperatorType::PREFIX, 80, Associativity::RIGHT),
        operators::negate
    );
    
    // Comparison operators
    register_builtin_operator(
        OperatorInfo("<", OperatorType::INFIX, 50, Associativity::LEFT),
        operators::less_than
    );
    register_builtin_operator(
        OperatorInfo("<=", OperatorType::INFIX, 50, Associativity::LEFT),
        operators::less_equal
    );
    register_builtin_operator(
        OperatorInfo(">", OperatorType::INFIX, 50, Associativity::LEFT),
        operators::greater_than
    );
    register_builtin_operator(
        OperatorInfo(">=", OperatorType::INFIX, 50, Associativity::LEFT),
        operators::greater_equal
    );
    register_builtin_operator(
        OperatorInfo("==", OperatorType::INFIX, 40, Associativity::LEFT),
        operators::equals
    );
    register_builtin_operator(
        OperatorInfo("!=", OperatorType::INFIX, 40, Associativity::LEFT),
        operators::not_equals
    );
    
    // Logical operators
    register_builtin_operator(
        OperatorInfo("&&", OperatorType::INFIX, 30, Associativity::LEFT),
        operators::logical_and
    );
    register_builtin_operator(
        OperatorInfo("||", OperatorType::INFIX, 20, Associativity::LEFT),
        operators::logical_or
    );
    register_builtin_operator(
        OperatorInfo("!", OperatorType::PREFIX, 80, Associativity::RIGHT),
        operators::logical_not
    );
}

void OperatorRegistry::register_builtin_operator(const OperatorInfo& info, OperatorFunction func) {
    builtin_operators_[info.symbol] = info;
    builtin_functions_[info.symbol] = std::move(func);
}

void OperatorRegistry::register_operator_overload(
    const std::string& symbol,
    const std::string& type_name,
    OperatorFunction func
) {
    overloads_[symbol][type_name] = std::move(func);
}

void OperatorRegistry::register_operator_overload(
    const std::string& symbol,
    const std::vector<std::string>& type_signature,
    OperatorFunction func
) {
    signature_overloads_[symbol][type_signature] = std::move(func);
}

void OperatorRegistry::register_custom_operator(const OperatorInfo& info) {
    custom_operators_[info.symbol] = info;
}

std::expected<OperatorInfo, std::string> OperatorRegistry::get_operator_info(
    const std::string& symbol
) const {
    // Check custom operators first
    if (auto it = custom_operators_.find(symbol); it != custom_operators_.end()) {
        return it->second;
    }
    
    // Then check built-in operators
    if (auto it = builtin_operators_.find(symbol); it != builtin_operators_.end()) {
        return it->second;
    }
    
    return std::unexpected(std::format("Unknown operator: {}", symbol));
}

std::expected<Value, std::string> OperatorRegistry::dispatch_operator(
    const std::string& symbol,
    const std::vector<Value>& operands
) {
    if (operands.empty()) {
        return std::unexpected(std::format("Operator {} requires at least one operand", symbol));
    }
    
    // Build type signature for all operands
    std::vector<std::string> type_signature;
    for (const auto& operand : operands) {
        type_signature.push_back(get_type_name(operand));
    }
    
    // Check for exact type signature match first
    if (auto op_it = signature_overloads_.find(symbol); op_it != signature_overloads_.end()) {
        if (auto sig_it = op_it->second.find(type_signature); sig_it != op_it->second.end()) {
            return sig_it->second(operands);
        }
    }
    
    // Fall back to single-type dispatch (for backward compatibility)
    std::string type_name = get_type_name(operands[0]);
    
    // Check for type-specific overload
    if (auto op_it = overloads_.find(symbol); op_it != overloads_.end()) {
        if (auto type_it = op_it->second.find(type_name); type_it != op_it->second.end()) {
            return type_it->second(operands);
        }
    }
    
    // Fall back to built-in implementation
    if (auto it = builtin_functions_.find(symbol); it != builtin_functions_.end()) {
        return it->second(operands);
    }
    
    // Check for prefix variant (e.g., unary -)
    std::string prefix_key = symbol + "_prefix";
    if (operands.size() == 1) {
        if (auto it = builtin_functions_.find(prefix_key); it != builtin_functions_.end()) {
            return it->second(operands);
        }
    }
    
    return std::unexpected(std::format("No implementation found for operator {} on types [{}]", 
        symbol, [&]() {
            std::string result;
            for (size_t i = 0; i < type_signature.size(); ++i) {
                if (i > 0) result += ", ";
                result += type_signature[i];
            }
            return result;
        }()));
}

bool OperatorRegistry::has_operator(const std::string& symbol) const {
    return builtin_operators_.contains(symbol) || custom_operators_.contains(symbol);
}

std::vector<OperatorInfo> OperatorRegistry::get_all_operators() const {
    std::vector<OperatorInfo> result;
    
    for (const auto& [_, info] : builtin_operators_) {
        result.push_back(info);
    }
    
    for (const auto& [_, info] : custom_operators_) {
        result.push_back(info);
    }
    
    return result;
}

std::string OperatorRegistry::get_type_name(const Value& value) const {
    if (value.is<Integer>()) return "Int";
    if (value.is<Boolean>()) return "Bool";
    if (value.is<String>()) return "String";
    if (value.is<Symbol>()) return "Symbol";
    if (value.is<Cons>()) return "Cons";
    if (value.is<Function>()) return "Function";
    return "Unknown";
}

// Built-in operator implementations
namespace operators {

std::expected<Value, std::string> add(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator + requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator + requires integer operands");
    }
    
    return Value(std::make_shared<Integer>((*a)->value() + (*b)->value()));
}

std::expected<Value, std::string> subtract(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator - requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator - requires integer operands");
    }
    
    return Value(std::make_shared<Integer>((*a)->value() - (*b)->value()));
}

std::expected<Value, std::string> multiply(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator * requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator * requires integer operands");
    }
    
    return Value(std::make_shared<Integer>((*a)->value() * (*b)->value()));
}

std::expected<Value, std::string> divide(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator / requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator / requires integer operands");
    }
    
    if ((*b)->value() == 0) {
        return std::unexpected("Division by zero");
    }
    
    return Value(std::make_shared<Integer>((*a)->value() / (*b)->value()));
}

std::expected<Value, std::string> modulo(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator % requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator % requires integer operands");
    }
    
    if ((*b)->value() == 0) {
        return std::unexpected("Modulo by zero");
    }
    
    return Value(std::make_shared<Integer>((*a)->value() % (*b)->value()));
}

std::expected<Value, std::string> negate(const std::vector<Value>& args) {
    if (args.size() != 1) {
        return std::unexpected("Operator - (unary) requires exactly 1 operand");
    }
    
    auto a = args[0].try_as<Integer>();
    
    if (!a) {
        return std::unexpected("Operator - (unary) requires integer operand");
    }
    
    return Value(std::make_shared<Integer>(-(*a)->value()));
}

std::expected<Value, std::string> less_than(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator < requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator < requires integer operands");
    }
    
    return Value(Boolean::from((*a)->value() < (*b)->value()));
}

std::expected<Value, std::string> less_equal(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator <= requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator <= requires integer operands");
    }
    
    return Value(Boolean::from((*a)->value() <= (*b)->value()));
}

std::expected<Value, std::string> greater_than(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator > requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator > requires integer operands");
    }
    
    return Value(Boolean::from((*a)->value() > (*b)->value()));
}

std::expected<Value, std::string> greater_equal(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator >= requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Integer>();
    auto b = args[1].try_as<Integer>();
    
    if (!a || !b) {
        return std::unexpected("Operator >= requires integer operands");
    }
    
    return Value(Boolean::from((*a)->value() >= (*b)->value()));
}

std::expected<Value, std::string> equals(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator == requires exactly 2 operands");
    }
    
    // Use structural equality from operations.hpp
    return Value(Boolean::from(equal(args[0], args[1])));
}

std::expected<Value, std::string> not_equals(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator != requires exactly 2 operands");
    }
    
    // Use structural equality from operations.hpp
    return Value(Boolean::from(!equal(args[0], args[1])));
}

std::expected<Value, std::string> logical_and(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator && requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Boolean>();
    auto b = args[1].try_as<Boolean>();
    
    if (!a || !b) {
        return std::unexpected("Operator && requires boolean operands");
    }
    
    return Value(Boolean::from((*a)->value() && (*b)->value()));
}

std::expected<Value, std::string> logical_or(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return std::unexpected("Operator || requires exactly 2 operands");
    }
    
    auto a = args[0].try_as<Boolean>();
    auto b = args[1].try_as<Boolean>();
    
    if (!a || !b) {
        return std::unexpected("Operator || requires boolean operands");
    }
    
    return Value(Boolean::from((*a)->value() || (*b)->value()));
}

std::expected<Value, std::string> logical_not(const std::vector<Value>& args) {
    if (args.size() != 1) {
        return std::unexpected("Operator ! requires exactly 1 operand");
    }
    
    auto a = args[0].try_as<Boolean>();
    
    if (!a) {
        return std::unexpected("Operator ! requires boolean operand");
    }
    
    return Value(Boolean::from(!(*a)->value()));
}

} // namespace operators

} // namespace meld::kernel
