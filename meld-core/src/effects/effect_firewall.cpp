#include "meld/effects/effect_firewall.hpp"

#include <algorithm>
#include <format>

namespace meld::effects {

// ─── Singleton ──────────────────────────────────────────────────────

EffectFirewall& EffectFirewall::instance() {
    static EffectFirewall singleton;
    return singleton;
}

// ─── load_permissions ───────────────────────────────────────────────
// Parse meld.toml allow arrays and @uses annotations into the internal
// config structure.  Requirements: 11.1, 11.2, 11.3, 11.4.

void EffectFirewall::load_permissions(const FirewallConfig& config) {
    config_ = config;
    violations_.clear();
}

// ─── check ──────────────────────────────────────────────────────────
// Verify that an effect is permitted by BOTH:
//   1. The module's meld.toml allow list
//   2. The calling function's @uses annotation
//
// On violation: record effect name, offending module, and call site.
// Requirements: 11.1, 11.2, 11.3, 11.4.

bool EffectFirewall::check(const std::string& effect_name,
                           const std::string& module_name,
                           const EffectSourceLocation& call_site) {
    bool permitted = true;

    // Check 1: meld.toml allow list for this module/package.
    if (!is_allowed_by_config(effect_name, module_name)) {
        violations_.push_back(FirewallViolation{
            effect_name,
            module_name,
            call_site,
            std::format("effect '{}' is not in meld.toml allow list for module '{}'",
                        effect_name, module_name)
        });
        permitted = false;
    }

    // Check 2: @uses annotation on the calling function.
    if (!is_allowed_by_uses(effect_name, module_name)) {
        violations_.push_back(FirewallViolation{
            effect_name,
            module_name,
            call_site,
            std::format("effect '{}' is not declared in @uses annotation in module '{}'",
                        effect_name, module_name)
        });
        permitted = false;
    }

    return permitted;
}

// ─── get_violations ─────────────────────────────────────────────────

std::vector<FirewallViolation> EffectFirewall::get_violations(
        const std::string& module_name) const {
    std::vector<FirewallViolation> result;
    for (const auto& v : violations_) {
        if (v.module_name == module_name) {
            result.push_back(v);
        }
    }
    return result;
}

std::vector<FirewallViolation> EffectFirewall::get_all_violations() const {
    return violations_;
}

void EffectFirewall::clear_violations() {
    violations_.clear();
}

bool EffectFirewall::has_permissions_for(const std::string& module_name) const {
    return config_.module_allow_lists.contains(module_name);
}

// ─── Static runtime entry point (Req 11.5) ──────────────────────────
// Called from compiled code (ORC JIT and AOT) at perform() sites.
// Returns 0 on success, non-zero on violation.

int EffectFirewall::runtime_check(const char* effect_name,
                                  const char* module_name,
                                  const char* file,
                                  size_t line,
                                  size_t column) {
    EffectSourceLocation loc{file ? file : "", line, column};
    bool ok = instance().check(effect_name ? effect_name : "",
                               module_name ? module_name : "",
                               loc);
    return ok ? 0 : 1;
}

// ─── Private helpers ────────────────────────────────────────────────

bool EffectFirewall::is_allowed_by_config(const std::string& effect_name,
                                          const std::string& module_name) const {
    auto it = config_.module_allow_lists.find(module_name);
    if (it == config_.module_allow_lists.end()) {
        // No allow list configured for this module — permissive by default.
        // Modules without explicit allow lists are unconstrained at the
        // config level (the @uses check still applies).
        return true;
    }

    const auto& allowed = it->second;
    return std::find(allowed.begin(), allowed.end(), effect_name) != allowed.end();
}

bool EffectFirewall::is_allowed_by_uses(const std::string& effect_name,
                                        const std::string& module_name) const {
    // Search for any function in this module that has a @uses annotation
    // containing the requested effect.  In a full implementation the
    // firewall would receive the specific function context from the call
    // stack; for now we check all @uses entries scoped to this module.
    bool found_any_annotation = false;

    for (const auto& [key, effects] : config_.uses_annotations) {
        // Keys are "module::function_name" — check module prefix.
        if (key.starts_with(module_name + "::") || key == module_name) {
            found_any_annotation = true;
            if (std::find(effects.begin(), effects.end(), effect_name) != effects.end()) {
                return true;
            }
        }
    }

    // If no @uses annotations exist for this module at all, the module
    // hasn't opted into the annotation system — permissive by default.
    if (!found_any_annotation) {
        return true;
    }

    // Annotations exist but none declare this effect — violation.
    return false;
}

} // namespace meld::effects
