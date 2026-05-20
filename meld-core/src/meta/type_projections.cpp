#include "meld/meta/type_projections.hpp"
#include <format>
#include <ranges>

namespace meld::meta {

// Helper functions implementation
namespace projection_helpers {

bool is_nullable(const MetaType& type) {
    auto& registry = TypeRegistry::instance();
    return registry.is_nullable_type(type);
}

std::expected<std::shared_ptr<MetaType>, std::string>
get_inner_type(std::shared_ptr<MetaType> nullable_type) {
    auto& registry = TypeRegistry::instance();
    return registry.get_inner_type(*nullable_type);
}

std::shared_ptr<MetaType> make_nullable(std::shared_ptr<MetaType> type) {
    auto& registry = TypeRegistry::instance();
    
    // If already nullable, return as-is
    if (is_nullable(*type)) {
        return type;
    }
    
    return registry.create_optional_type(type);
}

std::shared_ptr<MetaType> make_required(std::shared_ptr<MetaType> type) {
    // If not nullable, return as-is
    if (!is_nullable(*type)) {
        return type;
    }
    
    // Extract inner type from union
    auto inner_result = get_inner_type(type);
    if (inner_result.has_value()) {
        return *inner_result;
    }
    
    return type;
}

} // namespace projection_helpers

// OmitProjection implementation
std::expected<std::shared_ptr<MetaType>, std::string>
OmitProjection::apply(std::shared_ptr<MetaType> source_type, const std::set<std::string>& keys_to_omit) {
    if (auto* struct_type = dynamic_cast<StructMetaType*>(source_type.get())) {
        return apply_to_struct(std::shared_ptr<StructMetaType>(source_type, struct_type), keys_to_omit);
    }
    
    if (auto* class_type = dynamic_cast<ClassMetaType*>(source_type.get())) {
        return apply_to_class(std::shared_ptr<ClassMetaType>(source_type, class_type), keys_to_omit);
    }
    
    return std::unexpected(std::format(
        "omit can only be applied to struct or class types, got '{}'", 
        source_type->name()));
}

std::expected<std::shared_ptr<StructMetaType>, std::string>
OmitProjection::apply_to_struct(std::shared_ptr<StructMetaType> source, const std::set<std::string>& keys) {
    // Filter out fields with names in keys_to_omit
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        if (!keys.contains(field.name)) {
            new_fields.push_back(field);
        }
    }
    
    // Filter out properties with names in keys_to_omit
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        if (!keys.contains(prop.name)) {
            new_properties.push_back(prop);
        }
    }
    
    std::string new_name = std::format("omit<{}, ...>", source->name());
    return std::make_shared<StructMetaType>(new_name, std::move(new_fields), std::move(new_properties));
}

std::expected<std::shared_ptr<ClassMetaType>, std::string>
OmitProjection::apply_to_class(std::shared_ptr<ClassMetaType> source, const std::set<std::string>& keys) {
    // Filter out fields with names in keys_to_omit
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        if (!keys.contains(field.name)) {
            new_fields.push_back(field);
        }
    }
    
    // Keep all methods (Omit only affects properties/fields)
    std::vector<Method> new_methods = source->methods();
    
    // Filter out properties with names in keys_to_omit
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        if (!keys.contains(prop.name)) {
            new_properties.push_back(prop);
        }
    }
    
    std::string new_name = std::format("omit<{}, ...>", source->name());
    return std::make_shared<ClassMetaType>(new_name, std::move(new_fields), 
                                           std::move(new_methods), std::move(new_properties));
}

// PickProjection implementation
std::expected<std::shared_ptr<MetaType>, std::string>
PickProjection::apply(std::shared_ptr<MetaType> source_type, const std::set<std::string>& keys_to_pick) {
    if (auto* struct_type = dynamic_cast<StructMetaType*>(source_type.get())) {
        return apply_to_struct(std::shared_ptr<StructMetaType>(source_type, struct_type), keys_to_pick);
    }
    
    if (auto* class_type = dynamic_cast<ClassMetaType*>(source_type.get())) {
        return apply_to_class(std::shared_ptr<ClassMetaType>(source_type, class_type), keys_to_pick);
    }
    
    return std::unexpected(std::format(
        "pick can only be applied to struct or class types, got '{}'", 
        source_type->name()));
}

