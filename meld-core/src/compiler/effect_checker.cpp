#include "meld/compiler/effect_checker.hpp"
#include <algorithm>
#include <sstream>
#include <functional>
#include "meld/compat/visit.hpp"

namespace meld::compiler {

EffectChecker::EffectChecker() {
    register_builtin_operations();
}

// TASK 4.4: Walk an expression tree and emit deprecation warnings for any perform_expression nodes.
// Requirement 4.3: Recommend implicit syntax when explicit perform blocks are encountered.
void EffectChecker::collect_perform_deprecation_warnings(const parser::ast::expression& expr,
                                                         EffectCheckResult& result) {
    meld::compat::visit([&](const auto& concrete_expr) {
        using T = std::decay_t<decltype(concrete_expr)>;

        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::perform_expression>>) {
            const auto& perform = concrete_expr.get();
            result.warnings.push_back(
                "Deprecated: 'perform { " + perform.effect_name.name + "." +
                perform.operation_name.name + "(...) }' syntax. Use '" +
                perform.effect_name.name + "." + perform.operation_name.name +
                "(...)' instead.");
            // Also recurse into arguments
            for (const auto& arg : perform.arguments) {
                collect_perform_deprecation_warnings(arg, result);
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            for (const auto& stmt : concrete_expr.get().statements) {
                collect_perform_deprecation_warnings(stmt, result);
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            for (const auto& arg : concrete_expr.get().arguments) {
                collect_perform_deprecation_warnings(arg, result);
            }
            for (const auto& named_arg : concrete_expr.get().named_arguments) {
                collect_perform_deprecation_warnings(named_arg.value, result);
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
            collect_perform_deprecation_warnings(concrete_expr.get().body, result);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            collect_perform_deprecation_warnings(concrete_expr.get().value, result);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            collect_perform_deprecation_warnings(concrete_expr.get().value, result);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            collect_perform_deprecation_warnings(concrete_expr.get().left, result);
            collect_perform_deprecation_warnings(concrete_expr.get().right, result);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            collect_perform_deprecation_warnings(concrete_expr.get().operand, result);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>>) {
            // Implicit calls are fine — recurse into arguments only
            for (const auto& arg : concrete_expr.get().arguments) {
                collect_perform_deprecation_warnings(arg, result);
            }
        }
        // Literals, identifiers, etc. — nothing to do
    }, expr);
}

EffectCheckResult EffectChecker::check_function(const parser::ast::function_definition& func_def) {
    EffectCheckResult result;
    
    // TASK 35.11: Check manual annotation constraints first - Requirements 41.11, 41.12
    if (has_manual_uses_annotation(func_def)) {
        // Function has manual @uses annotation - validate it as a constraint
        result = validate_manual_annotation_constraints(func_def);
        if (!result.is_valid) {
            return result; // Return early if manual annotation constraint is violated
        }
        
        // Manual annotation is valid, continue with normal checking using declared effects
        std::set<std::string> declared_effects;
        for (const auto& effect : func_def.effects_clause) {
            declared_effects.insert(effect.name);
        }
        result.declared_effects = declared_effects;
        
        // TASK 4.4: Emit deprecation warnings for perform_expression nodes - Requirement 4.3
        // func_def.body is forward_ast<block_expression>, iterate its statements
        for (const auto& stmt : func_def.body.get().statements) {
            collect_perform_deprecation_warnings(stmt.get(), result);
        }
        
        // TASK 4.5: Warn about unused @uses declarations - Requirement 7.4
        // Analyze the body to find actually used effects (both implicit and explicit)
        std::set<std::string> actually_used = analyze_function_body_effects(func_def);
        auto unused = find_unused_effect_declarations(func_def, actually_used);
        for (const auto& unused_effect : unused) {
            result.warnings.push_back("Unused effect declaration: '" + unused_effect +
                "' is declared in @uses but never used in function " + func_def.name.name);
        }
        
        return result;
    }
    
    // No manual annotation - proceed with automatic inference and checking
    
    // Get declared effects from function signature
    std::set<std::string> declared_effects;
    for (const auto& effect : func_def.effects_clause) {
        declared_effects.insert(effect.name);
    }
    
    // If no effects declared, function is considered pure by default
    if (!func_def.has_effects) {
        declared_effects.insert("EffectPure");
    }
    
    // Handle polymorphic effects
    if (is_effect_polymorphic_function(func_def)) {
        // Validate that effect type parameters are properly defined
        if (!validate_effect_type_parameter_usage(func_def)) {
            result.is_valid = false;
            result.errors.push_back("Function " + func_def.name.name + " uses undefined effect type parameters");
            return result;
        }
        
        // For polymorphic functions, we need to check against all possible effect bindings
        // Try to infer concrete effect bindings from the function body
        std::map<std::string, std::string> inferred_bindings;
        auto inferred_effects = infer_effect_bindings_from_context(func_def, {});
        
        // Create bindings for each effect type parameter based on inference
        for (const auto& effect : func_def.effects_clause) {
            if (is_effect_type_parameter(effect.name)) {
                auto constrained_effects = resolve_effect_constraints(effect.name, inferred_effects);
                if (!constrained_effects.empty()) {
                    // Use the first constrained effect as the binding (could be enhanced)
                    inferred_bindings[effect.name] = *constrained_effects.begin();
                }
            }
        }
        
        auto resolved_effects = resolve_polymorphic_effects(func_def, inferred_bindings);
        declared_effects = resolved_effects;
    }
    
    // Analyze function body to determine required effects (with automatic inference)
    std::set<std::string> required_effects = analyze_function_body_effects(func_def);
    
    // Check if required effects are compatible with declared effects
    result.declared_effects = declared_effects;
    result.required_effects = required_effects;
    result.is_valid = are_effects_compatible(required_effects, declared_effects);
    
    if (!result.is_valid) {
        result.errors = generate_effect_errors(required_effects, declared_effects, 
                                              "function " + func_def.name.name);
    }
    
    // TASK 4.4: Emit deprecation warnings for perform_expression nodes - Requirement 4.3
    // func_def.body is forward_ast<block_expression>, iterate its statements
    for (const auto& stmt : func_def.body.get().statements) {
        collect_perform_deprecation_warnings(stmt.get(), result);
    }
    
    // TASK 4.5: Warn about unused @uses declarations - Requirement 7.4
    // Use find_unused_effect_declarations to compare declared vs actually used effects
    auto unused = find_unused_effect_declarations(func_def, required_effects);
    for (const auto& unused_effect : unused) {
        result.warnings.push_back("Unused effect declaration: '" + unused_effect +
            "' is declared in @uses but never used in function " + func_def.name.name);
    }
    
    return result;
}

std::set<std::string> EffectChecker::analyze_expression_effects(const parser::ast::expression& expr) {
    std::set<std::string> effects;
    
    // TASK 4.6 — Mixed implicit and explicit effect calls: Both implicit_effect_call and
    // perform_expression are handled by this visitor and feed into the same effect set.
    // This means a function can freely mix `Effect.op(args)` (implicit) and
    // `perform { Effect.op(args) }` (explicit) for the same or different effects without
    // conflict. This is intentional and required by Requirement 4.2.
    meld::compat::visit([&](const auto& concrete_expr) {
        using T = std::decay_t<decltype(concrete_expr)>;
        
        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            auto call_effects = analyze_function_call_effects(concrete_expr.get());
            effects.insert(call_effects.begin(), call_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
            auto lambda_effects = infer_lambda_effects(concrete_expr.get());
            effects.insert(lambda_effects.begin(), lambda_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::perform_expression>>) {
            auto perform_effects = analyze_perform_effects(concrete_expr.get());
            effects.insert(perform_effects.begin(), perform_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>>) {
            auto implicit_effects = analyze_implicit_effect_call(concrete_expr.get());
            effects.insert(implicit_effects.begin(), implicit_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            auto block_effects = analyze_block_effects(concrete_expr.get());
            effects.insert(block_effects.begin(), block_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            auto val_effects = analyze_expression_effects(concrete_expr.get().value.get());
            effects.insert(val_effects.begin(), val_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            auto var_effects = analyze_expression_effects(concrete_expr.get().value.get());
            effects.insert(var_effects.begin(), var_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            auto left_effects = analyze_expression_effects(concrete_expr.get().left.get());
            auto right_effects = analyze_expression_effects(concrete_expr.get().right.get());
            effects.insert(left_effects.begin(), left_effects.end());
            effects.insert(right_effects.begin(), right_effects.end());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            auto operand_effects = analyze_expression_effects(concrete_expr.get().operand.get());
            effects.insert(operand_effects.begin(), operand_effects.end());
        }
        // Add more expression types as needed
        // For literals and identifiers, no effects are added (they're pure)
    }, expr);
    
    return effects;
}

void EffectChecker::register_builtin_operations() {
    register_io_operations();
    register_network_operations();
    register_state_operations();
    register_time_operations();
    register_console_operations();
}

void EffectChecker::register_operation(const std::string& operation_name, 
                                      const std::string& required_effect,
                                      const std::string& description) {
    operation_effects_[operation_name].insert(required_effect);
    if (!description.empty()) {
        effect_descriptions_[operation_name] = description;
    }
}

bool EffectChecker::operation_requires_effect(const std::string& operation_name, 
                                             const std::string& effect_name) const {
    auto it = operation_effects_.find(operation_name);
    if (it == operation_effects_.end()) {
        return false;
    }
    return it->second.find(effect_name) != it->second.end();
}

std::set<std::string> EffectChecker::get_operation_effects(const std::string& operation_name) const {
    auto it = operation_effects_.find(operation_name);
    if (it == operation_effects_.end()) {
        return {};
    }
    return it->second;
}

void EffectChecker::register_function_effects(const std::string& function_name, 
                                             const std::set<std::string>& effects) {
    function_effects_[function_name] = effects;
}

std::set<std::string> EffectChecker::get_function_effects(const std::string& function_name) const {
    auto it = function_effects_.find(function_name);
    if (it == function_effects_.end()) {
        return {};
    }
    return it->second;
}

bool EffectChecker::are_effects_compatible(const std::set<std::string>& required,
                                          const std::set<std::string>& declared) const {
    // If function is declared as pure, it cannot have any effects
    if (declared.find("EffectPure") != declared.end()) {
        return required.empty() || (required.size() == 1 && required.find("EffectPure") != required.end());
    }
    
    // Check if all required effects are declared
    for (const auto& req_effect : required) {
        if (req_effect == "EffectPure") continue; // Pure is always allowed
        
        if (declared.find(req_effect) == declared.end()) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> EffectChecker::generate_effect_errors(
    const std::set<std::string>& required,
    const std::set<std::string>& declared,
    const std::string& context) const {
    
    std::vector<std::string> errors;
    
    // Find missing effects
    std::set<std::string> missing_effects;
    for (const auto& req_effect : required) {
        if (req_effect == "EffectPure") continue;
        
        if (declared.find(req_effect) == declared.end()) {
            missing_effects.insert(req_effect);
        }
    }
    
    if (!missing_effects.empty()) {
        std::ostringstream oss;
        oss << "Function " << context << " performs effects that are not declared: ";
        bool first = true;
        for (const auto& effect : missing_effects) {
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        oss << ". Add 'effects { ";
        first = true;
        for (const auto& effect : missing_effects) {
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        oss << " }' to the function signature.";
        errors.push_back(oss.str());
    }
    
    // Check for pure function violations
    if (declared.find("EffectPure") != declared.end() && !required.empty()) {
        std::ostringstream oss;
        oss << "Function " << context << " is declared as pure but performs effects: ";
        bool first = true;
        for (const auto& effect : required) {
            if (effect == "EffectPure") continue;
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        errors.push_back(oss.str());
    }
    
    return errors;
}

std::set<std::string> EffectChecker::infer_lambda_effects(const parser::ast::lambda_expression& lambda) {
    // Check if we've already computed effects for this lambda
    const void* lambda_ptr = &lambda;
    auto cache_it = lambda_effects_cache_.find(lambda_ptr);
    if (cache_it != lambda_effects_cache_.end()) {
        return cache_it->second;
    }
    
    // Analyze the lambda body to infer effects
    std::set<std::string> inferred_effects = analyze_expression_effects(lambda.body);
    
    // Cache the result for future queries
    lambda_effects_cache_[lambda_ptr] = inferred_effects;
    
    return inferred_effects;
}

std::set<std::string> EffectChecker::propagate_call_chain_effects(const parser::ast::function_call& call) {
    std::set<std::string> effects;
    
    // Get the function name
    std::string func_name = call.function_name.name;
    
    // Check if we have registered effects for this function
    auto func_effects = get_function_effects(func_name);
    effects.insert(func_effects.begin(), func_effects.end());
    
    // Also check built-in operations
    auto operation_effects = get_operation_effects(func_name);
    effects.insert(operation_effects.begin(), operation_effects.end());
    
    // Infer from function name patterns
    auto inferred_effects = infer_effects_from_function_name(func_name);
    effects.insert(inferred_effects.begin(), inferred_effects.end());
    
    // Analyze arguments for nested effects (including lambda arguments)
    for (const auto& arg : call.arguments) {
        auto arg_effects = analyze_expression_effects(arg);
        effects.insert(arg_effects.begin(), arg_effects.end());
    }
    
    for (const auto& named_arg : call.named_arguments) {
        auto arg_effects = analyze_expression_effects(named_arg.value);
        effects.insert(arg_effects.begin(), arg_effects.end());
    }
    
    return effects;
}

std::set<std::string> EffectChecker::infer_function_effects(const parser::ast::function_definition& func_def) {
    // Use the new analyze_function_body_effects method for more comprehensive analysis
    return analyze_function_body_effects(func_def);
}

// Implicit effect call analysis — returns the set of effects used
// Requirements: 2.1, 2.2, 2.3, 7.1
std::set<std::string> EffectChecker::analyze_implicit_effect_call(const parser::ast::implicit_effect_call& call) {
    std::set<std::string> effects;

    // The effect name from the call is the effect being used
    std::string effect_name = call.effect_name.name;
    effects.insert(effect_name);

    // Also analyze arguments for any nested effects
    for (const auto& arg : call.arguments) {
        auto arg_effects = analyze_expression_effects(arg);
        effects.insert(arg_effects.begin(), arg_effects.end());
    }

    return effects;
}

// Validate that an effect name + operation exist in known definitions
// Requirements: 2.1, 2.2, 2.3, 7.1, 7.2, 7.3
EffectCheckResult EffectChecker::validate_effect_operation(
    const std::string& effect_name,
    const std::string& operation_name,
    const std::vector<parser::ast::expression>& arguments) {

    EffectCheckResult result;
    result.is_valid = true;

    // Build the qualified operation key used in operation_effects_ (e.g., "Console.print")
    std::string qualified_op = effect_name + "." + operation_name;

    // Collect all known effect names from operation_effects_ keys
    std::set<std::string> known_effect_names;
    for (const auto& entry : operation_effects_) {
        // Keys are "EffectPrefix.operation" — extract the prefix
        auto dot_pos = entry.first.find('.');
        if (dot_pos != std::string::npos) {
            known_effect_names.insert(entry.first.substr(0, dot_pos));
        }
    }

    // Also consider effects registered via register_function_effects / function_effects_
    // and effect type parameters as known effect names
    for (const auto& entry : function_effects_) {
        for (const auto& eff : entry.second) {
            if (eff != "EffectPure") {
                known_effect_names.insert(eff);
            }
        }
    }

    // Requirement 7.1: Validate that the effect name matches a known effect definition
    // Check if any operation is registered under this effect name prefix
    bool effect_found = known_effect_names.find(effect_name) != known_effect_names.end();

    if (!effect_found) {
        result.is_valid = false;
        std::ostringstream oss;
        oss << "Unknown effect '" << effect_name << "'. ";
        if (!known_effect_names.empty()) {
            oss << "Available effect definitions: ";
            bool first = true;
            for (const auto& name : known_effect_names) {
                if (!first) oss << ", ";
                oss << name;
                first = false;
            }
        } else {
            oss << "No effect definitions are currently registered";
        }
        result.errors.push_back(oss.str());
        return result;
    }

    // Requirement 7.1 (operation existence): Validate that the operation exists on the matched effect
    auto op_it = operation_effects_.find(qualified_op);
    if (op_it == operation_effects_.end()) {
        result.is_valid = false;
        // Collect known operations for this effect to provide a helpful error
        std::vector<std::string> available_ops;
        for (const auto& entry : operation_effects_) {
            if (entry.first.find(effect_name + ".") == 0) {
                available_ops.push_back(entry.first.substr(effect_name.size() + 1));
            }
        }

        std::ostringstream oss;
        oss << "Unknown operation '" << operation_name << "' on effect '" << effect_name << "'. ";
        if (!available_ops.empty()) {
            oss << "Available operations: ";
            bool first = true;
            for (const auto& op : available_ops) {
                if (!first) oss << ", ";
                oss << op;
                first = false;
            }
        }
        result.errors.push_back(oss.str());
        return result;
    }

    // Record the required effect from the operation mapping
    result.required_effects = op_it->second;

    // Requirement 7.2: Basic argument count / type validation
    // We do a best-effort check here — full type checking would require a type system pass.
    // For now, we validate that the operation's registered effect is consistent.
    // (Detailed per-parameter type checking is deferred to the type checker phase,
    //  but we flag obvious mismatches when parameter metadata is available.)

    return result;
}

// Check for unused @uses declarations
// Requirements: 7.4
std::vector<std::string> EffectChecker::find_unused_effect_declarations(
    const parser::ast::function_definition& func_def,
    const std::set<std::string>& actually_used_effects) {

    std::vector<std::string> unused;

    // Get declared effects from the function's @uses / effects clause
    for (const auto& effect : func_def.effects_clause) {
        const std::string& declared = effect.name;

        // Skip EffectPure — it's a marker, not a real usage
        if (declared == "EffectPure") {
            continue;
        }

        // Skip effect type parameters — they are abstract and cannot be "unused"
        if (is_effect_type_parameter(declared)) {
            continue;
        }

        // If the declared effect is not in the actually-used set, it's unused
        if (actually_used_effects.find(declared) == actually_used_effects.end()) {
            unused.push_back(declared);
        }
    }

    return unused;
}

// TASK 35.8: Implement automatic effect inference functionality
// Requirement 41.6: Analyze function bodies to detect performed effects
// TASK 6.1 (implicit-effect-calls): implicit_effect_call nodes are detected here
// via analyze_expression_effects, which delegates to analyze_implicit_effect_call
// (added in task 4.3). No additional handling is needed — Requirements 5.1, 5.2.
std::set<std::string> EffectChecker::analyze_function_body_effects(const parser::ast::function_definition& func_def) {
    // Check cache first to avoid recomputation
    auto cache_it = inferred_effects_cache_.find(func_def.name.name);
    if (cache_it != inferred_effects_cache_.end()) {
        return cache_it->second;
    }
    
    // Analyze function body to determine what effects it actually performs
    // func_def.body is forward_ast<block_expression>, iterate its statements
    std::set<std::string> inferred_effects;
    for (const auto& stmt : func_def.body.get().statements) {
        auto stmt_effects = analyze_expression_effects(stmt.get());
        inferred_effects.insert(stmt_effects.begin(), stmt_effects.end());
    }
    
    // Analyze inline micro-tests for additional effects
    if (func_def.has_tests) {
        for (const auto& test : func_def.tests) {
            // test.body is forward_ast<block_expression>, iterate its statements
            for (const auto& stmt : test.body.get().statements) {
                auto test_effects = analyze_expression_effects(stmt.get());
                inferred_effects.insert(test_effects.begin(), test_effects.end());
            }
        }
    }
    
    // Analyze contract blocks for additional effects
    if (func_def.has_contracts) {
        // Analyze require blocks
        for (const auto& require_block : func_def.contracts.preconditions) {
            auto require_effects = analyze_expression_effects(require_block.condition.get());
            inferred_effects.insert(require_effects.begin(), require_effects.end());
        }
        
        // Analyze ensure blocks
        for (const auto& ensure_block : func_def.contracts.postconditions) {
            auto ensure_effects = analyze_expression_effects(ensure_block.condition.get());
            inferred_effects.insert(ensure_effects.begin(), ensure_effects.end());
        }
    }
    
    // If no effects were found, the function is pure
    if (inferred_effects.empty()) {
        inferred_effects.insert("EffectPure");
    }
    
    // Cache the result for future queries
    inferred_effects_cache_[func_def.name.name] = inferred_effects;
    
    // Register the inferred effects for this function (for call chain propagation)
    register_function_effects(func_def.name.name, inferred_effects);
    
    return inferred_effects;
}

// TASK 35.8: Propagate effects through call graph automatically
// Requirements 41.20, 41.21: Propagate effects through call graph automatically
// TASK 6.2 (implicit-effect-calls): The extract_calls lambda below recurses into
// implicit_effect_call and perform_expression arguments, ensuring any function calls
// nested inside effect operation arguments are captured in the call graph.
// Requirements 2.4, 5.3.
void EffectChecker::propagate_effects_through_call_graph(const std::vector<parser::ast::function_definition>& functions) {
    // Step 1: Build call graph by analyzing function calls in each function
    call_graph_.clear();
    reverse_call_graph_.clear();
    
    for (const auto& func : functions) {
        std::set<std::string> called_functions;
        
        // Find all function calls in the function body
        auto extract_calls = [&called_functions](const parser::ast::expression& expr, auto& self) -> void {
            meld::compat::visit([&](const auto& concrete_expr) {
                using T = std::decay_t<decltype(concrete_expr)>;
                
                if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
                    called_functions.insert(concrete_expr.get().function_name.name);
                    
                    // Recursively analyze arguments (forward_ast<expression> → .get())
                    for (const auto& arg : concrete_expr.get().arguments) {
                        self(arg.get(), self);
                    }
                    for (const auto& named_arg : concrete_expr.get().named_arguments) {
                        self(named_arg.value.get(), self);
                    }
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
                    for (const auto& stmt : concrete_expr.get().statements) {
                        self(stmt.get(), self);
                    }
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
                    self(concrete_expr.get().body.get(), self);
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
                    self(concrete_expr.get().left.get(), self);
                    self(concrete_expr.get().right.get(), self);
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
                    self(concrete_expr.get().operand.get(), self);
                }
                // TASK 6.2 (implicit-effect-calls): Recurse into implicit_effect_call and
                // perform_expression arguments so any nested function calls within effect
                // operation arguments are added to the call graph. Requirements 2.4, 5.3.
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>>) {
                    for (const auto& arg : concrete_expr.get().arguments) {
                        self(arg.get(), self);
                    }
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::perform_expression>>) {
                    for (const auto& arg : concrete_expr.get().arguments) {
                        self(arg.get(), self);
                    }
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
                    self(concrete_expr.get().value.get(), self);
                }
                else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
                    self(concrete_expr.get().value.get(), self);
                }
                // Add more expression types as needed
            }, expr);
        };
        
        // func.body is forward_ast<block_expression>, iterate its statements
        for (const auto& stmt : func.body.get().statements) {
            extract_calls(stmt.get(), extract_calls);
        }
        
        // Store call relationships
        call_graph_[func.name.name] = called_functions;
        
        // Build reverse call graph
        for (const auto& called_func : called_functions) {
            reverse_call_graph_[called_func].insert(func.name.name);
        }
    }
    
    // Step 2: Perform topological sort to determine propagation order
    std::vector<std::string> propagation_order;
    std::set<std::string> visited;
    std::set<std::string> in_progress;
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& func_name) -> bool {
        if (in_progress.find(func_name) != in_progress.end()) {
            // Cycle detected - handle gracefully by continuing
            return true;
        }
        if (visited.find(func_name) != visited.end()) {
            return true;
        }
        
        in_progress.insert(func_name);
        
        // Visit all functions this function calls first
        auto call_it = call_graph_.find(func_name);
        if (call_it != call_graph_.end()) {
            for (const auto& called_func : call_it->second) {
                if (!dfs(called_func)) {
                    return false;
                }
            }
        }
        
        in_progress.erase(func_name);
        visited.insert(func_name);
        propagation_order.push_back(func_name);
        
        return true;
    };
    
    // Start DFS from all functions
    for (const auto& func : functions) {
        if (visited.find(func.name.name) == visited.end()) {
            dfs(func.name.name);
        }
    }
    
    // Step 3: Propagate effects in topological order
    for (const auto& func_name : propagation_order) {
        std::set<std::string> total_effects;
        
        // Get direct effects from function body
        auto direct_effects_it = inferred_effects_cache_.find(func_name);
        if (direct_effects_it != inferred_effects_cache_.end()) {
            total_effects.insert(direct_effects_it->second.begin(), direct_effects_it->second.end());
        }
        
        // Add effects from called functions
        auto call_it = call_graph_.find(func_name);
        if (call_it != call_graph_.end()) {
            for (const auto& called_func : call_it->second) {
                auto called_effects = get_function_effects(called_func);
                total_effects.insert(called_effects.begin(), called_effects.end());
            }
        }
        
        // Remove EffectPure if other effects are present
        if (total_effects.size() > 1 && total_effects.find("EffectPure") != total_effects.end()) {
            total_effects.erase("EffectPure");
        }
        
        // Update function effects with propagated effects
        register_function_effects(func_name, total_effects);
        inferred_effects_cache_[func_name] = total_effects;
    }
}

// TASK 35.8: Generate @uses(...) annotations based on inference
// Requirement 41.6: Generate @uses(...) annotations based on inference
// TASK 6.3 (implicit-effect-calls): This method operates on the inferred effect set,
// which already includes effects from implicit_effect_call nodes (via
// analyze_function_body_effects → analyze_expression_effects). No additional handling
// is needed — Requirement 5.2.
std::string EffectChecker::generate_uses_annotation(const std::set<std::string>& inferred_effects) {
    if (inferred_effects.empty() || 
        (inferred_effects.size() == 1 && inferred_effects.find("EffectPure") != inferred_effects.end())) {
        return "@uses()"; // Pure function
    }
    
    std::ostringstream oss;
    oss << "@uses(";
    
    bool first = true;
    for (const auto& effect : inferred_effects) {
        if (effect == "EffectPure") continue; // Skip pure effect in mixed sets
        
        if (!first) {
            oss << ", ";
        }
        oss << effect;
        first = false;
    }
    
    oss << ")";
    return oss.str();
}

// TASK 35.8: Check if function needs effect annotation update
bool EffectChecker::needs_effect_annotation_update(const parser::ast::function_definition& func_def, 
                                                   const std::set<std::string>& inferred_effects) {
    // Get currently declared effects
    std::set<std::string> declared_effects;
    for (const auto& effect : func_def.effects_clause) {
        declared_effects.insert(effect.name);
    }
    
    // If no effects declared, function is considered pure by default
    if (!func_def.has_effects) {
        declared_effects.insert("EffectPure");
    }
    
    // Compare inferred effects with declared effects
    return inferred_effects != declared_effects;
}

// TASK 35.8: Update function with inferred effects (for IDE integration)
parser::ast::function_definition EffectChecker::update_function_with_inferred_effects(
    const parser::ast::function_definition& func_def,
    const std::set<std::string>& inferred_effects) {
    
    parser::ast::function_definition updated_func = func_def;
    
    // Clear existing effects clause
    updated_func.effects_clause.clear();
    
    // Add inferred effects (skip EffectPure if other effects are present)
    bool has_non_pure_effects = false;
    for (const auto& effect : inferred_effects) {
        if (effect != "EffectPure") {
            parser::ast::identifier effect_id;
            effect_id.name = effect;
            updated_func.effects_clause.push_back(effect_id);
            has_non_pure_effects = true;
        }
    }
    
    // Set has_effects flag based on whether we have non-pure effects
    updated_func.has_effects = has_non_pure_effects;
    
    return updated_func;
}

// TASK 35.8: Batch inference for multiple functions (handles call graph propagation)
std::map<std::string, std::set<std::string>> EffectChecker::infer_effects_for_functions(
    const std::vector<parser::ast::function_definition>& functions) {
    
    // Step 1: Analyze each function individually to get direct effects
    for (const auto& func : functions) {
        analyze_function_body_effects(func);
    }
    
    // Step 2: Propagate effects through call graph
    propagate_effects_through_call_graph(functions);
    
    // Step 3: Return the final inferred effects for all functions
    std::map<std::string, std::set<std::string>> result;
    for (const auto& func : functions) {
        result[func.name.name] = get_inferred_effects(func.name.name);
    }
    
    return result;
}

// TASK 35.8: Get inferred effects for a specific function (after batch inference)
std::set<std::string> EffectChecker::get_inferred_effects(const std::string& function_name) const {
    auto cache_it = inferred_effects_cache_.find(function_name);
    if (cache_it != inferred_effects_cache_.end()) {
        return cache_it->second;
    }
    
    // Fall back to registered function effects
    return get_function_effects(function_name);
}

// TASK 35.11: Manual annotation constraint functionality
// Check if function has manually written @uses annotation - Requirement 41.11
bool EffectChecker::has_manual_uses_annotation(const parser::ast::function_definition& func_def) const {
    // A function has a manual @uses annotation if it has an effects clause
    // In the AST, this is represented by has_effects being true and effects_clause being populated
    // This indicates the developer explicitly wrote an effects clause
    return func_def.has_effects && !func_def.effects_clause.empty();
}

// Validate that implementation matches manual annotation - Requirement 41.11, 41.12
EffectCheckResult EffectChecker::validate_manual_annotation_constraints(const parser::ast::function_definition& func_def) {
    EffectCheckResult result;
    result.is_valid = true;
    
    // Only validate if function has manual annotation
    if (!has_manual_uses_annotation(func_def)) {
        return result; // No manual annotation, nothing to validate
    }
    
    // Get manually declared effects
    std::set<std::string> manual_effects;
    for (const auto& effect : func_def.effects_clause) {
        manual_effects.insert(effect.name);
    }
    
    // Get inferred effects from implementation
    std::set<std::string> inferred_effects = analyze_function_body_effects(func_def);
    
    // Store effects in result for reference
    result.declared_effects = manual_effects;
    result.required_effects = inferred_effects;
    
    // Check if manual annotation is compatible with inferred effects
    if (!is_manual_annotation_compatible(manual_effects, inferred_effects)) {
        result.is_valid = false;
        result.errors = generate_manual_annotation_errors(manual_effects, inferred_effects, func_def.name.name);
    }
    
    return result;
}

// Check if manual annotation is compatible with inferred effects - Requirement 41.12
bool EffectChecker::is_manual_annotation_compatible(const std::set<std::string>& manual_effects,
                                                   const std::set<std::string>& inferred_effects) const {
    // Manual annotation acts as a constraint - the implementation cannot perform effects
    // that are not declared in the manual annotation
    
    // Special case: if manual annotation declares pure (@uses()), implementation must be pure
    bool manual_is_pure = manual_effects.empty() || 
                         (manual_effects.size() == 1 && manual_effects.find("EffectPure") != manual_effects.end());
    bool inferred_is_pure = inferred_effects.empty() || 
                           (inferred_effects.size() == 1 && inferred_effects.find("EffectPure") != inferred_effects.end());
    
    if (manual_is_pure) {
        return inferred_is_pure; // Pure annotation requires pure implementation
    }
    
    // For non-pure manual annotations, check that all inferred effects are declared
    for (const auto& inferred_effect : inferred_effects) {
        if (inferred_effect == "EffectPure") continue; // Skip pure effect in mixed sets
        
        if (manual_effects.find(inferred_effect) == manual_effects.end()) {
            return false; // Implementation performs undeclared effect
        }
    }
    
    return true; // All inferred effects are covered by manual annotation
}

// Generate error messages for manual annotation mismatches - Requirement 41.12
std::vector<std::string> EffectChecker::generate_manual_annotation_errors(
    const std::set<std::string>& manual_effects,
    const std::set<std::string>& inferred_effects,
    const std::string& function_name) const {
    
    std::vector<std::string> errors;
    
    // Check if manual annotation declares pure but implementation is not
    bool manual_is_pure = manual_effects.empty() || 
                         (manual_effects.size() == 1 && manual_effects.find("EffectPure") != manual_effects.end());
    bool inferred_is_pure = inferred_effects.empty() || 
                           (inferred_effects.size() == 1 && inferred_effects.find("EffectPure") != inferred_effects.end());
    
    if (manual_is_pure && !inferred_is_pure) {
        std::ostringstream oss;
        oss << "Function '" << function_name << "' is manually annotated as pure (@uses()) but implementation performs effects: ";
        bool first = true;
        for (const auto& effect : inferred_effects) {
            if (effect == "EffectPure") continue;
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        oss << ". Remove the effects from the implementation or update the @uses annotation.";
        errors.push_back(oss.str());
        return errors;
    }
    
    // Find effects performed by implementation but not declared in manual annotation
    std::set<std::string> undeclared_effects;
    for (const auto& inferred_effect : inferred_effects) {
        if (inferred_effect == "EffectPure") continue; // Skip pure effect
        
        if (manual_effects.find(inferred_effect) == manual_effects.end()) {
            undeclared_effects.insert(inferred_effect);
        }
    }
    
    if (!undeclared_effects.empty()) {
        std::ostringstream oss;
        oss << "Function '" << function_name << "' performs effects not declared in manual @uses annotation: ";
        bool first = true;
        for (const auto& effect : undeclared_effects) {
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        oss << ". Either remove these effects from the implementation or add them to the @uses annotation: @uses(";
        
        // Suggest updated annotation
        std::set<std::string> suggested_effects = manual_effects;
        suggested_effects.insert(undeclared_effects.begin(), undeclared_effects.end());
        
        first = true;
        for (const auto& effect : suggested_effects) {
            if (effect == "EffectPure") continue;
            if (!first) oss << ", ";
            oss << effect;
            first = false;
        }
        oss << ")";
        
        errors.push_back(oss.str());
    }
    
    return errors;
}

bool EffectChecker::is_effect_polymorphic_function(const parser::ast::function_definition& func_def) const {
    // Check if function has effect type parameters in its signature
    for (const auto& effect : func_def.effects_clause) {
        if (is_effect_type_parameter(effect.name)) {
            return true;
        }
    }
    return false;
}

std::set<std::string> EffectChecker::resolve_polymorphic_effects(
    const parser::ast::function_definition& func_def,
    const std::map<std::string, std::string>& effect_bindings) const {
    
    std::set<std::string> resolved_effects;
    
    for (const auto& effect : func_def.effects_clause) {
        if (is_effect_type_parameter(effect.name)) {
            // Resolve the effect type parameter to concrete effect
            auto binding_it = effect_bindings.find(effect.name);
            if (binding_it != effect_bindings.end()) {
                resolved_effects.insert(binding_it->second);
            } else {
                // If no binding provided, try to infer from context
                auto inferred_effects = infer_effect_bindings_from_context(func_def, {});
                auto constrained_effects = resolve_effect_constraints(effect.name, inferred_effects);
                
                if (!constrained_effects.empty()) {
                    resolved_effects.insert(constrained_effects.begin(), constrained_effects.end());
                } else {
                    // Fall back to all possible effects for this parameter
                    auto possible_effects = get_possible_effects_for_parameter(effect.name);
                    resolved_effects.insert(possible_effects.begin(), possible_effects.end());
                }
            }
        } else {
            // Concrete effect, add as-is
            resolved_effects.insert(effect.name);
        }
    }
    
    return resolved_effects;
}

std::set<std::string> EffectChecker::compose_effects(const std::set<std::string>& effects1, 
                                                    const std::set<std::string>& effects2) const {
    std::set<std::string> composed_effects;
    
    // Union of both effect sets
    composed_effects.insert(effects1.begin(), effects1.end());
    composed_effects.insert(effects2.begin(), effects2.end());
    
    // Remove EffectPure if other effects are present
    if (composed_effects.size() > 1 && composed_effects.find("EffectPure") != composed_effects.end()) {
        composed_effects.erase("EffectPure");
    }
    
    // Handle effect type parameters in composition
    std::set<std::string> resolved_composed_effects;
    for (const auto& effect : composed_effects) {
        if (is_effect_type_parameter(effect)) {
            // For effect type parameters, include all possible effects
            auto possible_effects = get_possible_effects_for_parameter(effect);
            resolved_composed_effects.insert(possible_effects.begin(), possible_effects.end());
        } else {
            resolved_composed_effects.insert(effect);
        }
    }
    
    // Remove EffectPure again if other effects were added during resolution
    if (resolved_composed_effects.size() > 1 && 
        resolved_composed_effects.find("EffectPure") != resolved_composed_effects.end()) {
        resolved_composed_effects.erase("EffectPure");
    }
    
    return resolved_composed_effects;
}

void EffectChecker::register_effect_type_parameter(const std::string& param_name, 
                                                  const std::set<std::string>& possible_effects) {
    effect_type_parameters_[param_name] = possible_effects;
}

void EffectChecker::register_effect_alias(const std::string& alias_name, const std::string& concrete_effect) {
    effect_aliases_[alias_name] = concrete_effect;
}

bool EffectChecker::is_effect_type_parameter(const std::string& effect_name) const {
    return effect_type_parameters_.find(effect_name) != effect_type_parameters_.end();
}

std::set<std::string> EffectChecker::get_possible_effects_for_parameter(const std::string& param_name) const {
    auto it = effect_type_parameters_.find(param_name);
    if (it == effect_type_parameters_.end()) {
        return {};
    }
    return it->second;
}

bool EffectChecker::validate_effect_type_parameter_usage(const parser::ast::function_definition& func_def) const {
    // Check that all effect type parameters used in the function are properly declared
    for (const auto& effect : func_def.effects_clause) {
        if (is_effect_type_parameter(effect.name)) {
            // Validate that the parameter has possible effects defined
            auto possible_effects = get_possible_effects_for_parameter(effect.name);
            if (possible_effects.empty()) {
                return false; // Invalid: no possible effects defined for this parameter
            }
        }
    }
    return true;
}

std::set<std::string> EffectChecker::infer_effect_bindings_from_context(
    const parser::ast::function_definition& func_def,
    const std::map<std::string, std::string>& type_bindings) const {
    
    std::set<std::string> inferred_effects;
    
    // Analyze function body to infer what concrete effects are needed
    // func_def.body is forward_ast<block_expression>; iterate statements
    // Use const_cast since analyze_expression_effects is non-const but safe here
    std::set<std::string> body_effects;
    for (const auto& stmt : func_def.body.get().statements) {
        auto stmt_effects = const_cast<EffectChecker*>(this)->analyze_expression_effects(stmt.get());
        body_effects.insert(stmt_effects.begin(), stmt_effects.end());
    }
    
    // For each effect type parameter, try to infer concrete bindings
    for (const auto& effect : func_def.effects_clause) {
        if (is_effect_type_parameter(effect.name)) {
            auto possible_effects = get_possible_effects_for_parameter(effect.name);
            
            // Find intersection of possible effects and required effects
            for (const auto& possible : possible_effects) {
                if (body_effects.find(possible) != body_effects.end()) {
                    inferred_effects.insert(possible);
                }
            }
        } else {
            // Concrete effect, add as-is
            inferred_effects.insert(effect.name);
        }
    }
    
    return inferred_effects;
}

std::set<std::string> EffectChecker::resolve_effect_constraints(
    const std::string& effect_param,
    const std::set<std::string>& required_effects) const {
    
    std::set<std::string> resolved_effects;
    
    if (is_effect_type_parameter(effect_param)) {
        auto possible_effects = get_possible_effects_for_parameter(effect_param);
        
        // Find intersection of possible effects and required effects
        for (const auto& required : required_effects) {
            if (possible_effects.find(required) != possible_effects.end()) {
                resolved_effects.insert(required);
            }
        }
    } else {
        // Concrete effect parameter
        if (required_effects.find(effect_param) != required_effects.end()) {
            resolved_effects.insert(effect_param);
        }
    }
    
    return resolved_effects;
}

std::set<std::string> EffectChecker::analyze_function_call_effects(const parser::ast::function_call& call) {
    // Use the new call chain propagation method
    return propagate_call_chain_effects(call);
}

std::set<std::string> EffectChecker::analyze_perform_effects(const parser::ast::perform_expression& perform) {
    std::set<std::string> effects;
    
    // Perform expressions always require the effect they're performing
    std::string effect_name = perform.effect_name.name;
    effects.insert(effect_name);
    
    // Analyze arguments for nested effects
    for (const auto& arg : perform.arguments) {
        auto arg_effects = analyze_expression_effects(arg);
        effects.insert(arg_effects.begin(), arg_effects.end());
    }
    
    return effects;
}

std::set<std::string> EffectChecker::analyze_block_effects(const parser::ast::block_expression& block) {
    std::set<std::string> effects;
    
    // Analyze all statements in the block
    for (const auto& stmt : block.statements) {
        auto stmt_effects = analyze_expression_effects(stmt);
        effects.insert(stmt_effects.begin(), stmt_effects.end());
    }
    
    return effects;
}

std::set<std::string> EffectChecker::infer_effects_from_function_name(const std::string& func_name) const {
    std::set<std::string> effects;
    
    // Pattern-based inference for common function names
    if (func_name.find("read") != std::string::npos || 
        func_name.find("write") != std::string::npos ||
        func_name.find("file") != std::string::npos ||
        func_name.find("File") != std::string::npos) {
        effects.insert("EffectIO");
    }
    
    if (func_name.find("http") != std::string::npos ||
        func_name.find("fetch") != std::string::npos ||
        func_name.find("request") != std::string::npos ||
        func_name.find("download") != std::string::npos ||
        func_name.find("upload") != std::string::npos) {
        effects.insert("EffectNetwork");
    }
    
    if (func_name.find("print") != std::string::npos ||
        func_name.find("log") != std::string::npos ||
        func_name.find("console") != std::string::npos) {
        effects.insert("EffectIO");
    }
    
    if (func_name.find("time") != std::string::npos ||
        func_name.find("sleep") != std::string::npos ||
        func_name.find("delay") != std::string::npos ||
        func_name.find("random") != std::string::npos) {
        effects.insert("EffectTime");
    }
    
    if (func_name.find("state") != std::string::npos ||
        func_name.find("cache") != std::string::npos ||
        func_name.find("database") != std::string::npos ||
        func_name.find("db") != std::string::npos) {
        effects.insert("EffectState");
    }
    
    return effects;
}

void EffectChecker::register_io_operations() {
    // Register IO operations: File.read, File.write, File.delete, etc.
    for (const auto& op : builtin_effects::IO_OPERATIONS) {
        register_operation(op.operation_name, op.required_effect, op.description);
    }
}

void EffectChecker::register_network_operations() {
    // Register network operations: http.get, http.post, fetch, etc.
    for (const auto& op : builtin_effects::NETWORK_OPERATIONS) {
        register_operation(op.operation_name, op.required_effect, op.description);
    }
}

void EffectChecker::register_state_operations() {
    for (const auto& op : builtin_effects::STATE_OPERATIONS) {
        register_operation(op.operation_name, op.required_effect, op.description);
    }
}

void EffectChecker::register_time_operations() {
    // Register time operations: Time.now, Time.sleep, Timer.start, etc.
    for (const auto& op : builtin_effects::TIME_OPERATIONS) {
        register_operation(op.operation_name, op.required_effect, op.description);
    }
}

void EffectChecker::register_console_operations() {
    for (const auto& op : builtin_effects::CONSOLE_OPERATIONS) {
        register_operation(op.operation_name, op.required_effect, op.description);
    }
}

} // namespace meld::compiler