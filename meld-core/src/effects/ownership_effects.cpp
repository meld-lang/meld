#include "meld/effects/ownership_effects.hpp"
#include <sstream>
#include <algorithm>
#include <format>

namespace meld::effects {

// ============================================================================
// OwnershipAwareEffectHandler Implementation
// ============================================================================

void OwnershipAwareEffectHandler::register_resource(
    const std::string& resource_id,
    ownership::OwnershipState state,
    std::shared_ptr<ownership::OwnershipProvenance> provenance) {
    
    EffectResource resource(resource_id, state, std::move(provenance));
    state_.managed_resources.push_back(std::move(resource));
    
    // Track in ownership tracker
    tracker_.track_ownership(resource_id, state, provenance);
}

std::expected<void, std::string> 
OwnershipAwareEffectHandler::release_resource(const std::string& resource_id) {
    auto it = std::find_if(state_.managed_resources.begin(),
                          state_.managed_resources.end(),
                          [&](const EffectResource& r) {
                              return r.resource_id == resource_id && r.is_active;
                          });
    
    if (it == state_.managed_resources.end()) {
        return std::unexpected("Resource not found or already released: " + resource_id);
    }
    
    it->is_active = false;
    return {};
}

std::expected<void, std::string> 
OwnershipAwareEffectHandler::transfer_ownership(
    const std::string& resource_id,
    ownership::OwnershipState new_state) {
    
    auto it = std::find_if(state_.managed_resources.begin(),
                          state_.managed_resources.end(),
                          [&](const EffectResource& r) {
                              return r.resource_id == resource_id && r.is_active;
                          });
    
    if (it == state_.managed_resources.end()) {
        return std::unexpected("Resource not found: " + resource_id);
    }
    
    it->ownership_state = new_state;
    tracker_.track_ownership(resource_id, new_state, it->provenance);
    
    return {};
}

bool OwnershipAwareEffectHandler::owns_resource(const std::string& resource_id) const {
    return std::any_of(state_.managed_resources.begin(),
                      state_.managed_resources.end(),
                      [&](const EffectResource& r) {
                          return r.resource_id == resource_id && r.is_active;
                      });
}

std::optional<ownership::OwnershipState> 
OwnershipAwareEffectHandler::get_resource_ownership(
    const std::string& resource_id) const {
    
    auto it = std::find_if(state_.managed_resources.begin(),
                          state_.managed_resources.end(),
                          [&](const EffectResource& r) {
                              return r.resource_id == resource_id && r.is_active;
                          });
    
    if (it != state_.managed_resources.end()) {
        return it->ownership_state;
    }
    
    return std::nullopt;
}

void OwnershipAwareEffectHandler::set_parameter_ownership(
    const std::string& param_name,
    ownership::OwnershipState state) {
    
    state_.parameter_ownership[param_name] = state;
}

void OwnershipAwareEffectHandler::set_return_ownership(
    ownership::OwnershipState state) {
    
    state_.return_ownership = state;
}

std::expected<void, std::string> 
OwnershipAwareEffectHandler::validate_ownership() const {
    // Check that all active resources have valid ownership
    for (const auto& resource : state_.managed_resources) {
        if (resource.is_active) {
            if (!resource.provenance) {
                return std::unexpected(
                    "Resource missing provenance: " + resource.resource_id
                );
            }
            
            auto validation = ownership::validate_ownership_provenance(*resource.provenance);
            if (!validation) {
                return std::unexpected(validation.error());
            }
        }
    }
    
    return {};
}

// ============================================================================
// EffectAwareResourceManager Implementation
// ============================================================================

std::expected<std::string, std::string> 
EffectAwareResourceManager::acquire_resource(
    const std::string& effect_name,
    ownership::OwnershipState ownership,
    std::shared_ptr<ownership::OwnershipProvenance> provenance) {
    
    // Generate unique resource ID
    std::string resource_id = std::format("resource_{}", next_resource_id_++);
    
    // Track resource
    resource_to_effect_[resource_id] = effect_name;
    effect_to_resources_[effect_name].push_back(resource_id);
    
    // Track ownership
    tracker_.track_ownership(resource_id, ownership, std::move(provenance));
    
    return resource_id;
}

std::expected<void, std::string> 
EffectAwareResourceManager::release_resource(
    const std::string& resource_id,
    const std::string& effect_name) {
    
    // Check if resource exists
    auto it = resource_to_effect_.find(resource_id);
    if (it == resource_to_effect_.end()) {
        return std::unexpected("Resource not found: " + resource_id);
    }
    
    // Check if effect owns resource
    if (it->second != effect_name) {
        return std::unexpected(
            std::format("Effect {} does not own resource {}",
                       effect_name, resource_id)
        );
    }
    
    // Remove from tracking
    resource_to_effect_.erase(it);
    
    auto& resources = effect_to_resources_[effect_name];
    resources.erase(std::remove(resources.begin(), resources.end(), resource_id),
                   resources.end());
    
    return {};
}

std::expected<void, std::string> 
EffectAwareResourceManager::transfer_resource(
    const std::string& resource_id,
    const std::string& from_effect,
    const std::string& to_effect) {
    
    // Check if resource exists
    auto it = resource_to_effect_.find(resource_id);
    if (it == resource_to_effect_.end()) {
        return std::unexpected("Resource not found: " + resource_id);
    }
    
    // Check if from_effect owns resource
    if (it->second != from_effect) {
        return std::unexpected(
            std::format("Effect {} does not own resource {}",
                       from_effect, resource_id)
        );
    }
    
    // Transfer ownership
    it->second = to_effect;
    
    // Update effect resource lists
    auto& from_resources = effect_to_resources_[from_effect];
    from_resources.erase(std::remove(from_resources.begin(), from_resources.end(), resource_id),
                        from_resources.end());
    
    effect_to_resources_[to_effect].push_back(resource_id);
    
    return {};
}

std::optional<ownership::OwnershipState> 
EffectAwareResourceManager::get_resource_ownership(
    const std::string& resource_id) const {
    
    // Check if resource exists
    if (!resource_to_effect_.contains(resource_id)) {
        return std::nullopt;
    }
    
    // Get ownership from tracker
    auto prov = tracker_.get_provenance(resource_id);
    if (!prov) {
        return std::nullopt;
    }
    
    // In a full implementation, would extract ownership state from provenance
    return ownership::OwnershipState::Owned;
}

std::vector<std::string> 
EffectAwareResourceManager::get_effect_resources(const std::string& effect_name) const {
    auto it = effect_to_resources_.find(effect_name);
    if (it != effect_to_resources_.end()) {
        return it->second;
    }
    return {};
}

std::expected<void, std::string> 
EffectAwareResourceManager::validate_all_resources() const {
    // Validate all tracked resources
    for (const auto& [resource_id, effect_name] : resource_to_effect_) {
        auto result = tracker_.validate_with_confidence(resource_id, 0.8f);
        if (!result) {
            return std::unexpected(
                std::format("Resource {} validation failed: {}",
                           resource_id, result.error())
            );
        }
    }
    
    return {};
}

// ============================================================================
// CompileTimeEffectTracker Implementation
// ============================================================================

void CompileTimeEffectTracker::register_effect(
    const std::string& effect_name,
    const EffectOperation& operation) {
    
    effects_[effect_name].push_back(operation);
}

bool CompileTimeEffectTracker::is_effect_registered(
    const std::string& effect_name) const {
    
    return effects_.contains(effect_name);
}

std::optional<EffectOperation> 
CompileTimeEffectTracker::get_effect_operation(
    const std::string& effect_name,
    const std::string& operation_name) const {
    
    auto it = effects_.find(effect_name);
    if (it == effects_.end()) {
        return std::nullopt;
    }
    
    for (const auto& op : it->second) {
        if (op.operation_name == operation_name) {
            return op;
        }
    }
    
    return std::nullopt;
}

std::expected<void, std::string> 
CompileTimeEffectTracker::validate_effect_usage(
    const std::string& effect_name,
    const std::vector<ownership::OwnershipState>& arg_ownership) const {
    
    auto it = effects_.find(effect_name);
    if (it == effects_.end()) {
        return std::unexpected("Effect not registered: " + effect_name);
    }
    
    // In a full implementation, would validate argument ownership
    // matches effect requirements
    
    return {};
}

std::expected<EffectOperation, std::string> 
CompileTimeEffectTracker::infer_effect_ownership(
    const std::string& effect_name,
    const kernel::Value& ast_node) {
    
    // In a full implementation, would analyze AST to infer ownership
    EffectOperation operation(effect_name, "inferred");
    operation.return_ownership = ownership::OwnershipState::Owned;
    
    return operation;
}

std::vector<std::string> 
CompileTimeEffectTracker::get_registered_effects() const {
    std::vector<std::string> effect_names;
    for (const auto& [name, _] : effects_) {
        effect_names.push_back(name);
    }
    return effect_names;
}

// ============================================================================
// ScopedEffectHandler Implementation
// ============================================================================

void ScopedEffectHandler::cleanup() {
    if (cleaned_up_) {
        return;
    }
    
    // Release all resources managed by this handler
    auto resources = manager_.get_effect_resources(name_);
    for (const auto& resource_id : resources) {
        manager_.release_resource(resource_id, name_);
    }
    
    cleaned_up_ = true;
}

// ============================================================================
// EffectComposition Implementation
// ============================================================================

void EffectComposition::add_effect(
    const std::string& effect_name,
    const EffectOperation& operation) {
    
    effects_.push_back({effect_name, operation});
}

std::expected<EffectOperation, std::string> 
EffectComposition::compose() const {
    if (effects_.empty()) {
        return std::unexpected("No effects to compose");
    }
    
    // Create composed operation
    EffectOperation composed("composed", "composed_op");
    
    // Merge parameters from all effects
    for (const auto& [name, op] : effects_) {
        composed.parameters.insert(composed.parameters.end(),
                                  op.parameters.begin(),
                                  op.parameters.end());
    }
    
    // Use return ownership from last effect
    composed.return_ownership = effects_.back().second.return_ownership;
    
    return composed;
}

std::expected<void, std::string> 
EffectComposition::validate() const {
    // Validate that effects can be composed
    // Check for ownership conflicts
    
    for (size_t i = 0; i < effects_.size(); ++i) {
        for (size_t j = i + 1; j < effects_.size(); ++j) {
            // In a full implementation, would check for conflicts
        }
    }
    
    return {};
}

std::vector<ownership::OwnershipState> 
EffectComposition::get_parameter_ownership() const {
    std::vector<ownership::OwnershipState> ownership;
    
    for (const auto& [name, op] : effects_) {
        for (const auto& [param_name, param_ownership] : op.parameters) {
            ownership.push_back(param_ownership);
        }
    }
    
    return ownership;
}

ownership::OwnershipState EffectComposition::get_return_ownership() const {
    if (effects_.empty()) {
        return ownership::OwnershipState::Owned;
    }
    
    return effects_.back().second.return_ownership;
}

// ============================================================================
// Helper Functions
// ============================================================================

EffectOperation create_effect_operation(
    const std::string& effect_name,
    const std::string& operation_name,
    const std::vector<std::pair<std::string, ownership::OwnershipState>>& params,
    ownership::OwnershipState return_ownership) {
    
    EffectOperation operation(effect_name, operation_name);
    operation.parameters = params;
    operation.return_ownership = return_ownership;
    
    return operation;
}

std::expected<void, std::string> validate_effect_handler_ownership(
    const OwnershipAwareEffectHandler& handler) {
    
    return handler.validate_ownership();
}

bool is_effect_operation_safe(
    const EffectOperation& operation,
    const std::vector<ownership::OwnershipState>& arg_ownership) {
    
    // Check if argument ownership matches operation requirements
    if (arg_ownership.size() != operation.parameters.size()) {
        return false;
    }
    
    for (size_t i = 0; i < arg_ownership.size(); ++i) {
        if (arg_ownership[i] != operation.parameters[i].second) {
            return false;
        }
    }
    
    return true;
}

std::expected<EffectOperation, std::string> infer_effect_ownership_from_usage(
    const std::string& effect_name,
    const kernel::Value& usage_ast) {
    
    // In a full implementation, would analyze usage AST
    EffectOperation operation(effect_name, "inferred");
    operation.return_ownership = ownership::OwnershipState::Owned;
    
    return operation;
}

std::expected<EffectOperation, std::string> merge_effect_operations(
    const std::vector<EffectOperation>& operations) {
    
    if (operations.empty()) {
        return std::unexpected("No operations to merge");
    }
    
    EffectOperation merged("merged", "merged_op");
    
    // Merge parameters
    for (const auto& op : operations) {
        merged.parameters.insert(merged.parameters.end(),
                                op.parameters.begin(),
                                op.parameters.end());
    }
    
    // Use return ownership from last operation
    merged.return_ownership = operations.back().return_ownership;
    
    return merged;
}

std::string serialize_effect_operation(const EffectOperation& operation) {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"effect_name\": \"" << operation.effect_name << "\",\n";
    oss << "  \"operation_name\": \"" << operation.operation_name << "\",\n";
    oss << "  \"parameters\": [\n";
    
    for (size_t i = 0; i < operation.parameters.size(); ++i) {
        const auto& [name, ownership] = operation.parameters[i];
        oss << "    {\"name\": \"" << name << "\", \"ownership\": \"";
        oss << (ownership == ownership::OwnershipState::Owned ? "Owned" : "Borrowed");
        oss << "\"}";
        if (i < operation.parameters.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    
    oss << "  ],\n";
    oss << "  \"return_ownership\": \"";
    oss << (operation.return_ownership == ownership::OwnershipState::Owned ? "Owned" : "Borrowed");
    oss << "\"\n";
    oss << "}";
    
    return oss.str();
}

std::string visualize_effect_resources(
    const EffectAwareResourceManager& manager,
    const std::string& effect_name) {
    
    std::ostringstream oss;
    
    oss << "Effect: " << effect_name << "\n";
    oss << "Resources:\n";
    
    auto resources = manager.get_effect_resources(effect_name);
    for (const auto& resource_id : resources) {
        oss << "  - " << resource_id;
        
        auto ownership = manager.get_resource_ownership(resource_id);
        if (ownership) {
            oss << " (";
            oss << (*ownership == ownership::OwnershipState::Owned ? "Owned" : "Borrowed");
            oss << ")";
        }
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace meld::effects