std::expected<std::shared_ptr<StructMetaType>, std::string>
PickProjection::apply_to_struct(std::shared_ptr<StructMetaType> source, const std::set<std::string>& keys) {
    // Keep only fields with names in keys_to_pick
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        if (keys.contains(field.name)) {
            new_fields.push_back(field);
        }
    }
    
    // Keep only properties with names in keys_to_pick
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        if (keys.contains(prop.name)) {
            new_properties.push_back(prop);
        }
    }
    
    // Verify all requested keys were found
    size_t total_found = new_fields.size() + new_properties.size();
    if (total_found < keys.size()) {
        return std::unexpected(std::format(
            "Some keys not found in struct '{}'. Requested: {}, Found: {}",
            source->name(), keys.size(), total_found));
    }
    
    std::string new_name = std::format("pick<{}, ...>", source->name());
    return std::make_shared<StructMetaType>(new_name, std::move(new_fields), std::move(new_properties));
}

std::expected<std::shared_ptr<ClassMetaType>, std::string>
PickProjection::apply_to_class(std::shared_ptr<ClassMetaType> source, const std::set<std::string>& keys) {
    // Keep only fields with names in keys_to_pick
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        if (keys.contains(field.name)) {
            new_fields.push_back(field);
        }
    }
    
    // Don't include methods in Pick (only properties/fields)
    std::vector<Method> new_methods;
    
    // Keep only properties with names in keys_to_pick
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        if (keys.contains(prop.name)) {
            new_properties.push_back(prop);
        }
    }
    
    // Verify all requested keys were found
    size_t total_found = new_fields.size() + new_properties.size();
    if (total_found < keys.size()) {
        return std::unexpected(std::format(
            "Some keys not found in class '{}'. Requested: {}, Found: {}",
            source->name(), keys.size(), total_found));
    }
    
    std::string new_name = std::format("pick<{}, ...>", source->name());
    return std::make_shared<ClassMetaType>(new_name, std::move(new_fields), 
                                           std::move(new_methods), std::move(new_properties));
}

// PartialProjection implementation
std::expected<std::shared_ptr<MetaType>, std::string>
PartialProjection::apply(std::shared_ptr<MetaType> source_type) {
    if (auto* struct_type = dynamic_cast<StructMetaType*>(source_type.get())) {
        return apply_to_struct(std::shared_ptr<StructMetaType>(source_type, struct_type));
    }
    
    if (auto* class_type = dynamic_cast<ClassMetaType*>(source_type.get())) {
        return apply_to_class(std::shared_ptr<ClassMetaType>(source_type, class_type));
    }
    
    return std::unexpected(std::format(
        "partial can only be applied to struct or class types, got '{}'", 
        source_type->name()));
}

std::expected<std::shared_ptr<StructMetaType>, std::string>
PartialProjection::apply_to_struct(std::shared_ptr<StructMetaType> source) {
    // Make all field types nullable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        auto nullable_type = projection_helpers::make_nullable(field.type);
        new_fields.emplace_back(field.name, nullable_type, field.is_mutable);
    }
    
    // Make all property types nullable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.type = projection_helpers::make_nullable(prop.type);
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("partial<{}>", source->name());
    return std::make_shared<StructMetaType>(new_name, std::move(new_fields), std::move(new_properties));
}

std::expected<std::shared_ptr<ClassMetaType>, std::string>
PartialProjection::apply_to_class(std::shared_ptr<ClassMetaType> source) {
    // Make all field types nullable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        auto nullable_type = projection_helpers::make_nullable(field.type);
        new_fields.emplace_back(field.name, nullable_type, field.is_mutable);
    }
    
    // Keep methods as-is
    std::vector<Method> new_methods = source->methods();
    
    // Make all property types nullable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.type = projection_helpers::make_nullable(prop.type);
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("partial<{}>", source->name());
    return std::make_shared<ClassMetaType>(new_name, std::move(new_fields), 
                                           std::move(new_methods), std::move(new_properties));
}

