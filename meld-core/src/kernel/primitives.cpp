#include "meld/kernel/primitives.hpp"
#include <sstream>

namespace meld::kernel {

std::string Value::to_string() const {
    return std::visit([](const auto& ptr) -> std::string {
        if (!ptr) return "<null>";
        if constexpr (std::is_same_v<std::decay_t<decltype(ptr)>,
                                     std::shared_ptr<types::StructInstance>>) {
            return "<struct-instance>";
        } else {
            return ptr->to_string();
        }
    }, value_);
}

bool Value::is_truthy() const {
    // False and Empty are falsy
    if (is<Boolean>()) {
        return as<Boolean>()->value();
    }
    if (is<Empty>()) {
        return false;
    }
    if (is<Optional<Value>>()) {
        return as<Optional<Value>>()->is_some();
    }
    return true; // Everything else is truthy
}

std::string Cons::to_string() const {
    std::ostringstream oss;
    oss << "(";
    
    std::vector<std::string> elements;
    const Cons* current = this;
    Value current_val = Value(std::make_shared<Cons>(*this));
    
    while (current_val.is<Cons>()) {
        auto cons_ptr = current_val.as<Cons>();
        elements.push_back(cons_ptr->car().to_string());
        current_val = cons_ptr->cdr();
    }
    
    // Check if it's a proper list (ends with Empty)
    if (current_val.is<Empty>()) {
        // Proper list
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) oss << " ";
            oss << elements[i];
        }
    } else {
        // Improper list (dotted pair)
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) oss << " ";
            oss << elements[i];
        }
        oss << " . " << current_val.to_string();
    }
    
    oss << ")";
    return oss.str();
}

} // namespace meld::kernel
