#pragma once

#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include "memory.hpp"
#include <memory>
#include <map>
#include <expected>
#include <format>

namespace meld::types {

// Forward declarations
class TypeInstance;
class StructInstance;
class ClassInstance;

// Copyable interface
class Copyable {
public:
    virtual ~Copyable() = default;
    virtual std::shared_ptr<TypeInstance> copy() const = 0;
};

// Base class for all type instances
class TypeInstance {
public:
    virtual ~TypeInstance() = default;
    
    virtual std::shared_ptr<meta::MetaType> get_type() const = 0;
    virtual kernel::Value to_value() const = 0;
    
protected:
    TypeInstance() = default;
};

// Struct instance - value type with copy-by-value semantics
// Structs are automatically Copyable
class StructInstance : public TypeInstance, public Copyable {
public:
    explicit StructInstance(std::shared_ptr<meta::StructMetaType> type)
        : type_(std::move(type)) {}
    
    std::shared_ptr<meta::MetaType> get_type() const override {
        return type_;
    }
    
    kernel::Value to_value() const override;
    
    // Field access
    std::expected<kernel::Value, std::string> get_field(const std::string& name) const;
    std::expected<void, std::string> set_field(const std::string& name, kernel::Value value);
    
    // Copy semantics (automatic for structs)
    std::shared_ptr<StructInstance> copy_struct() const;
    
    // Copyable interface implementation
    std::shared_ptr<TypeInstance> copy() const override {
        return copy_struct();
    }
    
private:
    std::shared_ptr<meta::StructMetaType> type_;
    std::map<std::string, kernel::Value> fields_;
};

// Class instance - reference type with managed memory
// Classes can optionally implement Copyable
class ClassInstance : public TypeInstance, 
                     public ManagedObject,
                     public std::enable_shared_from_this<ClassInstance> {
public:
    explicit ClassInstance(std::shared_ptr<meta::ClassMetaType> type)
        : type_(std::move(type)), is_copyable_(false) {}
    
    std::shared_ptr<meta::MetaType> get_type() const override {
        return type_;
    }
    
    kernel::Value to_value() const override;
    
    // Field access
    std::expected<kernel::Value, std::string> get_field(const std::string& name) const;
    std::expected<void, std::string> set_field(const std::string& name, kernel::Value value);
    
    // Weak reference support for breaking cycles
    void set_weak_field(const std::string& name, WeakRef<ClassInstance> weak_ref);
    std::expected<WeakRef<ClassInstance>, std::string> get_weak_field(const std::string& name) const;
    
    // Copyable support (optional for classes)
    void set_copyable(bool copyable) { is_copyable_ = copyable; }
    bool is_copyable() const { return is_copyable_; }
    
    // Copy method (must be explicitly implemented for classes)
    std::shared_ptr<ClassInstance> copy_class() const;
    
protected:
    void on_deallocate() override;
    
private:
    std::shared_ptr<meta::ClassMetaType> type_;
    std::map<std::string, kernel::Value> fields_;
    std::map<std::string, WeakRef<ClassInstance>> weak_fields_;
    bool is_copyable_;
};

// Factory functions
std::shared_ptr<StructInstance> create_struct_instance(std::shared_ptr<meta::StructMetaType> type);
std::shared_ptr<ClassInstance> create_class_instance(std::shared_ptr<meta::ClassMetaType> type);

// Helper to extract instance from Value
std::expected<std::shared_ptr<StructInstance>, std::string> 
as_struct_instance(const kernel::Value& value);

std::expected<std::shared_ptr<ClassInstance>, std::string> 
as_class_instance(const kernel::Value& value);

} // namespace meld::types
