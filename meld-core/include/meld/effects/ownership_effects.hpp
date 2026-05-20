#pragma once

#include "meld/types/ownership.hpp"
#include "meld/ownership/provenance_integration.hpp"
#include "meld/kernel/primitives.hpp"
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <optional>
#include <functional>

namespace meld::effects {

// ============================================================================
// OWNERSHIP-AWARE ALGEBRAIC EFFECTS
// Task 13.2: Enhance algebraic effects with ownership
// Requirements: 9.2, 9.3
// ============================================================================

// Effect resource tracking
// Tracks resources managed by effect handlers
struct EffectResource {
    std::string resource_id;
    ownership::OwnershipState ownership_state;
    std::shared_ptr<ownership::OwnershipProvenance> provenance;
    bool is_active;
    
    EffectResource(std::string id, ownership::OwnershipState state,
                  std::shared_ptr<ownership::OwnershipProvenance> prov)
        : resource_id(std::move(id))
        , ownership_state(state)
        , provenance(std::move(prov))
        , is_active(true) {}
};

// Effect handler state
// Maintains ownership state within effect handlers
struct EffectHandlerState {
    std::string handler_name;
    std::vector<EffectResource> managed_resources;
    std::map<std::string, ownership::OwnershipState> parameter_ownership;
    std::optional<ownership::OwnershipState> return_ownership;
    
    EffectHandlerState(std::string name)
        : handler_name(std::move(name)) {}
};

// Ownership-aware effect handler
// Effect handler that tracks ownership of resources
class OwnershipAwareEffectHandler {
public:
    OwnershipAwareEffectHandler(std::string name,
                               ownership::ProvenanceAwareOwnershipTracker& tracker)
        : state_(std::move(name))
        , tracker_(tracker) {}
    
    // Register a resource with the handler
    void register_resource(const std::string& resource_id,
                          ownership::OwnershipState state,
                          std::shared_ptr<ownership::OwnershipProvenance> provenance);
    
    // Release a resource from the handler
    std::expected<void, std::string> release_resource(const std::string& resource_id);
    
    // Transfer ownership of a resource
    std::expected<void, std::string> transfer_ownership(
        const std::string& resource_id,
        ownership::OwnershipState new_state
    );
    
    // Check if resource is owned by handler
    bool owns_resource(const std::string& resource_id) const;
    
    // Get resource ownership state
    std::optional<ownership::OwnershipState> get_resource_ownership(
        const std::string& resource_id
    ) const;
    
    // Set parameter ownership
    void set_parameter_ownership(const std::string& param_name,
                                ownership::OwnershipState state);
    
    // Set return ownership
    void set_return_ownership(ownership::OwnershipState state);
    
    // Get handler state
    const EffectHandlerState& state() const { return state_; }
    
    // Validate handler ownership consistency
    std::expected<void, std::string> validate_ownership() const;
    
private:
    EffectHandlerState state_;
    ownership::ProvenanceAwareOwnershipTracker& tracker_;
};

// Effect operation with ownership
// Represents an effect operation with ownership tracking
struct EffectOperation {
    std::string effect_name;
    std::string operation_name;
    std::vector<std::pair<std::string, ownership::OwnershipState>> parameters;
    ownership::OwnershipState return_ownership;
    std::shared_ptr<ownership::OwnershipProvenance> provenance;
    
    EffectOperation(std::string eff, std::string op)
        : effect_name(std::move(eff))
        , operation_name(std::move(op))
        , return_ownership(ownership::OwnershipState::Owned) {}
};

// Effect-aware resource manager
// Manages resources across effect boundaries
class EffectAwareResourceManager {
public:
    EffectAwareResourceManager(ownership::ProvenanceAwareOwnershipTracker& tracker)
        : tracker_(tracker) {}
    
    // Acquire a resource through an effect
    std::expected<std::string, std::string> acquire_resource(
        const std::string& effect_name,
        ownership::OwnershipState ownership,
        std::shared_ptr<ownership::OwnershipProvenance> provenance
    );
    
    // Release a resource through an effect
    std::expected<void, std::string> release_resource(
        const std::string& resource_id,
        const std::string& effect_name
    );
    
    // Transfer resource between effects
    std::expected<void, std::string> transfer_resource(
        const std::string& resource_id,
        const std::string& from_effect,
        const std::string& to_effect
    );
    
    // Check resource ownership
    std::optional<ownership::OwnershipState> get_resource_ownership(
        const std::string& resource_id
    ) const;
    