// RequiredProjection implementation
std::expected<std::shared_ptr<MetaType>, std::string>
RequiredProjection::apply(std::shared_ptr<MetaType> source_type) {
    if (auto* struct_type = dynamic_cast<StructMetaType*>(source_type.get())) {
        return apply_to_struct(std::shared_ptr<StructMetaType>(source_type, struct_type));
    }
    
    if (auto* class_type = dynamic_cast<ClassMetaType*>(source_type.get())) {
        return apply_to_class(std::shared_ptr<ClassMetaType>(source_type, class_type));
    }
    
    return std::unexpected(std::format(
        "required can only be applied to struct or class types, got '{}'", 
        source_type->name()));
}

std::expected<std::shared_ptr<StructMetaType>, std::string>
RequiredProjection::apply_to_struct(std::shared_ptr<StructMetaType> source) {
    // Make all field types non-nullable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        auto required_type = projection_helpers::make_required(field.type);
        new_fields.emplace_back(field.name, required_type, field.is_mutable);
    }
    
    // Make all property types non-nullable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.type = projection_helpers::make_required(prop.type);
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("required<{}>", source->name());
    return std::make_shared<StructMetaType>(new_name, std::move(new_fields), std::move(new_properties));
}

std::expected<std::shared_ptr<ClassMetaType>, std::string>
RequiredProjection::apply_to_class(std::shared_ptr<ClassMetaType> source) {
    // Make all field types non-nullable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        auto required_type = projection_helpers::make_required(field.type);
        new_fields.emplace_back(field.name, required_type, field.is_mutable);
    }
    
    // Keep methods as-is
    std::vector<Method> new_methods = source->methods();
    
    // Make all property types non-nullable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.type = projection_helpers::make_required(prop.type);
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("required<{}>", source->name());
    return std::make_shared<ClassMetaType>(new_name, std::move(new_fields), 
                                           std::move(new_methods), std::move(new_properties));
}

// ReadonlyProjection implementation
std::expected<std::shared_ptr<MetaType>, std::string>
ReadonlyProjection::apply(std::shared_ptr<MetaType> source_type) {
    if (auto* struct_type = dynamic_cast<StructMetaType*>(source_type.get())) {
        return apply_to_struct(std::shared_ptr<StructMetaType>(source_type, struct_type));
    }
    
    if (auto* class_type = dynamic_cast<ClassMetaType*>(source_type.get())) {
        return apply_to_class(std::shared_ptr<ClassMetaType>(source_type, class_type));
    }
    
    return std::unexpected(std::format(
        "readonly can only be applied to struct or class types, got '{}'", 
        source_type->name()));
}

std::expected<std::shared_ptr<StructMetaType>, std::string>
ReadonlyProjection::apply_to_struct(std::shared_ptr<StructMetaType> source) {
    // Make all fields immutable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        new_fields.emplace_back(field.name, field.type, false); // is_mutable = false
    }
    
    // Make all properties immutable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.is_mutable = false;
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("readonly<{}>", source->name());
    return std::make_shared<StructMetaType>(new_name, std::move(new_fields), std::move(new_properties));
}

std::expected<std::shared_ptr<ClassMetaType>, std::string>
ReadonlyProjection::apply_to_class(std::shared_ptr<ClassMetaType> source) {
    // Make all fields immutable
    std::vector<Field> new_fields;
    for (const auto& field : source->fields()) {
        new_fields.emplace_back(field.name, field.type, false); // is_mutable = false
    }
    
    // Keep methods as-is
    std::vector<Method> new_methods = source->methods();
    
    // Make all properties immutable
    std::vector<Property> new_properties;
    for (const auto& prop : source->properties()) {
        Property new_prop = prop;
        new_prop.is_mutable = false;
        new_properties.push_back(std::move(new_prop));
    }
    
    std::string new_name = std::format("readonly<{}>", source->name());
    return std::make_shared<ClassMetaType>(new_name, std::move(new_fields), 
                                           std::move(new_methods), std::move(new_properties));
}

} // namespace meld::meta
