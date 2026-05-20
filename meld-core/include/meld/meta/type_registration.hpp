#pragma once

#ifdef __APPLE__
#include "meld/meta/reflection_helper.hpp"  // pulls in rttr stubs
#else
#include <rttr/type>
#endif
#include <string>
#include <memory>
#include <vector>

namespace meld::meta {

/**
 * RttrTypeRegistry - Manages RTTR type registration for Meld types
 * 
 * This class provides a centralized system for registering Meld types
 * with the RTTR reflection library. It ensures all types are properly
 * registered at program startup.
 */
class RttrTypeRegistry {
public:
    /**
     * Get the singleton instance
     */
    static RttrTypeRegistry& instance();
    
    /**
     * Initialize all type registrations
     * 
     * This method should be called once at program startup to register
     * all Meld types with RTTR. It's safe to call multiple times.
     */
    void initialize();
    
    /**
     * Check if the registry has been initialized
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * Check if a type is registered
     */
    bool is_registered(const std::string& name) const;
    
    /**
     * Get RTTR type by name
     */
    rttr::type get_type(const std::string& name) const;
    
    /**
     * Get all registered type names
     */
    std::vector<std::string> get_all_type_names() const;
    
    /**
     * Get registration statistics
     */
    size_t get_type_count() const;
    
private:
    RttrTypeRegistry() = default;
    ~RttrTypeRegistry() = default;
    
    // Non-copyable, non-movable
    RttrTypeRegistry(const RttrTypeRegistry&) = delete;
    RttrTypeRegistry& operator=(const RttrTypeRegistry&) = delete;
    RttrTypeRegistry(RttrTypeRegistry&&) = delete;
    RttrTypeRegistry& operator=(RttrTypeRegistry&&) = delete;
    
    bool initialized_ = false;
};

/**
 * Initialize Meld type registration
 * 
 * This function should be called once at program startup to register
 * all Meld types with RTTR.
 */
void initialize_meld_types();

} // namespace meld::meta
