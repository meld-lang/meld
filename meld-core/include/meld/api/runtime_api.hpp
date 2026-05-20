#pragma once

/**
 * @file runtime_api.hpp
 * @brief Public API for Meld runtime functionality
 * 
 * This header provides the main interface for other packages to interact
 * with the Meld runtime system.
 */

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/types/instance.hpp"

namespace meld::api {

/**
 * @brief Main runtime interface for external packages
 */
class RuntimeAPI {
public:
    /**
     * @brief Initialize the Meld runtime system
     * @return True if initialization succeeded
     */
    static bool initialize();
    
    /**
     * @brief Shutdown the Meld runtime system
     */
    static void shutdown();
    
    /**
     * @brief Execute Meld code in the runtime
     * @param code The compiled code to execute
     * @return Execution result
     */
    static bool execute(const std::string& code);
    
    /**
     * @brief Get runtime statistics
     * @return Runtime statistics as key-value pairs
     */
    static std::map<std::string, std::string> get_stats();
};

} // namespace meld::api