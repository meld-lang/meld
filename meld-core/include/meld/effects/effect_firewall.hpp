#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

#include "effect_checker.hpp"  // reuse EffectSourceLocation, EffectViolation

namespace meld::effects {

/**
 * Permission entry parsed from meld.toml allow arrays.
 * Maps a module (or package) to the set of effects it is allowed to perform.
 */
struct EffectPermission {
    std::string module_name;
    std::set<std::string> allowed_effects;
};

/**
 * Firewall violation: a runtime attempt to perform an effect that is not
 * permitted by either the @uses annotation or the meld.toml allow list.
 *
 * Requirements: 11.1, 11.2, 11.3, 11.4
 */
struct FirewallViolation {
    std::string effect_name;
    std::string module_name;
    EffectSourceLocation call_site;
    std::string reason;  // e.g. "not in @uses annotation" or "not in meld.toml allow list"
};

/**
 * Simplified project config view consumed by the firewall.
 * Populated from meld.toml parsing before the firewall is used.
 */
struct FirewallConfig {
    /// Per-module (or per-package) allow lists from meld.toml [targets] / [dependencies].
    std::map<std::string, std::vector<std::string>> module_allow_lists;

    /// Per-function @uses annotations discovered during parsing.
    /// Key: "module::function_name", Value: declared effect names.
    std::map<std::string, std::vector<std::string>> uses_annotations;
};

/**
 * Effect Firewall — runtime enforcement of effect permissions.
 *
 * A single implementation shared by all three execution tiers:
 *   - Tier 1 (AST Interpreter): called directly from eval_perform_expression
 *   - Tier 2 (ORC JIT): compiler emits calls to the static check entry point
 *   - Tier 3 (AOT): compiler emits calls to the same static check entry point
 *
 * The firewall ensures that:
 *   1. The effect is declared in the calling function's @uses annotation
 *   2. The effect is listed in the module's meld.toml allow array
 *
 * Requirements: 11.1, 11.2, 11.3, 11.4, 11.5
 */
class EffectFirewall {
public:
    EffectFirewall() = default;
    ~EffectFirewall() = default;

    /**
     * Load permissions from a FirewallConfig (parsed from meld.toml and @uses).
     * Must be called before any check() calls.
     */
    void load_permissions(const FirewallConfig& config);

    /**
     * Check whether an effect is permitted in the current context.
     *
     * @param effect_name   The effect being performed (e.g. "fs.read", "net").
     * @param module_name   The module performing the effect.
     * @param call_site     Source location of the perform() call.
     * @return true if the effect is permitted, false if it violates policy.
     *
     * On violation, the details are recorded and retrievable via get_violations().
     * Called identically by all 3 tiers (Req 11.1, 11.5).
     */
    bool check(const std::string& effect_name,
               const std::string& module_name,
               const EffectSourceLocation& call_site);

    /**
     * Get all recorded violations for a specific module.
     * Returns an empty vector if no violations have been recorded.
     */
    std::vector<FirewallViolation> get_violations(const std::string& module_name) const;

    /**
     * Get all recorded violations across all modules.
     */
    std::vector<FirewallViolation> get_all_violations() const;

    /**
     * Clear all recorded violations (useful between runs).
     */
    void clear_violations();

    /**
     * Check if a module has any allow-list configured.
     */
    bool has_permissions_for(const std::string& module_name) const;

    // ─── Static entry point for compiled tiers (Req 11.5) ───────────
    //
    // The compiler emits calls to this C-linkage function at perform() sites.
    // It delegates to the singleton firewall instance so that ORC JIT and AOT
    // use the exact same enforcement logic as the interpreter.

    /**
     * Runtime check entry point callable from compiled code.
     * Returns 0 on success, non-zero on violation.
     */
    static int runtime_check(const char* effect_name,
                             const char* module_name,
                             const char* file,
                             size_t line,
                             size_t column);

    /**
     * Access the process-wide singleton used by compiled tiers.
     */
    static EffectFirewall& instance();

private:
    FirewallConfig config_;
    std::vector<FirewallViolation> violations_;

    /// Check effect against meld.toml allow list for the module.
    bool is_allowed_by_config(const std::string& effect_name,
                              const std::string& module_name) const;

    /// Check effect against @uses annotation for the current function context.
    bool is_allowed_by_uses(const std::string& effect_name,
                            const std::string& module_name) const;
};

} // namespace meld::effects
