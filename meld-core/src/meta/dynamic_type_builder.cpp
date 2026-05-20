#include "meld/meta/dynamic_type_builder.hpp"
#include <format>

namespace meld::meta {

DynamicTypeBuilder::DynamicTypeBuilder(std::string name, TypeKind kind)
    : name_(std::move(name))
    , kind_(kind)
    , base_class_(nullptr) {
}

DynamicTypeBuilder DynamicTypeBuilder::createClass(std::string name) {
    return DynamicTypeBuilder(std::move(name), TypeKind::Class);
}

DynamicTypeBuilder DynamicTypeBuilder::createStruct(std::string name) {
    return DynamicTypeBuilder(std::move(name), TypeKind::Struct);
}

DynamicTypeBuilder DynamicTypeBuilder::createTrait(std::string name) {
    return DynamicTypeBuilder(std::move(name), TypeKind::Trait);
}

DynamicTypeBuilder& DynamicTypeBuilder::addField(
    std::string name, 
    std::shared_ptr<MetaType> type, 
    bool is_mutable) {
    
    fields_.emplace_back(std::move(name), std::move(type), is_mutable);
    return *this;
}

DynamicTypeBuilder& DynamicTypeBuilder::addProperty(
    std::string name,
    std::shared_ptr<MetaType> type,
    bool is_mutable) {
    
    Property prop(std::move(name), std::move(type), is_mutable);
    properties_.push_back(std::move(prop));
    return *this;
}

DynamicTypeBuilder& DynamicTypeBuilder::addPropertyWithGetter(
    std::string name,
    std::shared_ptr<MetaType> type,
    kernel::Value getter) {
    
    Property prop(std::move(name), std::move(type), false);
    prop.has_custom_getter = true;
    prop.getter_impl = std::move(getter);
    prop.has_backing_field = false;
    properties_.push_back(std::move(prop));
    return *this;
}

DynamicTypeBuilder& DynamicTypeBuilder::addPropertyWithAccessors(
    std::string name,
    std::shared_ptr<MetaType> type,
    kernel::Value getter,
    kernel::Value setter) {
    
    Property prop(std::move(name), std::move(type), true);
    prop.has_custom_getter = true;
    prop.has_custom_setter = true;
    prop.getter_impl = std::move(getter);
    prop.setter_impl = std::move(setter);
    prop.has_backing_field = false;
    properties_.push_back(std::move(prop));
    return *this;
}

DynamicTypeBuilder& DynamicTypeBuilder::addMethod(
    std::string name,
    std::vector<std::shared_ptr<MetaType>> param_types,
    std::shared_ptr<MetaType> return_type,
    kernel::Value implementation) {
    
    methods_.emplace_back(
        std::move(name),
        std::move(param_types),
        std::move(return_type),
        std::move(implementation)
    );
    return *this;
}

DynamicTypeBuilder& DynamicTypeBuilder::setBaseClass(std::shared_ptr<ClassMetaType> base_class) {
    if (kind_ != TypeKind::Class) {
        throw std::runtime_error("Base class can only be set for class types");
    }
    base_class_ = std::move(base_class);
    return *this;
}

std::expected<std::shared_ptr<MetaType>, std::string> DynamicTypeBuilder::build() {
    // Validate the type configuration
    if (name_.empty()) {
        return std::unexpected("Type name cannot be empty");
    }
    
    // Check if type already exists
    auto& registry = TypeRegistry::instance();
    auto existing = registry.get_type(name_);
    if (existing.has_value()) {
        return std::unexpected(std::format("Type '{}' already exists", name_));
    }
    
    // Create the appropriate MetaType based on kind
    std::shared_ptr<MetaType> type;
    
    switch (kind_) {
        case TypeKind::Class: {
            // Validate: classes can have fields, properties, and methods
            type = std::make_shared<ClassMetaType>(
                name_,
                fields_,
                methods_,
                properties_,
                base_class_
            );
            break;
        }
        
        case TypeKind::Struct: {
            // Validate: structs can have fields and properties, but not methods
            if (!methods_.empty()) {
                return std::unexpected("Structs cannot have methods");
            }
            if (base_class_) {
                return std::unexpected("Structs cannot have base classes");
            }
            
            type = std::make_shared<StructMetaType>(
                name_,
                fields_,
                properties_
            );
            break;
        }
        
        case TypeKind::Trait: {
            // Validate: traits can only have methods (no fields or properties)
            if (!fields_.empty()) {
                return std::unexpected("Traits cannot have fields");
            }
            if (!properties_.empty()) {
                return std::unexpected("Traits cannot have properties");
            }
            if (base_class_) {
                return std::unexpected("Traits cannot have base classes");
            }
            
            type = std::make_shared<TraitMetaType>(
                name_,
                methods_
            );
            break;
        }
    }
    
    // Register the type
    registry.register_type(name_, type);
    
    return type;
}

} // namespace meld::meta
