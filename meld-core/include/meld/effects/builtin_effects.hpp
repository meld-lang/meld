#pragma once

#include "effect.hpp"
#include <memory>
#include <vector>

namespace meld::effects {

// ============================================================================
// BUILT-IN EFFECT HANDLERS
// Task 35.12: Implement built-in effects
// Requirements: 41.19
// ============================================================================

// FileSystem effect handler
// Provides: read, write, delete, exists operations
std::shared_ptr<EffectHandler> create_filesystem_handler();

// Network effect handler  
// Provides: get, post operations
std::shared_ptr<EffectHandler> create_network_handler();

// Console effect handler
// Provides: print, println, readLine operations
std::shared_ptr<EffectHandler> create_console_handler();

// Random effect handler
// Provides: nextInt, nextFloat operations
std::shared_ptr<EffectHandler> create_random_handler();

// Seedable random effect handler (AI_DX Req 141)
// Provides: nextInt, nextFloat, reseed operations
// When seed is provided, produces deterministic sequences
std::shared_ptr<EffectHandler> create_seedable_random_handler(int64_t seed);

// Time effect handler
// Provides: now, sleep operations
std::shared_ptr<EffectHandler> create_time_handler();

// Virtual time effect handler (AI_DX Req 142)
// Provides: now, sleep, advance, set_origin operations
// Clock does not advance automatically — only via explicit advance() calls
std::shared_ptr<EffectHandler> create_virtual_time_handler(int64_t origin_ms = 0);

// Exception effect definition and handler
// Provides: raise operation
std::shared_ptr<EffectDefinition> create_exception_effect();
std::shared_ptr<EffectHandler> create_exception_handler();

// Async effect definition and handler
// Provides: wait operation
std::shared_ptr<EffectDefinition> create_async_effect();
std::shared_ptr<EffectHandler> create_async_handler();

// Generator effect definition and handler
// Provides: yield operation
std::shared_ptr<EffectDefinition> create_generator_effect();
std::shared_ptr<EffectHandler> create_generator_handler();

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

// Create all built-in effect handlers
std::vector<std::shared_ptr<EffectHandler>> create_all_builtin_handlers();

// Install all built-in effects in the runtime
void install_builtin_effects();

// Uninstall all built-in effects from the runtime
void uninstall_builtin_effects();

// RAII helper for built-in effects
class BuiltinEffectsScope {
public:
    BuiltinEffectsScope();
    ~BuiltinEffectsScope();
    
    // Non-copyable, non-movable
    BuiltinEffectsScope(const BuiltinEffectsScope&) = delete;
    BuiltinEffectsScope& operator=(const BuiltinEffectsScope&) = delete;
    BuiltinEffectsScope(BuiltinEffectsScope&&) = delete;
    BuiltinEffectsScope& operator=(BuiltinEffectsScope&&) = delete;
};

} // namespace meld::effects