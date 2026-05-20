#pragma once

#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <expected>

namespace meld::meta {

/**
 * DynamicTypeBuilder - Builder for creating types at runtime
 * 
 * This class provides a fluent API for dynamically creating classes,
 * structs, and traits at runtime. The created types are registered
 * with the MetaType system and can be used like any other type.
 */
class DynamicTypeBuilder {
public:
    /**
     * Start building a new class type
     * 
     * @param name Class name
     * @return Builder instance
     */
    static DynamicTypeBuilder createClass(std::string name);
    
    /**
     * Start building a new struct type
     * 
     * @param name Struct name
     * @return Builder instance
     */
    static DynamicTypeBuilder createStruct(std::string name);
    
    /**
     * Start building a new trait type
     * 
     * @param name Trait name
     * @return Builder instance
     */
    static DynamicTypeBuilder createTrait(std::string name);
    
    /**
     * Add a field to the type
     * 
     * @param name Field name
     * @param type Field type
     * @param is_mutable Whether the field is mutable
     * @return This builder for chaining
     */
    DynamicTypeBuilder& addField(std::string name, std::shared_ptr<MetaType> type, bool is_mutable = false);
    
    /**
     * Add a property to the type
     * 
     * @param name Property name
     * @param type Property type
     * @param is_mutable Whether the property is mutable
     * @return This builder for chaining
     */
    DynamicTypeBuilder& addProperty(std::string name, std::shared_ptr<MetaType> type, bool is_mutable = false);
    
    /**
     * Add a property with custom getter
     * 
     * @param name Property name
     * @param type Property type
     * @param getter Getter function
     * @return This builder for chaining
     */
    DynamicTypeBuilder& addPropertyWithGetter(
        std::string name, 
        std::shared_ptr<MetaType> type,
        kernel::Value getter
    );
    
    /**
     * Add a property with custom getter and setter
     * 
     * @param name Property name
     * @param type Property type
     * @param getter Getter function
     * @param setter Setter function
     * @return This builder for chaining
     */
    DynamicTypeBuilder& addPropertyWithAccessors(
        std::string name,
        std::shared_ptr<MetaType> type,
        kernel::Value getter,
        kernel::Value setter
    );
    
    /**
     * Add a method to the type
     * 
     * @param name Method name
     * @param param_types Parameter types
     * @param return_type Return type
     * @param implementation Method implementation
     * @return This builder for chaining
     */
    DynamicTypeBuilder& addMethod(
        std::string name,
        std::vector<std::shared_ptr<MetaType>> param_types,
        std::shared_ptr<MetaType> return_type,
        kernel::Value implementation
    );
    
    /**
     * Set the base class (for classes only)
     * 
     * @param base_class Base class type
     * @return This builder for chaining
     */
    DynamicTypeBuilder& setBaseClass(std::shared_ptr<ClassMetaType> base_class);
    
    /**
     * Build and register the type
     * 
     * @return The created MetaType, or error message
     */
    std::expected<std::shared_ptr<MetaType>, std::string> build();
    
private:
    enum class TypeKind {
        Class,
        Struct,
        Trait
    };
    
    DynamicTypeBuilder(std::string name, TypeKind kind);
    
    std::string name_;
    TypeKind kind_;
    std::vector<Field> fields_;
    std::vector<Property> properties_;
    std::vector<Method> methods_;
    std::shared_ptr<ClassMetaType> base_class_;
};

/**
 * Meta - Namespace for dynamic type creation API
 * 
 * This provides the public API for creating types at runtime,
 * matching the design document's Meta.createClass(...) syntax.
 */
namespace Meta {

/**
 * Create a new class type dynamically
 * 
 * @param name Class name
 * @return Builder for configuring the class
 */
inline DynamicTypeBuilder createClass(std::string name) {
    return DynamicTypeBuilder::createClass(std::move(name));
}

/**
 * Create a new struct type dynamically
 * 
 * @param name Struct name
 * @return Builder for configuring the struct
 */
inline DynamicTypeBuilder createStruct(std::string name) {
    return DynamicTypeBuilder::createStruct(std::move(name));
}

/**
 * Create a new trait type dynamically
 * 
 * @param name Trait name
 * @return Builder for configuring the trait
 */
inline DynamicTypeBuilder createTrait(std::string name) {
    return DynamicTypeBuilder::createTrait(std::move(name));
}

} // namespace Meta

} // namespace meld::meta
