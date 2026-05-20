#include "meld/types/instance.hpp"
#include <algorithm>

namespace meld::types {

// StructInstance implementation
kernel::Value StructInstance::to_value() const {
    return kernel::Value(std::make_shared<kernel::Symbol>("struct:" + type_->name()));
}

std::expected<kernel::Value, std::string> 
StructInstance::get_field(const std::string& name) const {
    // Check if field exists in type
    auto field_result = type_->get_field(name);
    if (!field_result) {
        return std::unexpected(field_result.error());
    }
    
    // Get field value
    auto it = fields_.find(name);
    if (it == fields_.end()) {
        return std::unexpected(std::format("Field '{}' not initialized", name));
    }
    
    return it->second;
}

std::expected<void, std::string> 
StructInstance::set_field(const std::string& name, kernel::Value value) {
    // Check if field exists and is mutable
    auto field_result = type_->get_field(name);
    if (!field_result) {
        return std::unexpected(field_result.error());
    }
    
    const auto* field = *field_result;
    if (!field->is_mutable) {
        return std::unexpected(std::format("Field '{}' is immutable", name));
    }
    
    fields_[name] = std::move(value);
    return {};
}

std::shared_ptr<StructInstance> StructInstance::copy_struct() const {
    auto new_instance = std::make_shared<StructInstance>(type_);
    new_instance->fields_ = fields_; // Deep copy of fields
    return new_instance;
}

// ClassInstance implementation
kernel::Value ClassInstance::to_value() const {
    return kernel::Value(std::make_shared<kernel::Symbol>("class:" + type_->name()));
}

std::expected<kernel::Value, std::string> 
ClassInstance::get_field(const std::string& name) const {
    // Check if field exists in type (including base classes)
    auto field_result = type_->get_field(name);
    if (!field_result) {
        return std::unexpected(field_result.error());
    }
    
    // Get field value
    auto it = fields_.find(name);
    if (it == fields_.end()) {
        return std::unexpected(std::format("Field '{}' not initialized", name));
    }
    
    return it->second;
}

std::expected<void, std::string> 
ClassInstance::set_field(const std::string& name, kernel::Value value) {
    // Check if field exists and is mutable
    auto field_result = type_->get_field(name);
    if (!field_result) {
        return std::unexpected(field_result.error());
    }
    
    const auto* field = *field_result;
    if (!field->is_mutable) {
        return std::unexpected(std::format("Field '{}' is immutable", name));
    }
    
    fields_[name] = std::move(value);
    return {};
}

void ClassInstance::set_weak_field(const std::string& name, WeakRef<ClassInstance> weak_ref) {
    weak_fields_[name] = std::move(weak_ref);
}

std::expected<WeakRef<ClassInstance>, std::string> 
ClassInstance::get_weak_field(const std::string& name) const {
    auto it = weak_fields_.find(name);
    if (it == weak_fields_.end()) {
        return std::unexpected(std::format("Weak field '{}' not found", name));
    }
    return it->second;
}

void ClassInstance::on_deallocate() {
    // Cleanup when object is deallocated
    fields_.clear();
    weak_fields_.clear();
    HeapAllocator::instance().unregister_allocation(this);
}

std::shared_ptr<ClassInstance> ClassInstance::copy_class() const {
    if (!is_copyable_) {
        throw std::runtime_error("Class is not copyable");
    }
    
    // Create new instance
    auto new_instance = create_class_instance(type_);
    
    // Copy fields (shallow copy of values)
    new_instance->fields_ = fields_;
    new_instance->is_copyable_ = is_copyable_;
    
    // Note: weak_fields are not copied (they reference other objects)
    
    return new_instance;
}

// Factory functions
std::shared_ptr<StructInstance> create_struct_instance(std::shared_ptr<meta::StructMetaType> type) {
    return std::make_shared<StructInstance>(std::move(type));
}

std::shared_ptr<ClassInstance> create_class_instance(std::shared_ptr<meta::ClassMetaType> type) {
    // Use heap allocator for managed memory
    return HeapAllocator::instance().allocate<ClassInstance>(std::move(type));
}

// Helper functions
std::expected<std::shared_ptr<StructInstance>, std::string> 
as_struct_instance(const kernel::Value& value) {
    // In a full implementation, this would check the value's tag
    // For now, we'll return an error
    return std::unexpected(std::string("Value is not a struct instance"));
}

std::expected<std::shared_ptr<ClassInstance>, std::string> 
as_class_instance(const kernel::Value& value) {
    // In a full implementation, this would check the value's tag
    // For now, we'll return an error
    return std::unexpected(std::string("Value is not a class instance"));
}

} // namespace meld::types
