#pragma once

#include "meld/parser/ast.hpp"
#include "meld/effects/effect_types.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>

namespace meld::compiler {

// Effect checking result
struct EffectCheckResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::set<std::string> required_effects;
    std::set<std::string> declared_effects;
};

// Effect operation mapping - maps operations to required effects
struct EffectOperationMapping {
    std::string operation_name;
    std::string required_effect;
    std::string description;
};

// Effect checker class
class EffectChecker {
public:
    EffectChecker();
    
    // Check effects for a function definition
    EffectCheckResult check_function(const parser::ast::function_definition& func_def);
    
    // TASK 4.4: Collect deprecation warnings for perform_expression nodes - Requirement 4.3
    void collect_perform_deprecation_warnings(const parser::ast::expression& expr,
                                              EffectCheckResult& result);
    
    // Check effects for an expression
    std::set<std::string> analyze_expression_effects(const parser::ast::expression& expr);
    
    // Infer effects for lambda expressions
    std::set<std::string> infer_lambda_effects(const parser::ast::lambda_expression& lambda);
    
    // Propagate effects through function call chains
    std::set<std::string> propagate_call_chain_effects(const parser::ast::function_call& call);
    
    // Infer effects for a function definition (without declared effects)
    std::set<std::string> infer_function_effects(const parser::ast::function_definition& func_def);
    
    // Implicit effect call analysis and validation
    // Analyze implicit effect call — returns the set of effects used
    std::set<std::string> analyze_implicit_effect_call(const parser::ast::implicit_effect_call& call);
    
    // Validate that an effect name + operation exist in known definitions
    EffectCheckResult validate_effect_operation(const std::string& effect_name,
                                               const std::string& operation_name,
                                               const std::vector<parser::ast::expression>& arguments);
    
    // Check for unused @uses declarations
    std::vector<std::string> find_unused_effect_declarations(
        const parser::ast::function_definition& func_def,
        const std::set<std::string>& actually_used_effects);
    
    // TASK 35.8: Automatic effect inference functionality
    // Analyze function bodies to detect performed effects - Requirement 41.6
    std::set<std::string> analyze_function_body_effects(const parser::ast::function_definition& func_def);
    
    // Propagate effects through call graph automatically - Requirement 41.20, 41.21
    void propagate_effects_through_call_graph(const std::vector<parser::ast::function_definition>& functions);
    
    // Generate @uses(...) annotations based on inference - Requirement 41.6
    std::string generate_uses_annotation(const std::set<std::string>& inferred_effects);
    
    // Check if function needs effect annotation update
    bool needs_effect_annotation_update(const parser::ast::function_definition& func_def, 
                                       const std::set<std::string>& inferred_effects);
    
    // Update function with inferred effects (for IDE integration)
    parser::ast::function_definition update_function_with_inferred_effects(
        const parser::ast::function_definition& func_def,
        const std::set<std::string>& inferred_effects);
    
    // Batch inference for multiple functions (handles call graph propagation)
    std::map<std::string, std::set<std::string>> infer_effects_for_functions(
        const std::vector<parser::ast::function_definition>& functions);
    
    // Get inferred effects for a specific function (after batch inference)
    std::set<std::string> get_inferred_effects(const std::string& function_name) const;
    
    // TASK 35.11: Manual annotation constraint functionality
    // Check if function has manually written @uses annotation - Requirement 41.11
    bool has_manual_uses_annotation(const parser::ast::function_definition& func_def) const;
    
    // Validate that implementation matches manual annotation - Requirement 41.11, 41.12
    EffectCheckResult validate_manual_annotation_constraints(const parser::ast::function_definition& func_def);
    
    // Check if manual annotation is compatible with inferred effects - Requirement 41.12
    bool is_manual_annotation_compatible(const std::set<std::string>& manual_effects,
                                        const std::set<std::string>& inferred_effects) const;
    
    // Generate error messages for manual annotation mismatches - Requirement 41.12
    std::vector<std::string> generate_manual_annotation_errors(
        const std::set<std::string>& manual_effects,
        const std::set<std::string>& inferred_effects,
        const std::string& function_name) const;
    
    // Effect polymorphism support
    bool is_effect_polymorphic_function(const parser::ast::function_definition& func_def) const;
    std::set<std::string> resolve_polymorphic_effects(const parser::ast::function_definition& func_def,
                                                      const std::map<std::string, std::string>& effect_bindings) const;
    std::set<std::string> compose_effects(const std::set<std::string>& effects1, 
                                         const std::set<std::string>& effects2) const;
    
    // Enhanced effect polymorphism support
    bool validate_effect_type_parameter_usage(const parser::ast::function_definition& func_def) const;
    std::set<std::string> infer_effect_bindings_from_context(const parser::ast::function_definition& func_def,
                                                            const std::map<std::string, std::string>& type_bindings) const;
    std::set<std::string> resolve_effect_constraints(const std::string& effect_param,
                                                    const std::set<std::string>& required_effects) const;
    
    // Register built-in effect operations
    void register_builtin_operations();
    
    // Register custom effect operation
    void register_operation(const std::string& operation_name, 
                           const std::string& required_effect,
                           const std::string& description = "");
    
    // Check if an operation requires a specific effect
    bool operation_requires_effect(const std::string& operation_name, 
                                  const std::string& effect_name) const;
    
    // Get all effects required by an operation
    std::set<std::string> get_operation_effects(const std::string& operation_name) const;
    
    // Register function effects (for call chain propagation)
    void register_function_effects(const std::string& function_name, 
                                  const std::set<std::string>& effects);
    
