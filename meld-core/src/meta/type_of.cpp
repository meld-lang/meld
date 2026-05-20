#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/meta/metatype.hpp"

namespace meld::kernel {

// Type query - returns MetaType instances
// Implementation lives in meta target to avoid circular dependency (kernel -> meta)
std::shared_ptr<meta::MetaType> type_of(const Value& value) {
    auto& registry = meta::TypeRegistry::instance();
    
    if (value.is<Symbol>()) {
        auto result = registry.get_type("Symbol");
        return result.value_or(meta::MetaType::create_primitive("Symbol", sizeof(void*)));
    }
    if (value.is<Cons>()) {
        return meta::MetaType::create_primitive("Cons", sizeof(void*));
    }
    if (value.is<Empty>()) {
        return meta::MetaType::create_primitive("Empty", 0);
    }
    if (value.is<Function>()) {
        auto result = registry.get_type("Function");
        return result.value_or(meta::MetaType::create_primitive("Function", sizeof(void*)));
    }
    if (value.is<Integer>()) {
        return registry.get_int_type();
    }
    if (value.is<Boolean>()) {
        return registry.get_bool_type();
    }
    if (value.is<String>()) {
        return registry.get_string_type();
    }
    if (value.is<Optional<Value>>()) {
        return meta::MetaType::create_primitive("Optional", sizeof(void*));
    }
    return meta::MetaType::create_primitive("Unknown", 0);
}

} // namespace meld::kernel
