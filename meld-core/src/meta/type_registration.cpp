#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <iostream>

// RTTR registration is disabled on MSVC and Apple Clang due to incompatible template
// instantiations. MSVC has issues in rttr/detail/registration/bind_impl.h, and
// Apple Clang has issues with C++23 deducing-this member functions and incomplete
// rttr::argument types.
// The RttrTypeRegistry still provides a functional API via rttr::type
// queries, but the RTTR_REGISTRATION block is only compiled on
// compatible compilers.
#if !defined(_MSC_VER) && !defined(__APPLE__)
#include <rttr/registration>

using namespace rttr;
using namespace meld::kernel;
using namespace meld::meta;

// RTTR Registration Block
// This block is executed automatically when the library is loaded
RTTR_REGISTRATION
{
    std::cout << "[RTTR] Registering Meld types..." << std::endl;
    
    // Register primitive types
    registration::class_<Symbol>("Symbol")
        .constructor<std::string>()
        .property("name", &Symbol::name)
        .method("to_string", &Symbol::to_string)
        .method("hash", &Symbol::hash);
    
    registration::class_<Integer>("Integer")
        .constructor<int64_t>()
        .property("value", &Integer::value)
        .method("to_string", &Integer::to_string);
    
    registration::class_<Boolean>("Boolean")
        .constructor<bool>()
        .property("value", &Boolean::value)
        .method("to_string", &Boolean::to_string);
    
    registration::class_<String>("String")
        .constructor<std::string>()
        .property("value", &String::value)
        .method("to_string", &String::to_string)
        .method("length", &String::length);
    
    registration::class_<Function>("Function")
        .constructor<std::vector<std::shared_ptr<Symbol>>, Value>()
        .property_readonly("parameters", &Function::parameters)
        .property_readonly("return_type", &Function::return_type)
        .property_readonly("name", &Function::name)
        .method("to_string", &Function::to_string)
        .method("arity", &Function::arity);
    
    // Register MetaType hierarchy
    registration::class_<MetaType>("MetaType")
        .method("name", &MetaType::name)
        .method("size", &MetaType::size)
        .method("is_value_type", &MetaType::is_value_type)
        .method("type_name", &MetaType::type_name)
        .method("is_assignable_from", &MetaType::is_assignable_from)
        .method("is_subtype_of", &MetaType::is_subtype_of);
    
    registration::class_<PrimitiveMetaType>("PrimitiveMetaType")
        .constructor<std::string, size_t>()
        (
            rttr::policy::ctor::as_raw_ptr
        );
    
    registration::class_<StructMetaType>("StructMetaType")
        .constructor<std::string, std::vector<Field>>()
        (
            rttr::policy::ctor::as_raw_ptr
        )
        .method("fields", &StructMetaType::fields);
    
    registration::class_<ClassMetaType>("ClassMetaType")
        .constructor<std::string, std::vector<Field>, std::vector<Method>>()
        (
            rttr::policy::ctor::as_raw_ptr
        )
        .method("fields", &ClassMetaType::fields)
        .method("methods", &ClassMetaType::methods);
    
    registration::class_<TraitMetaType>("TraitMetaType")
        .constructor<std::string, std::vector<Method>>()
        (
            rttr::policy::ctor::as_raw_ptr
        )
        .method("methods", &TraitMetaType::methods);
    
    registration::class_<UnionMetaType>("UnionMetaType")
        .constructor<std::vector<std::shared_ptr<MetaType>>>()
        (
            rttr::policy::ctor::as_raw_ptr
        )
        .method("types", &UnionMetaType::types)
        .method("contains_type", &UnionMetaType::contains_type);
    
    registration::class_<IntersectionMetaType>("IntersectionMetaType")
        .constructor<std::vector<std::shared_ptr<MetaType>>>()
        (
            rttr::policy::ctor::as_raw_ptr
        )
        .method("types", &IntersectionMetaType::types)
        .method("satisfies_all", &IntersectionMetaType::satisfies_all);
    
    // Register Field and Method helper types
    registration::class_<Field>("Field")
        .constructor<std::string, std::shared_ptr<MetaType>>()
        .property("name", &Field::name)
        .property("type", &Field::type)
        .property("is_mutable", &Field::is_mutable);
    
    registration::class_<Method>("Method")
        .constructor<std::string, std::vector<std::shared_ptr<MetaType>>, std::shared_ptr<MetaType>>()
        .property("name", &Method::name)
        .property("param_types", &Method::param_types)
        .property("return_type", &Method::return_type);
    
    std::cout << "[RTTR] Meld type registration complete." << std::endl;
}

#endif // !_MSC_VER && !__APPLE__

namespace meld::meta {

// RttrTypeRegistry implementation
RttrTypeRegistry& RttrTypeRegistry::instance() {
    static RttrTypeRegistry instance;
    return instance;
}

void RttrTypeRegistry::initialize() {
    if (initialized_) {
        return; // Already initialized
    }
    
    std::cout << "[RttrTypeRegistry] Initializing Meld type registry..." << std::endl;
    
#if !defined(_MSC_VER) && !defined(__APPLE__)
    // RTTR registration happens automatically via RTTR_REGISTRATION
    // We just need to mark as initialized and verify registration worked
    
    // Verify some key types are registered
    std::vector<std::string> required_types = {
        "Symbol", "Integer", "Boolean", "String", "Function",
        "MetaType", "PrimitiveMetaType", "StructMetaType", "ClassMetaType",
        "TraitMetaType", "UnionMetaType", "IntersectionMetaType",
        "Field", "Method"
    };
    
    size_t registered_count = 0;
    for (const auto& type_name : required_types) {
        if (is_registered(type_name)) {
            registered_count++;
        } else {
            std::cerr << "[RttrTypeRegistry] Warning: Type '" << type_name << "' not registered" << std::endl;
        }
    }
    
    std::cout << "[RttrTypeRegistry] Registered " << registered_count << "/" << required_types.size() 
              << " required types" << std::endl;
#else
    std::cout << "[RttrTypeRegistry] RTTR registration disabled on this platform" << std::endl;
#endif
    
    initialized_ = true;
    
    std::cout << "[RttrTypeRegistry] Initialization complete. Total types: " 
              << get_type_count() << std::endl;
}

bool RttrTypeRegistry::is_registered(const std::string& name) const {
    rttr::type type = rttr::type::get_by_name(name);
    return type.is_valid();
}

rttr::type RttrTypeRegistry::get_type(const std::string& name) const {
    return rttr::type::get_by_name(name);
}

std::vector<std::string> RttrTypeRegistry::get_all_type_names() const {
    std::vector<std::string> names;
    
    for (auto& type : rttr::type::get_types()) {
        if (type.is_class()) {
            names.push_back(type.get_name().to_string());
        }
    }
    
    return names;
}

size_t RttrTypeRegistry::get_type_count() const {
    size_t count = 0;
    
    for (auto& type : rttr::type::get_types()) {
        if (type.is_class()) {
            count++;
        }
    }
    
    return count;
}

// Convenience function
void initialize_meld_types() {
    RttrTypeRegistry::instance().initialize();
}

} // namespace meld::meta
