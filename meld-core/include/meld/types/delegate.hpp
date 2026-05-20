#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <functional>
#include <expected>
#include <memory>

namespace meld::types {

// Property delegate interface - objects that can handle property get/set
class PropertyDelegate {
public:
    virtual ~PropertyDelegate() = default;
    
    // Get the property value
    virtual std::expected<kernel::Value, std::string> get_value() = 0;
    
    // Set the property value
    virtual std::expected<void, std::string> set_value(const kernel::Value& value) = 0;
};

// Lazy delegate - computes value on first access and caches it
class LazyDelegate : public PropertyDelegate {
public:
    explicit LazyDelegate(std::function<kernel::Value()> initializer)
        : initializer_(std::move(initializer))
        , initialized_(false) {}
    
    std::expected<kernel::Value, std::string> get_value() override {
        if (!initialized_) {
            cached_value_ = initializer_();
            initialized_ = true;
        }
        return cached_value_;
    }
    
    std::expected<void, std::string> set_value(const kernel::Value& value) override {
        return std::unexpected("Cannot set value on lazy delegate");
    }
    
private:
    std::function<kernel::Value()> initializer_;
    kernel::Value cached_value_;
    bool initialized_;
};

// Observable delegate - notifies on value changes
class ObservableDelegate : public PropertyDelegate {
public:
    using ChangeCallback = std::function<void(const kernel::Value& old_value, const kernel::Value& new_value)>;
    
    ObservableDelegate(kernel::Value initial_value, ChangeCallback on_change)
        : value_(std::move(initial_value))
        , on_change_(std::move(on_change)) {}
    
    std::expected<kernel::Value, std::string> get_value() override {
        return value_;
    }
    
    std::expected<void, std::string> set_value(const kernel::Value& new_value) override {
        kernel::Value old_value = value_;
        value_ = new_value;
        if (on_change_) {
            on_change_(old_value, new_value);
        }
        return {};
    }
    
private:
    kernel::Value value_;
    ChangeCallback on_change_;
};

// Validated delegate - validates values before setting
class ValidatedDelegate : public PropertyDelegate {
public:
    using Validator = std::function<std::expected<void, std::string>(const kernel::Value&)>;
    
    ValidatedDelegate(kernel::Value initial_value, Validator validator)
        : value_(std::move(initial_value))
        , validator_(std::move(validator)) {}
    
    std::expected<kernel::Value, std::string> get_value() override {
        return value_;
    }
    
    std::expected<void, std::string> set_value(const kernel::Value& new_value) override {
        if (validator_) {
            auto validation_result = validator_(new_value);
            if (!validation_result.has_value()) {
                return std::unexpected(validation_result.error());
            }
        }
        value_ = new_value;
        return {};
    }
    
private:
    kernel::Value value_;
    Validator validator_;
};

// Mapped delegate - transforms values on get/set
class MappedDelegate : public PropertyDelegate {
public:
    using Getter = std::function<kernel::Value(const kernel::Value&)>;
    using Setter = std::function<kernel::Value(const kernel::Value&)>;
    
    MappedDelegate(kernel::Value initial_value, Getter getter, Setter setter)
        : value_(std::move(initial_value))
        , getter_(std::move(getter))
        , setter_(std::move(setter)) {}
    
    std::expected<kernel::Value, std::string> get_value() override {
        if (getter_) {
            return getter_(value_);
        }
        return value_;
    }
    
    std::expected<void, std::string> set_value(const kernel::Value& new_value) override {
        if (setter_) {
            value_ = setter_(new_value);
        } else {
            value_ = new_value;
        }
        return {};
    }
    
private:
    kernel::Value value_;
    Getter getter_;
    Setter setter_;
};

// Delegate factory - creates standard delegates
class DelegateFactory {
public:
    // Create a lazy delegate
    static std::shared_ptr<PropertyDelegate> lazy(std::function<kernel::Value()> initializer) {
        return std::make_shared<LazyDelegate>(std::move(initializer));
    }
    
    // Create an observable delegate
    static std::shared_ptr<PropertyDelegate> observable(
        kernel::Value initial_value,
        ObservableDelegate::ChangeCallback on_change) {
        return std::make_shared<ObservableDelegate>(std::move(initial_value), std::move(on_change));
    }
    
    // Create a validated delegate
    static std::shared_ptr<PropertyDelegate> validated(
        kernel::Value initial_value,
        ValidatedDelegate::Validator validator) {
        return std::make_shared<ValidatedDelegate>(std::move(initial_value), std::move(validator));
    }
    
    // Create a mapped delegate
    static std::shared_ptr<PropertyDelegate> mapped(
        kernel::Value initial_value,
        MappedDelegate::Getter getter,
        MappedDelegate::Setter setter) {
        return std::make_shared<MappedDelegate>(std::move(initial_value), std::move(getter), std::move(setter));
    }
};

} // namespace meld::types
