#include "meld/mms/meta_system.hpp"

namespace meld::mms {

MetaSystem::MetaSystem() {
    load_builtins();
}

void MetaSystem::register_kind(const std::string& name, const TypeKind& kind) {
    kinds_[name] = kind;
}

bool MetaSystem::has_kind(const std::string& name) const {
    return kinds_.count(name) > 0;
}

const TypeKind& MetaSystem::get_kind(const std::string& name) const {
    return kinds_.at(name);
}

ExpansionResult MetaSystem::expand(const std::string& kind_name,
                                    const std::string& type_name,
                                    const parser::ast::expression& /*body*/) {
    ExpansionResult result;
    result.type_name = type_name;
    result.kind_name = kind_name;
    if (has_kind(kind_name)) {
        result.kind = get_kind(kind_name);
    }
    return result;
}

void MetaSystem::load_builtins() {
    // class — reference semantics, ARC-managed
    TypeKind class_kind;
    class_kind.name = "class";
    class_kind.metadata = {
        {"semantics", "reference"}, {"memory", "arc"},
        {"copyable", "false"}, {"fields_allowed", "true"}, {"methods_allowed", "true"}
    };
    kinds_["class"] = class_kind;

    // struct — value semantics, stack-allocated
    TypeKind struct_kind;
    struct_kind.name = "struct";
    struct_kind.metadata = {
        {"semantics", "value"}, {"memory", "stack"},
        {"copyable", "true"}, {"fields_allowed", "true"}, {"methods_allowed", "true"}
    };
    kinds_["struct"] = struct_kind;

    // enum — tagged union, value semantics
    TypeKind enum_kind;
    enum_kind.name = "enum";
    enum_kind.metadata = {
        {"semantics", "value"}, {"memory", "stack"},
        {"copyable", "true"}, {"variants_allowed", "true"}
    };
    kinds_["enum"] = enum_kind;

    // trait — interface definition, no storage
    TypeKind trait_kind;
    trait_kind.name = "trait";
    trait_kind.metadata = {
        {"semantics", "interface"}, {"memory", "none"},
        {"methods_allowed", "true"}, {"fields_allowed", "false"}
    };
    kinds_["trait"] = trait_kind;

    // effect — algebraic effect definition
    TypeKind effect_kind;
    effect_kind.name = "effect";
    effect_kind.metadata = {
        {"semantics", "effect"}, {"memory", "none"},
        {"operations_allowed", "true"}
    };
    kinds_["effect"] = effect_kind;
}

} // namespace meld::mms