    // Get effects for a function (for call chain propagation)
    std::set<std::string> get_function_effects(const std::string& function_name) const;
    
    // Effect polymorphism management
    void register_effect_type_parameter(const std::string& param_name, 
                                       const std::set<std::string>& possible_effects);
    void register_effect_alias(const std::string& alias_name, const std::string& concrete_effect);
    bool is_effect_type_parameter(const std::string& effect_name) const;
    std::set<std::string> get_possible_effects_for_parameter(const std::string& param_name) const;
    
    // Validate effect compatibility
    bool are_effects_compatible(const std::set<std::string>& required,
                               const std::set<std::string>& declared) const;
    
    // Generate effect error messages
    std::vector<std::string> generate_effect_errors(
        const std::set<std::string>& required,
        const std::set<std::string>& declared,
        const std::string& context) const;
    
private:
    // Map operation names to required effects
    std::map<std::string, std::set<std::string>> operation_effects_;
    
    // Map effect names to descriptions
    std::map<std::string, std::string> effect_descriptions_;
    
    // Map function names to their inferred/declared effects (for call chain propagation)
    std::map<std::string, std::set<std::string>> function_effects_;
    
    // TASK 35.8: Additional data structures for automatic effect inference
    // Cache for inferred effects per function (by function name)
    std::map<std::string, std::set<std::string>> inferred_effects_cache_;
    
    // Call graph for effect propagation (function -> functions it calls)
    std::map<std::string, std::set<std::string>> call_graph_;
    
    // Reverse call graph (function -> functions that call it)
    std::map<std::string, std::set<std::string>> reverse_call_graph_;
    
    // Map lambda expressions to their inferred effects (by memory address for uniqueness)
    std::map<const void*, std::set<std::string>> lambda_effects_cache_;
    
    // Effect polymorphism support
    std::map<std::string, std::set<std::string>> effect_type_parameters_; // Maps type param names to possible effects
    std::map<std::string, std::string> effect_aliases_; // Maps effect aliases to concrete effects
    
    // Analyze function call effects
    std::set<std::string> analyze_function_call_effects(const parser::ast::function_call& call);
    
    // Analyze perform expression effects
    std::set<std::string> analyze_perform_effects(const parser::ast::perform_expression& perform);
    
    // Analyze block expression effects
    std::set<std::string> analyze_block_effects(const parser::ast::block_expression& block);
    
    // Check if function name implies effects
    std::set<std::string> infer_effects_from_function_name(const std::string& func_name) const;
    
    // Built-in effect operations
    void register_io_operations();
    void register_network_operations();
    void register_state_operations();
    void register_time_operations();
    void register_console_operations();
};

// Built-in effect operation definitions
// Access these operations via builtin_effects:: scope
namespace builtin_effects {
    
    // IO operations
    const std::vector<EffectOperationMapping> IO_OPERATIONS = {
        {"File.read", "EffectIO", "Read file from disk"},
        {"File.write", "EffectIO", "Write file to disk"},
        {"File.delete", "EffectIO", "Delete file from disk"},
        {"File.exists", "EffectIO", "Check if file exists"},
        {"File.copy", "EffectIO", "Copy file"},
        {"File.move", "EffectIO", "Move file"},
        {"Directory.create", "EffectIO", "Create directory"},
        {"Directory.list", "EffectIO", "List directory contents"},
        {"readFile", "EffectIO", "Read file function"},
        {"writeFile", "EffectIO", "Write file function"},
    };
    
    // Network operations
    const std::vector<EffectOperationMapping> NETWORK_OPERATIONS = {
        {"http.get", "EffectNetwork", "HTTP GET request"},
        {"http.post", "EffectNetwork", "HTTP POST request"},
        {"http.put", "EffectNetwork", "HTTP PUT request"},
        {"http.delete", "EffectNetwork", "HTTP DELETE request"},
        {"fetch", "EffectNetwork", "Fetch data from URL"},
        {"Socket.connect", "EffectNetwork", "Connect to socket"},
        {"Socket.send", "EffectNetwork", "Send data via socket"},
        {"Socket.receive", "EffectNetwork", "Receive data from socket"},
    };
    
    // State operations
    const std::vector<EffectOperationMapping> STATE_OPERATIONS = {
        {"GlobalState.get", "EffectState", "Get global state"},
        {"GlobalState.set", "EffectState", "Set global state"},
        {"Cache.get", "EffectState", "Get from cache"},
        {"Cache.set", "EffectState", "Set cache value"},
        {"Database.query", "EffectState", "Database query"},
        {"Database.update", "EffectState", "Database update"},
    };
    
    // Time operations
    const std::vector<EffectOperationMapping> TIME_OPERATIONS = {
        {"Time.now", "EffectTime", "Get current time"},
        {"Time.sleep", "EffectTime", "Sleep for duration"},
        {"Timer.start", "EffectTime", "Start timer"},
        {"Timer.stop", "EffectTime", "Stop timer"},
        {"Date.now", "EffectTime", "Get current date"},
        {"Random.next", "EffectTime", "Generate random number"},
        {"Random.seed", "EffectTime", "Set random seed"},
    };
    
    // Console operations
    const std::vector<EffectOperationMapping> CONSOLE_OPERATIONS = {
        {"Console.print", "EffectIO", "Print to console"},
        {"Console.println", "EffectIO", "Print line to console"},
        {"Console.readLine", "EffectIO", "Read line from console"},
        {"print", "EffectIO", "Print function"},
        {"println", "EffectIO", "Print line function"},
        {"readLine", "EffectIO", "Read line function"},
    };
}

} // namespace meld::compiler