    // Get resources managed by effect
    std::vector<std::string> get_effect_resources(const std::string& effect_name) const;
    
    // Validate all resources
    std::expected<void, std::string> validate_all_resources() const;
    
private:
    ownership::ProvenanceAwareOwnershipTracker& tracker_;
    std::map<std::string, std::string> resource_to_effect_;  // resource_id -> effect_name
    std::map<std::string, std::vector<std::string>> effect_to_resources_;  // effect_name -> resource_ids
    size_t next_resource_id_ = 0;
};

// Compile-time effect tracking
// Tracks effects at compile time with ownership information
class CompileTimeEffectTracker {
public:
    // Register an effect with ownership requirements
    void register_effect(const std::string& effect_name,
                        const EffectOperation& operation);
    
    // Check if effect is registered
    bool is_effect_registered(const std::string& effect_name) const;
    
    // Get effect operation
    std::optional<EffectOperation> get_effect_operation(
        const std::string& effect_name,
        const std::string& operation_name
    ) const;
    
    // Validate effect usage
    std::expected<void, std::string> validate_effect_usage(
        const std::string& effect_name,
        const std::vector<ownership::OwnershipState>& arg_ownership
    ) const;
    
    // Infer effect ownership requirements
    std::expected<EffectOperation, std::string> infer_effect_ownership(
        const std::string& effect_name,
        const kernel::Value& ast_node
    );
    
    // Get all registered effects
    std::vector<std::string> get_registered_effects() const;
    
private:
    std::map<std::string, std::vector<EffectOperation>> effects_;
};

// Effect handler with automatic cleanup
// RAII-style effect handler that ensures resource cleanup
class ScopedEffectHandler {
public:
    ScopedEffectHandler(std::string name,
                       OwnershipAwareEffectHandler& handler,
                       EffectAwareResourceManager& manager)
        : name_(std::move(name))
        , handler_(handler)
        , manager_(manager) {}
    
    ~ScopedEffectHandler() {
        // Automatic cleanup on scope exit
        cleanup();
    }
    
    // Disable copy
    ScopedEffectHandler(const ScopedEffectHandler&) = delete;
    ScopedEffectHandler& operator=(const ScopedEffectHandler&) = delete;
    
    // Enable move
    ScopedEffectHandler(ScopedEffectHandler&&) = default;
    ScopedEffectHandler& operator=(ScopedEffectHandler&&) = default;
    
    // Get handler
    OwnershipAwareEffectHandler& handler() { return handler_; }
    
    // Manual cleanup
    void cleanup();
    
private:
    std::string name_;
    OwnershipAwareEffectHandler& handler_;
    EffectAwareResourceManager& manager_;
    bool cleaned_up_ = false;
};

// Effect composition with ownership
// Composes multiple effects while preserving ownership
class EffectComposition {
public:
    // Add an effect to the composition
    void add_effect(const std::string& effect_name,
                   const EffectOperation& operation);
    
    // Compose effects
    std::expected<EffectOperation, std::string> compose() const;
    
    // Validate composition
    std::expected<void, std::string> validate() const;
    
    // Get composed ownership requirements
    std::vector<ownership::OwnershipState> get_parameter_ownership() const;
    
    ownership::OwnershipState get_return_ownership() const;
    
private:
    std::vector<std::pair<std::string, EffectOperation>> effects_;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Create effect operation with ownership
EffectOperation create_effect_operation(
    const std::string& effect_name,
    const std::string& operation_name,
    const std::vector<std::pair<std::string, ownership::OwnershipState>>& params,
    ownership::OwnershipState return_ownership
);

// Validate effect handler ownership
std::expected<void, std::string> validate_effect_handler_ownership(
    const OwnershipAwareEffectHandler& handler
);

// Check if effect operation is safe
bool is_effect_operation_safe(
    const EffectOperation& operation,
    const std::vector<ownership::OwnershipState>& arg_ownership
);

// Infer effect ownership from usage
std::expected<EffectOperation, std::string> infer_effect_ownership_from_usage(
    const std::string& effect_name,
    const kernel::Value& usage_ast
);

// Merge effect ownership requirements
std::expected<EffectOperation, std::string> merge_effect_operations(
    const std::vector<EffectOperation>& operations
);

// Serialize effect operation
std::string serialize_effect_operation(const EffectOperation& operation);

// Visualize effect resource ownership
std::string visualize_effect_resources(
    const EffectAwareResourceManager& manager,
    const std::string& effect_name
);

} // namespace meld::effects
