#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace meld::parser::ast {
    struct expression;
}

namespace meld::effects {

/**
 * Source location for effect violation reporting.
 */
struct EffectSourceLocation {
    std::string file;
    size_t line = 0;
    size_t column = 0;
};

/**
 * An effect violation: a package performs an effect not in its allow list.
 * Requirements: 10.4, 10.5
 */
struct EffectViolation {
    std::string package_name;
    std::string effect_name;
    EffectSourceLocation source_location;
};

/**
 * A node in the effect tree representing one package's effect profile.
 * Requirements: 10.1, 10.2, 10.5
 */
struct EffectNode {
    std::string package_name;
    std::vector<std::string> direct_effects;       // effects this package performs
    std::vector<std::string> transitive_effects;    // effects from its dependencies
    std::vector<std::string> allowed_effects;       // from meld.toml allow array
    std::vector<EffectViolation> violations;         // effects not in allow list
    std::vector<EffectNode*> dependencies;           // child dependency nodes
};

/**
 * The full effect tree for a project, rooted at the project package.
 * Requirements: 10.1, 10.2
 */
struct EffectTree {
    std::string root_package;
    std::vector<EffectNode> nodes;

    /// Find a node by package name, or nullptr if not found.
    const EffectNode* find_node(const std::string& package_name) const {
        for (const auto& node : nodes) {
            if (node.package_name == package_name) {
                return &node;
            }
        }
        return nullptr;
    }

    /// Collect all violations across every node.
    std::vector<EffectViolation> all_violations() const {
        std::vector<EffectViolation> result;
        for (const auto& node : nodes) {
            result.insert(result.end(),
                          node.violations.begin(),
                          node.violations.end());
        }
        return result;
    }
};

/**
 * Represents a parsed module AST with its package metadata.
 * Passed into EffectChecker::build_effect_tree().
 */
struct ModuleInfo {
    std::string package_name;
    std::string file_path;
    std::vector<std::string> allowed_effects;                    // from meld.toml
    std::vector<std::string> dependency_names;                   // transitive deps
    std::shared_ptr<std::vector<parser::ast::expression>> ast;   // parsed AST
};

/**
 * Builds an effect tree by walking module ASTs to discover perform() calls
 * and @uses annotations, then comparing against meld.toml allow arrays.
 *
 * Requirements: 10.1, 10.2, 10.4, 10.5
 */
class EffectChecker {
public:
    EffectChecker() = default;
    ~EffectChecker() = default;

    /**
     * Build the full effect tree for a project and its transitive dependencies.
     *
     * @param modules  All modules (project + dependencies) with their ASTs and
     *                 meld.toml metadata.
     * @return         The effect tree with violations flagged.
     */
    EffectTree build_effect_tree(const std::vector<ModuleInfo>& modules) const;

private:
    /// Walk a single module's AST to extract direct effect names.
    std::vector<std::string> collect_direct_effects(
        const std::vector<parser::ast::expression>& ast,
        const std::string& file_path,
        std::vector<EffectViolation>& violations_out,
        const std::string& package_name,
        const std::vector<std::string>& allowed) const;

    /// Compute transitive effects for a node from its dependency nodes.
    void propagate_transitive_effects(
        EffectNode& node,
        const std::map<std::string, size_t>& index) const;
};

} // namespace meld::effects
