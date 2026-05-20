#include "meld/meta/metatype.hpp"
#include "meld/kernel/operations.hpp"
#include <algorithm>
#include <numeric>
#include <format>

namespace meld::meta {

// MetaType base implementation
void MetaType::add_annotation(const std::string& name, const std::map<std::string, std::string>& values) {
    annotations_[name] = values;
}

void MetaType::add_annotation(const std::string& name, const std::string& value) {
    annotations_[name] = {{"", value}};
}

std::map<std::string, std::map<std::string, std::string>> MetaType::get_annotations() const {
    return annotations_;
}

bool MetaType::has_annotation(const std::string& name) const {
    return annotations_.find(name) != annotations_.end();
}

std::expected<std::map<std::string, std::string>, std::string> MetaType::get_annotation(const std::string& name) const {
    auto it = annotations_.find(name);
    if (it != annotations_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Annotation '{}' not found", name));
}

bool MetaType::is_assignable_from(const MetaType& other) const {
    return this == &other || other.is_subtype_of(*this);
}

bool MetaType::is_subtype_of(const MetaType& other) const {
    return this == &other;
}

// Get the singleton MetaType instance (the type of all types)
std::shared_ptr<MetaType> MetaType::get_metatype_instance() {
    static auto metatype_instance = std::make_shared<MetaTypeMetaType>();
    return metatype_instance;
}

// typeof operation - returns MetaType instances
std::shared_ptr<MetaType> MetaType::typeof_value(const MetaType& type) {
    // All types (including MetaType itself) are instances of MetaType
    return get_metatype_instance();
}

std::shared_ptr<MetaType> MetaType::get_metatype() const {
    // MetaType is the type of all types, including itself
    return get_metatype_instance();
}

// MetaTypeMetaType implementation - self-referential
std::shared_ptr<MetaType> MetaTypeMetaType::get_metatype() const {
    // MetaType is an instance of itself
    return MetaType::get_metatype_instance();
}

// Factory methods
std::shared_ptr<MetaType> MetaType::create_primitive(std::string name, size_t size) {
    return std::make_shared<PrimitiveMetaType>(std::move(name), size);
}

std::shared_ptr<MetaType> MetaType::create_struct(std::string name, std::vector<Field> fields, std::vector<Property> properties) {
    return std::make_shared<StructMetaType>(std::move(name), std::move(fields), std::move(properties));
}

std::shared_ptr<MetaType> MetaType::create_class(std::string name, std::vector<Field> fields, std::vector<Method> methods, std::vector<Property> properties) {
    return std::make_shared<ClassMetaType>(std::move(name), std::move(fields), std::move(methods), std::move(properties));
}

std::shared_ptr<MetaType> MetaType::create_enum(std::string name, std::vector<EnumVariant> variants) {
    return std::make_shared<EnumMetaType>(std::move(name), std::move(variants));
}

std::shared_ptr<MetaType> MetaType::create_newtype(std::string name, std::shared_ptr<MetaType> wrapped_type, bool is_transparent) {
    return std::make_shared<NewtypeMetaType>(std::move(name), std::move(wrapped_type), is_transparent);
}

std::shared_ptr<MetaType> MetaType::create_trait(std::string name, std::vector<Method> methods) {
    return std::make_shared<TraitMetaType>(std::move(name), std::move(methods));
}

std::shared_ptr<MetaType> MetaType::create_union(std::vector<std::shared_ptr<MetaType>> types) {
    return std::make_shared<UnionMetaType>(std::move(types));
}

std::shared_ptr<MetaType> MetaType::create_intersection(std::vector<std::shared_ptr<MetaType>> types) {
    return std::make_shared<IntersectionMetaType>(std::move(types));
}

std::shared_ptr<MetaType> MetaType::create_refinement(std::string name, std::shared_ptr<MetaType> base_type, kernel::Value predicate) {
    return std::make_shared<RefinementMetaType>(std::move(name), std::move(base_type), std::move(predicate));
}

// StructMetaType implementation
size_t StructMetaType::size() const {
    return std::accumulate(fields_.begin(), fields_.end(), size_t(0),
        [](size_t sum, const Field& f) { return sum + f.type->size(); });
}

std::expected<const Field*, std::string> StructMetaType::get_field(const std::string& name) const {
    auto it = std::ranges::find_if(fields_, [&name](const Field& f) { return f.name == name; });
    if (it != fields_.end()) {
        return &(*it);
    }
    return std::unexpected(std::format("Field '{}' not found in struct '{}'", name, name_));
}

std::expected<const Property*, std::string> StructMetaType::get_property(const std::string& name) const {
    auto it = std::ranges::find_if(properties_, [&name](const Property& p) { return p.name == name; });
    if (it != properties_.end()) {
        return &(*it);
    }
    return std::unexpected(std::format("Property '{}' not found in struct '{}'", name, name_));
}

// ClassMetaType implementation
std::expected<const Field*, std::string> ClassMetaType::get_field(const std::string& name) const {
    // Search in this class
    auto it = std::ranges::find_if(fields_, [&name](const Field& f) { return f.name == name; });
    if (it != fields_.end()) {
        return &(*it);
    }
    
    // Search in base class
    if (base_class_) {
        return base_class_->get_field(name);
    }
    
    return std::unexpected(std::format("Field '{}' not found in class '{}'", name, name_));
}

std::expected<const Method*, std::string> ClassMetaType::get_method(const std::string& name) const {
    // Search in this class
    auto it = std::ranges::find_if(methods_, [&name](const Method& m) { return m.name == name; });
    if (it != methods_.end()) {
        return &(*it);
    }
    
    // Search in base class
    if (base_class_) {
        return base_class_->get_method(name);
    }
    
    return std::unexpected(std::format("Method '{}' not found in class '{}'", name, name_));
}

std::expected<const Property*, std::string> ClassMetaType::get_property(const std::string& name) const {
    // Search in this class
    auto it = std::ranges::find_if(properties_, [&name](const Property& p) { return p.name == name; });
    if (it != properties_.end()) {
        return &(*it);
    }
    
    // Search in base class
    if (base_class_) {
        return base_class_->get_property(name);
    }
    
    return std::unexpected(std::format("Property '{}' not found in class '{}'", name, name_));
}

bool ClassMetaType::is_subtype_of(const MetaType& other) const {
    if (this == &other) return true;
    
    // Check if other is a base class
    if (auto* other_class = dynamic_cast<const ClassMetaType*>(&other)) {
        if (base_class_ && base_class_->is_subtype_of(*other_class)) {
            return true;
        }
    }
    
    return false;
}

// EnumMetaType implementation
size_t EnumMetaType::size() const {
    // For enums, we need to calculate the size based on the largest variant
    size_t max_size = sizeof(int); // Tag size for discriminant
    
    for (const auto& variant : variants_) {
        size_t variant_size = 0;
        for (const auto& field : variant.associated_fields) {
            variant_size += field.type->size();
        }
        max_size = std::max(max_size, sizeof(int) + variant_size);
    }
    
    return max_size;
}

std::expected<const EnumVariant*, std::string> EnumMetaType::get_variant(const std::string& name) const {
    auto it = std::ranges::find_if(variants_, [&name](const EnumVariant& v) { return v.name == name; });
    if (it != variants_.end()) {
        return &(*it);
    }
    return std::unexpected(std::format("Variant '{}' not found in enum '{}'", name, name_));
}

bool EnumMetaType::has_variant(const std::string& name) const {
    return std::ranges::any_of(variants_, [&name](const EnumVariant& v) { return v.name == name; });
}

std::expected<const EnumVariant*, std::string> EnumMetaType::get_variant_by_index(size_t index) const {
    if (index >= variants_.size()) {
        return std::unexpected(std::format("Variant index {} out of range for enum '{}' (has {} variants)", 
                                         index, name_, variants_.size()));
    }
    return &variants_[index];
}

// TraitMetaType implementation
std::expected<const Method*, std::string> TraitMetaType::get_method(const std::string& name) const {
    auto it = std::ranges::find_if(methods_, [&name](const Method& m) { return m.name == name; });
    if (it != methods_.end()) {
        return &(*it);
    }
    return std::unexpected(std::format("Method '{}' not found in trait '{}'", name, name_));
}

// UnionMetaType implementation
std::string UnionMetaType::name() const {
    std::string result;
    for (size_t i = 0; i < types_.size(); ++i) {
        if (i > 0) result += " | ";
        result += types_[i]->name();
    }
    return result;
}

size_t UnionMetaType::size() const {
    // Union size is the maximum of all types (needs to hold any variant)
    if (types_.empty()) return 0;
    return std::ranges::max(types_ | std::views::transform([](const auto& t) { return t->size(); }));
}

bool UnionMetaType::contains_type(const MetaType& type) const {
    // Check if the type is directly in the union
    return std::ranges::any_of(types_, [&type](const auto& t) { 
        return t.get() == &type || t->name() == type.name(); 
    });
}

bool UnionMetaType::is_assignable_from(const MetaType& other) const {
    // A value of type T is assignable to (A | B) if T is assignable to A or B
    // This means T must be a subtype of at least one of the union members
    
    // If other is also a union, all its types must be assignable to this union
    if (auto* other_union = dynamic_cast<const UnionMetaType*>(&other)) {
        return std::ranges::all_of(other_union->types(), 
            [this](const auto& t) { return this->is_assignable_from(*t); });
    }
    
    // Otherwise, check if any of our types can accept the other type
    return std::ranges::any_of(types_, [&other](const auto& t) { 
        return t->is_assignable_from(other); 
    });
}

bool UnionMetaType::is_subtype_of(const MetaType& other) const {
    // (A | B) is a subtype of C if both A and B are subtypes of C
    // This is the contrapositive of is_assignable_from
    
    // If other is also a union, we're a subtype if all our types are in the other union
    if (auto* other_union = dynamic_cast<const UnionMetaType*>(&other)) {
        return std::ranges::all_of(types_, 
            [other_union](const auto& t) { return other_union->is_assignable_from(*t); });
    }
    
    // Otherwise, all our types must be subtypes of the other type
    return std::ranges::all_of(types_, 
        [&other](const auto& t) { return t->is_subtype_of(other); });
}

// IntersectionMetaType implementation
std::string IntersectionMetaType::name() const {
    std::string result;
    for (size_t i = 0; i < types_.size(); ++i) {
        if (i > 0) result += " & ";
        result += types_[i]->name();
    }
    return result;
}

size_t IntersectionMetaType::size() const {
    // Intersection size is the maximum (must satisfy all constraints)
    if (types_.empty()) return 0;
    return std::ranges::max(types_ | std::views::transform([](const auto& t) { return t->size(); }));
}

bool IntersectionMetaType::satisfies_all(const MetaType& type) const {
    // Check if the given type satisfies all constraints in the intersection
    return std::ranges::all_of(types_, [&type](const auto& t) { 
        return type.is_subtype_of(*t); 
    });
}

bool IntersectionMetaType::is_assignable_from(const MetaType& other) const {
    // A value of type T is assignable to (A & B) if T satisfies both A and B
    // This means T must be a subtype of all intersection members
    
    // If other is also an intersection, it must satisfy all our constraints
    if (auto* other_intersection = dynamic_cast<const IntersectionMetaType*>(&other)) {
        // Other intersection is assignable if it has all our types (or subtypes)
        return std::ranges::all_of(types_, 
            [other_intersection](const auto& t) { 
                return other_intersection->satisfies_all(*t); 
            });
    }
    
    // Otherwise, check if the other type satisfies all our constraints
    return satisfies_all(other);
}

bool IntersectionMetaType::is_subtype_of(const MetaType& other) const {
    // (A & B) is a subtype of C if (A & B) satisfies C
    // Since (A & B) must satisfy both A and B, it's a subtype of both
    
    // If other is also an intersection, we must satisfy all its constraints
    if (auto* other_intersection = dynamic_cast<const IntersectionMetaType*>(&other)) {
        return std::ranges::all_of(other_intersection->types(), 
            [this](const auto& t) { return this->satisfies_all(*t); });
    }
    
    // Otherwise, we're a subtype if any of our types is a subtype of other
    // (because we satisfy all our types, so we satisfy any one of them)
    return std::ranges::any_of(types_, 
        [&other](const auto& t) { return t->is_subtype_of(other); });
}

// GenericMetaType implementation
std::expected<std::shared_ptr<MetaType>, std::string> 
GenericMetaType::instantiate(std::shared_ptr<MetaType> concrete_type) const {
    // Check bound constraint
    if (bound_ && !concrete_type->is_subtype_of(*bound_)) {
        return std::unexpected(std::format(
            "Type '{}' does not satisfy bound '{}' for type parameter '{}'",
            concrete_type->name(), bound_->name(), name_));
    }
    
    return concrete_type;
}

// TypeRegistry implementation
TypeRegistry::TypeRegistry() {
    // Register built-in types
    register_type("int", std::make_shared<PrimitiveMetaType>("int", sizeof(int64_t)));
    register_type("float", std::make_shared<PrimitiveMetaType>("float", sizeof(double)));
    register_type("bool", std::make_shared<PrimitiveMetaType>("bool", sizeof(bool)));
    register_type("string", std::make_shared<PrimitiveMetaType>("string", sizeof(void*)));
    register_type("unit", std::make_shared<PrimitiveMetaType>("unit", 0));
    register_type("symbol", std::make_shared<PrimitiveMetaType>("symbol", sizeof(void*)));
    register_type("function", std::make_shared<PrimitiveMetaType>("function", sizeof(void*)));
    
    // Register Null type
    null_type_ = std::make_shared<PrimitiveMetaType>("null", 0);
    register_type("null", null_type_);
    
    // Create Copyable trait
    std::vector<Method> copyable_methods = {
        Method("copy", {}, nullptr, kernel::Value())  // Returns Self
    };
    copyable_trait_ = std::make_shared<TraitMetaType>("copyable", std::move(copyable_methods));
    register_type("copyable", copyable_trait_);
    
    // Register collection/container types with lowercase names
    // Option type - generic sum type for optional values
    auto option_type = std::make_shared<EnumMetaType>("option", std::vector<EnumVariant>{
        EnumVariant("some", {Field("value", std::make_shared<GenericMetaType>("T", Variance::Covariant))}),
        EnumVariant("none")
    });
    register_type("option", option_type);
    
    // Result type - generic sum type for error handling
    auto result_type = std::make_shared<EnumMetaType>("result", std::vector<EnumVariant>{
        EnumVariant("ok", {Field("value", std::make_shared<GenericMetaType>("T", Variance::Covariant))}),
        EnumVariant("err", {Field("error", std::make_shared<GenericMetaType>("E", Variance::Covariant))})
    });
    register_type("result", result_type);
    
    // Mutable collection types
    register_type("mutable_list", std::make_shared<StructMetaType>("mutable_list", std::vector<Field>{
        Field("elements", std::make_shared<GenericMetaType>("T", Variance::Invariant)),
        Field("length", std::make_shared<PrimitiveMetaType>("int", sizeof(int64_t)))
    }));
    register_type("mutable_map", std::make_shared<StructMetaType>("mutable_map", std::vector<Field>{
        Field("keys", std::make_shared<GenericMetaType>("K", Variance::Invariant)),
        Field("values", std::make_shared<GenericMetaType>("V", Variance::Invariant))
    }));
    register_type("mutable_set", std::make_shared<StructMetaType>("mutable_set", std::vector<Field>{
        Field("elements", std::make_shared<GenericMetaType>("T", Variance::Invariant))
    }));
    
    // Persistent (immutable) collection types
    register_type("persistent_list", std::make_shared<StructMetaType>("persistent_list", std::vector<Field>{
        Field("elements", std::make_shared<GenericMetaType>("T", Variance::Covariant)),
        Field("length", std::make_shared<PrimitiveMetaType>("int", sizeof(int64_t)))
    }));
    register_type("persistent_map", std::make_shared<StructMetaType>("persistent_map", std::vector<Field>{
        Field("keys", std::make_shared<GenericMetaType>("K", Variance::Covariant)),
        Field("values", std::make_shared<GenericMetaType>("V", Variance::Covariant))
    }));
    register_type("persistent_set", std::make_shared<StructMetaType>("persistent_set", std::vector<Field>{
        Field("elements", std::make_shared<GenericMetaType>("T", Variance::Covariant))
    }));
    
    // Lazy sequence type
    register_type("lazy_sequence", std::make_shared<StructMetaType>("lazy_sequence", std::vector<Field>{
        Field("generator", std::make_shared<GenericMetaType>("T", Variance::Covariant))
    }));
    
    // Collection base trait
    std::vector<Method> collection_methods = {
        Method("size", {}, nullptr, kernel::Value()),
        Method("is_empty", {}, nullptr, kernel::Value()),
        Method("contains", {}, nullptr, kernel::Value())
    };
    register_type("collection", std::make_shared<TraitMetaType>("collection", std::move(collection_methods)));
    
    // Register constructor functions for option and result
    // some(value) -> option<T>
    register_type("some", std::make_shared<FunctionMetaType>(
        std::vector<std::shared_ptr<MetaType>>{std::make_shared<GenericMetaType>("T", Variance::Covariant)},
        option_type
    ));
    // none() -> option<T>
    register_type("none", std::make_shared<FunctionMetaType>(
        std::vector<std::shared_ptr<MetaType>>{},
        option_type
    ));
    // ok(value) -> result<T, E>
    register_type("ok", std::make_shared<FunctionMetaType>(
        std::vector<std::shared_ptr<MetaType>>{std::make_shared<GenericMetaType>("T", Variance::Covariant)},
        result_type
    ));
    // err(error) -> result<T, E>
    register_type("err", std::make_shared<FunctionMetaType>(
        std::vector<std::shared_ptr<MetaType>>{std::make_shared<GenericMetaType>("E", Variance::Covariant)},
        result_type
    ));
}

void TypeRegistry::register_type(const std::string& name, std::shared_ptr<MetaType> type) {
    std::lock_guard<std::mutex> lock(mutex_);
    types_[name] = std::move(type);
}

std::expected<std::shared_ptr<MetaType>, std::string> 
TypeRegistry::get_type(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if it's an alias first
    auto alias_it = type_aliases_.find(name);
    if (alias_it != type_aliases_.end()) {
        return alias_it->second;
    }
    
    // Otherwise look up the actual type
    auto it = types_.find(name);
    if (it != types_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Type '{}' not found", name));
}

void TypeRegistry::register_type_alias(const std::string& alias_name, const std::string& target_type_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Resolve the target type
    auto target_it = types_.find(target_type_name);
    if (target_it == types_.end()) {
        // Check if target is also an alias
        auto alias_it = type_aliases_.find(target_type_name);
        if (alias_it != type_aliases_.end()) {
            type_aliases_[alias_name] = alias_it->second;
            return;
        }
        throw std::runtime_error(std::format("Target type '{}' not found for alias '{}'", 
                                            target_type_name, alias_name));
    }
    
    type_aliases_[alias_name] = target_it->second;
}

void TypeRegistry::register_type_alias(const std::string& alias_name, std::shared_ptr<MetaType> target_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    type_aliases_[alias_name] = std::move(target_type);
}

std::expected<std::shared_ptr<MetaType>, std::string> 
TypeRegistry::resolve_alias(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Type alias '{}' not found", name));
}

bool TypeRegistry::is_alias(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return type_aliases_.find(name) != type_aliases_.end();
}

std::shared_ptr<MetaType> TypeRegistry::get_int_type() const {
    return types_.at("int");
}

std::shared_ptr<MetaType> TypeRegistry::get_bool_type() const {
    return types_.at("bool");
}

std::shared_ptr<MetaType> TypeRegistry::get_string_type() const {
    return types_.at("string");
}

std::shared_ptr<MetaType> TypeRegistry::get_unit_type() const {
    return types_.at("unit");
}

std::shared_ptr<MetaType> TypeRegistry::get_float_type() const {
    return types_.at("float");
}

std::shared_ptr<MetaType> TypeRegistry::get_null_type() const {
    return null_type_;
}

std::shared_ptr<MetaType> TypeRegistry::create_optional_type(std::shared_ptr<MetaType> inner) {
    return MetaType::create_union({inner, null_type_});
}

bool TypeRegistry::is_nullable_type(const MetaType& type) const {
    // Check if type is a union containing Null
    if (auto* union_type = dynamic_cast<const UnionMetaType*>(&type)) {
        return std::ranges::any_of(union_type->types(), 
            [this](const auto& t) { return t.get() == null_type_.get(); });
    }
    return false;
}

std::expected<std::shared_ptr<MetaType>, std::string> 
TypeRegistry::get_inner_type(const MetaType& nullable_type) const {
    if (auto* union_type = dynamic_cast<const UnionMetaType*>(&nullable_type)) {
        // Find the non-null type in the union
        for (const auto& t : union_type->types()) {
            if (t.get() != null_type_.get()) {
                return t;
            }
        }
    }
    return std::unexpected("Type is not a nullable type");
}

std::shared_ptr<TraitMetaType> TypeRegistry::get_copyable_trait() const {
    return copyable_trait_;
}

std::shared_ptr<MetaType> TypeRegistry::create_array_type(std::shared_ptr<MetaType> element) {
    std::vector<Field> fields = {
        Field("elements", element, false),
        Field("length", get_int_type(), false)
    };
    return MetaType::create_struct(std::format("Array<{}>", element->name()), std::move(fields));
}

std::shared_ptr<MetaType> TypeRegistry::create_function_type(
    std::vector<std::shared_ptr<MetaType>> params,
    std::shared_ptr<MetaType> return_type) {
    
    return std::make_shared<FunctionMetaType>(std::move(params), std::move(return_type));
}

// FunctionMetaType implementation
bool FunctionMetaType::is_compatible_with(const FunctionMetaType& other) const {
    // Check parameter count
    if (param_types_.size() != other.param_types_.size()) {
        return false;
    }
    
    // Check parameter types (contravariant)
    for (size_t i = 0; i < param_types_.size(); ++i) {
        if (!other.param_types_[i]->is_assignable_from(*param_types_[i])) {
            return false;
        }
    }
    
    // Check return type (covariant)
    if (!return_type_->is_assignable_from(*other.return_type_)) {
        return false;
    }
    
    return true;
}

bool FunctionMetaType::can_call_with(const std::vector<std::shared_ptr<MetaType>>& arg_types) const {
    // Check parameter count
    if (arg_types.size() != param_types_.size()) {
        return false;
    }
    
    // Check each argument type
    for (size_t i = 0; i < arg_types.size(); ++i) {
        if (!param_types_[i]->is_assignable_from(*arg_types[i])) {
            return false;
        }
    }
    
    return true;
}

// RefinementMetaType implementation
std::expected<bool, std::string> RefinementMetaType::validate_value(const kernel::Value& value) const {
    // First check if the value is compatible with the base type
    // This is a simplified check - in a full implementation, we'd use proper type checking
    
    // Call the predicate function with the value
    try {
        if (!predicate_.is<kernel::Function>()) {
            return std::unexpected("Predicate is not a function");
        }
        
        auto result = kernel::apply(predicate_, {value});
        if (!result.has_value()) {
            return std::unexpected("Error calling predicate function: " + result.error());
        }
        
        if (result->is<kernel::Boolean>()) {
            return result->as<kernel::Boolean>()->value();
        } else if (result->is<kernel::String>()) {
            // Error case - predicate returned error string
            return std::unexpected(result->as<kernel::String>()->value());
        } else {
            return std::unexpected("Predicate function must return a boolean value");
        }
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Error evaluating predicate: ") + e.what());
    }
}

std::expected<kernel::Value, std::string> RefinementMetaType::from_value(const kernel::Value& value) const {
    auto validation_result = validate_value(value);
    if (!validation_result.has_value()) {
        return std::unexpected(validation_result.error());
    }
    
    if (!validation_result.value()) {
        return std::unexpected(std::format("Value does not satisfy refinement constraint for type '{}'", name_));
    }
    
    // If validation passes, return the value (possibly wrapped with type information)
    return value;
}

bool RefinementMetaType::is_assignable_from(const MetaType& other) const {
    // A refinement type can accept values from:
    // 1. The same refinement type
    // 2. Other refinement types with the same base type (if they satisfy our constraint)
    // 3. The base type (if the value satisfies our constraint - checked at runtime)
    
    if (this == &other) {
        return true;
    }
    
    // Check if other is a refinement type with the same base
    if (auto other_refinement = other.as<RefinementMetaType>()) {
        return base_type_->is_assignable_from(*other_refinement->base_type_);
    }
    
    // Check if other is our base type
    return base_type_->is_assignable_from(other);
}

bool RefinementMetaType::is_subtype_of(const MetaType& other) const {
    // A refinement type is a subtype of:
    // 1. Its base type
    // 2. Any type that its base type is a subtype of
    
    if (this == &other) {
        return true;
    }
    
    // We are a subtype of our base type
    if (&other == base_type_.get()) {
        return true;
    }
    
    // We are a subtype of anything our base type is a subtype of
    return base_type_->is_subtype_of(other);
}

// Static factory method for creating validated instances
std::expected<kernel::Value, std::string> RefinementMetaType::create_validated(
    std::shared_ptr<RefinementMetaType> refinement_type,
    const kernel::Value& value) {
    
    if (!refinement_type) {
        return std::unexpected("Refinement type is null");
    }
    
    return refinement_type->from_value(value);
}

// Static factory methods for composition and inheritance
std::shared_ptr<RefinementMetaType> RefinementMetaType::create_inherited(
    const std::string& name,
    std::shared_ptr<RefinementMetaType> parent,
    kernel::Value additional_predicate) {
    
    if (!parent) {
        throw std::invalid_argument("Parent refinement type cannot be null");
    }
    
    return std::make_shared<RefinementMetaType>(name, parent, additional_predicate);
}

std::shared_ptr<RefinementMetaType> RefinementMetaType::create_composed_and(
    const std::string& name,
    std::shared_ptr<MetaType> base_type,
    const std::vector<std::shared_ptr<RefinementMetaType>>& refinements) {
    
    if (refinements.empty()) {
        throw std::invalid_argument("Cannot create composed refinement type with no refinements");
    }
    
    // Verify all refinements have compatible base types
    for (const auto& refinement : refinements) {
        if (!refinement) {
            throw std::invalid_argument("Refinement in composition cannot be null");
        }
        if (!base_type->is_assignable_from(*refinement->base_type())) {
            throw std::invalid_argument(std::format(
                "Refinement type '{}' has incompatible base type '{}' for composition with base type '{}'",
                refinement->name(), refinement->base_type()->name(), base_type->name()));
        }
    }
    
    return std::make_shared<RefinementMetaType>(name, base_type, refinements, CompositionType::AND);
}

std::shared_ptr<RefinementMetaType> RefinementMetaType::create_composed_or(
    const std::string& name,
    std::shared_ptr<MetaType> base_type,
    const std::vector<std::shared_ptr<RefinementMetaType>>& refinements) {
    
    if (refinements.empty()) {
        throw std::invalid_argument("Cannot create composed refinement type with no refinements");
    }
    
    // Verify all refinements have compatible base types
    for (const auto& refinement : refinements) {
        if (!refinement) {
            throw std::invalid_argument("Refinement in composition cannot be null");
        }
        if (!base_type->is_assignable_from(*refinement->base_type())) {
            throw std::invalid_argument(std::format(
                "Refinement type '{}' has incompatible base type '{}' for composition with base type '{}'",
                refinement->name(), refinement->base_type()->name(), base_type->name()));
        }
    }
    
    return std::make_shared<RefinementMetaType>(name, base_type, refinements, CompositionType::OR);
}

// Helper methods for predicate combination
kernel::Value RefinementMetaType::combine_predicates_and(const kernel::Value& pred1, const kernel::Value& pred2) {
    // Create a new function that returns true only if both predicates return true
    kernel::Function::NativeImpl impl = [pred1, pred2](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() != 1) {
            return kernel::Value(std::make_shared<kernel::String>("Combined predicate expects exactly one argument"));
        }
        
        // Evaluate first predicate
        auto result1 = kernel::apply(pred1, args);
        if (!result1.has_value()) {
            return kernel::Value(std::make_shared<kernel::String>("Error in first predicate: " + result1.error()));
        }
        
        if (!result1->is<kernel::Boolean>()) {
            return kernel::Value(std::make_shared<kernel::String>("First predicate must return boolean"));
        }
        
        if (!result1->as<kernel::Boolean>()->value()) {
            return kernel::Value(kernel::Boolean::false_value());
        }
        
        // Evaluate second predicate
        auto result2 = kernel::apply(pred2, args);
        if (!result2.has_value()) {
            return kernel::Value(std::make_shared<kernel::String>("Error in second predicate: " + result2.error()));
        }
        
        if (!result2->is<kernel::Boolean>()) {
            return kernel::Value(std::make_shared<kernel::String>("Second predicate must return boolean"));
        }
        
        return kernel::Value(result2->as<kernel::Boolean>()->value() ? kernel::Boolean::true_value() : kernel::Boolean::false_value());
    };
    return kernel::Value(std::make_shared<kernel::Function>(
        std::vector<std::shared_ptr<kernel::Symbol>>{},
        kernel::Value(),
        impl,
        std::string("combined_and_predicate")));
}

kernel::Value RefinementMetaType::combine_predicates_or(const kernel::Value& pred1, const kernel::Value& pred2) {
    // Create a new function that returns true if either predicate returns true
    kernel::Function::NativeImpl impl = [pred1, pred2](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() != 1) {
            return kernel::Value(std::make_shared<kernel::String>("Combined predicate expects exactly one argument"));
        }
        
        // Evaluate first predicate
        auto result1 = kernel::apply(pred1, args);
        if (!result1.has_value()) {
            return kernel::Value(std::make_shared<kernel::String>("Error in first predicate: " + result1.error()));
        }
        
        if (!result1->is<kernel::Boolean>()) {
            return kernel::Value(std::make_shared<kernel::String>("First predicate must return boolean"));
        }
        
        if (result1->as<kernel::Boolean>()->value()) {
            return kernel::Value(kernel::Boolean::true_value());
        }
        
        // Evaluate second predicate
        auto result2 = kernel::apply(pred2, args);
        if (!result2.has_value()) {
            return kernel::Value(std::make_shared<kernel::String>("Error in second predicate: " + result2.error()));
        }
        
        if (!result2->is<kernel::Boolean>()) {
            return kernel::Value(std::make_shared<kernel::String>("Second predicate must return boolean"));
        }
        
        return kernel::Value(result2->as<kernel::Boolean>()->value() ? kernel::Boolean::true_value() : kernel::Boolean::false_value());
    };
    return kernel::Value(std::make_shared<kernel::Function>(
        std::vector<std::shared_ptr<kernel::Symbol>>{},
        kernel::Value(),
        impl,
        std::string("combined_or_predicate")));
}

kernel::Value RefinementMetaType::combine_multiple_predicates(
    const std::vector<std::shared_ptr<RefinementMetaType>>& refinements,
    CompositionType composition_type) {
    
    if (refinements.empty()) {
        throw std::invalid_argument("Cannot combine empty list of refinements");
    }
    
    if (refinements.size() == 1) {
        return refinements[0]->predicate();
    }
    
    // Start with the first predicate
    kernel::Value combined = refinements[0]->predicate();
    
    // Combine with the rest
    for (size_t i = 1; i < refinements.size(); ++i) {
        if (composition_type == CompositionType::AND) {
            combined = combine_predicates_and(combined, refinements[i]->predicate());
        } else {
            combined = combine_predicates_or(combined, refinements[i]->predicate());
        }
    }
    
    return combined;
}

} // namespace meld::meta

// Add missing TypeRegistry methods
namespace meld::meta {

bool TypeRegistry::is_registered(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return types_.find(name) != types_.end();
}

std::vector<std::string> TypeRegistry::get_all_type_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(types_.size());
    for (const auto& [name, _] : types_) {
        names.push_back(name);
    }
    return names;
}

} // namespace meld::meta
