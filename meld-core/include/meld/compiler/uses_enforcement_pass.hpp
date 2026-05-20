#pragma once

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <set>

namespace meld::compiler {

struct UsesViolation {
    std::string function_name;
    std::string effect_name;
    std::string operation_name;
    std::string source_file;
    std::string message;
};

struct UsesEnforcementResult {
    bool ok = true;
    std::vector<UsesViolation> violations;
};

/**
 * Static enforcement of @uses annotations.
 *
 * Rules:
 * - Functions with @uses(...) can only call effects in their allow-list
 * - Functions WITHOUT @uses cannot call any effects (pure by default)
 * - main() is implicitly @uses(*) (all effects allowed)
 * - Functions named with "test" prefix are implicitly @uses(*)
 */
class UsesEnforcementPass {
public:
    UsesEnforcementResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file);

private:
    void check_function(
        const parser::ast::function_definition& func,
        const std::string& source_file,
        UsesEnforcementResult& result);

    void scan_body_for_effects(
        const parser::ast::expression& expr,
        const std::string& func_name,
        const std::set<std::string>& allowed,
        const std::string& source_file,
        UsesEnforcementResult& result);
};

} // namespace meld::compiler
