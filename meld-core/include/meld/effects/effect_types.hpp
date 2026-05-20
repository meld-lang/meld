#pragma once

#include "effect.hpp"
#include <set>
#include <memory>
#include <string>
#include <vector>

namespace meld::effects {

// Base Effect trait - all effects inherit from this
class Effect {
public:
    virtual ~Effect() = default;
    virtual std::string name() const = 0;
    virtual bool equals(const Effect& other) const = 0;
    virtual std::unique_ptr<Effect> clone() const = 0;
    
    // Comparison operators
    bool operator==(const Effect& other) const {
        return equals(other);
    }
    
    bool operator!=(const Effect& other) const {
        return !equals(other);
    }
};

// Pure effect - represents no side effects
class EffectPure : public Effect {
public:
    std::string name() const override {
        return "Pure";
    }
    
    bool equals(const Effect& other) const override {
        return dynamic_cast<const EffectPure*>(&other) != nullptr;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<EffectPure>();
    }
    
    // Singleton instance
    static const EffectPure& instance() {
        static EffectPure pure;
        return pure;
    }
};

// IO effect - represents file system and console operations
class EffectIO : public Effect {
public:
    std::string name() const override {
        return "IO";
    }
    
    bool equals(const Effect& other) const override {
        return dynamic_cast<const EffectIO*>(&other) != nullptr;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<EffectIO>();
    }
    
    // Singleton instance
    static const EffectIO& instance() {
        static EffectIO io;
        return io;
    }
};

// Network effect - represents network operations
class EffectNetwork : public Effect {
public:
    std::string name() const override {
        return "Network";
    }
    
    bool equals(const Effect& other) const override {
        return dynamic_cast<const EffectNetwork*>(&other) != nullptr;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<EffectNetwork>();
    }
    
    // Singleton instance
    static const EffectNetwork& instance() {
        static EffectNetwork network;
        return network;
    }
};

// State effect - represents mutable state access
class EffectState : public Effect {
public:
    std::string name() const override {
        return "State";
    }
    
    bool equals(const Effect& other) const override {
        return dynamic_cast<const EffectState*>(&other) != nullptr;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<EffectState>();
    }
    
    // Singleton instance
    static const EffectState& instance() {
        static EffectState state;
        return state;
    }
};

// Time effect - represents time-dependent operations
class EffectTime : public Effect {
public:
    std::string name() const override {
        return "Time";
    }
    
    bool equals(const Effect& other) const override {
        return dynamic_cast<const EffectTime*>(&other) != nullptr;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<EffectTime>();
    }
    
    // Singleton instance
    static const EffectTime& instance() {
        static EffectTime time;
        return time;
    }
};

// Custom user-defined effect
class CustomEffect : public Effect {
public:
    explicit CustomEffect(std::string name) : name_(std::move(name)) {}
    
    std::string name() const override {
        return name_;
    }
    
    bool equals(const Effect& other) const override {
        const auto* custom = dynamic_cast<const CustomEffect*>(&other);
        return custom != nullptr && custom->name_ == name_;
    }
    
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<CustomEffect>(name_);
    }
    
private:
    std::string name_;
};

// Effect set for composing multiple effects
class EffectSet {
public:
    EffectSet() = default;
    
    // Create from single effect
    explicit EffectSet(const Effect& effect) {
        add(effect);
    }
    
    // Create from multiple effects
    EffectSet(std::initializer_list<std::reference_wrapper<const Effect>> effects) {
        for (const auto& effect : effects) {
            add(effect.get());
        }
    }
    
    // Add an effect to the set
    void add(const Effect& effect) {
        // Don't add Pure effect to sets (it's the identity)
        if (dynamic_cast<const EffectPure*>(&effect) != nullptr) {
            return;
        }
        
        // Check if effect already exists
        for (const auto& existing : effects_) {
            if (existing->equals(effect)) {
                return; // Already present
            }
        }
        
        effects_.push_back(effect.clone());
    }
    
    // Remove an effect from the set
    void remove(const Effect& effect) {
        effects_.erase(
            std::remove_if(effects_.begin(), effects_.end(),
                [&effect](const std::unique_ptr<Effect>& e) {
                    return e->equals(effect);
                }),
            effects_.end()
        );
    }
    
    // Check if effect is in the set
    bool contains(const Effect& effect) const {
        for (const auto& existing : effects_) {
            if (existing->equals(effect)) {
                return true;
            }
        }
        return false;
    }
    
    // Check if this set is pure (empty)
    bool is_pure() const {
        return effects_.empty();
    }
    
    // Get all effects
    const std::vector<std::unique_ptr<Effect>>& effects() const {
        return effects_;
    }
    
    // Union with another effect set
    EffectSet union_with(const EffectSet& other) const {
        EffectSet result = *this;
        for (const auto& effect : other.effects_) {
            result.add(*effect);
        }
        return result;
    }
    
    // Check if this set is a subset of another
    bool is_subset_of(const EffectSet& other) const {
        for (const auto& effect : effects_) {
            if (!other.contains(*effect)) {
                return false;
            }
        }
        return true;
    }
    
    // Equality comparison
    bool equals(const EffectSet& other) const {
        if (effects_.size() != other.effects_.size()) {
            return false;
        }
        
        for (const auto& effect : effects_) {
            if (!other.contains(*effect)) {
                return false;
            }
        }
        
        return true;
    }
    
    bool operator==(const EffectSet& other) const {
        return equals(other);
    }
    
    bool operator!=(const EffectSet& other) const {
        return !equals(other);
    }
    
    // Copy constructor
    EffectSet(const EffectSet& other) {
        for (const auto& effect : other.effects_) {
            effects_.push_back(effect->clone());
        }
    }
    
    // Assignment operator
    EffectSet& operator=(const EffectSet& other) {
        if (this != &other) {
            effects_.clear();
            for (const auto& effect : other.effects_) {
                effects_.push_back(effect->clone());
            }
        }
        return *this;
    }
    
    // Move constructor
    EffectSet(EffectSet&& other) noexcept = default;
    
    // Move assignment
    EffectSet& operator=(EffectSet&& other) noexcept = default;
    
    // String representation for debugging
    std::string to_string() const {
        if (is_pure()) {
            return "Pure";
        }
        
        std::string result = "{";
        for (size_t i = 0; i < effects_.size(); ++i) {
            if (i > 0) result += ", ";
            result += effects_[i]->name();
        }
        result += "}";
        return result;
    }
    
private:
    std::vector<std::unique_ptr<Effect>> effects_;
};

// Helper functions for creating effect sets
inline EffectSet pure_effect() {
    return EffectSet();
}

inline EffectSet io_effect() {
    return EffectSet(EffectIO::instance());
}

inline EffectSet network_effect() {
    return EffectSet(EffectNetwork::instance());
}

inline EffectSet state_effect() {
    return EffectSet(EffectState::instance());
}

inline EffectSet time_effect() {
    return EffectSet(EffectTime::instance());
}

inline EffectSet custom_effect(const std::string& name) {
    return EffectSet(CustomEffect(name));
}

// Combine multiple effects
template<typename... Effects>
EffectSet combine_effects(const Effects&... effects) {
    EffectSet result;
    (result.add(effects), ...);
    return result;
}

} // namespace meld::effects