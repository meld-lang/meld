#include "meld/macro/imposes_annotation.hpp"
#include "meld/parser/ast.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <sstream>
#include <algorithm>
#include <variant>
#include "meld/compat/visit.hpp"

namespace meld::macro {

namespace x3 = boost::spirit::x3;

void ImposesAnnotation::register_annotation() {
    // Note: @imposes is a function-level annotation, not a class-level decorator
    // We need a different registration mechanism for function annotations
    // For now, we'll register it as a special function transformer
    
    // This would be called during compiler initialization to register
    // the @imposes annotation handler with the macro system
    
    // TODO: Implement function-level annotation registry
    // FunctionAnnotationRegistry::instance().register_annotation("imposes", 
    //     [](const parser::ast::function_definition& func_def, 
    //        const std::vector<std::string>& params,
    //        MacroExpander& expander) -> std::expected<kernel::Value, std::string> {
    //         return transform_imposes_function(func_def, params, expander);
    //     });
}

std::expected<kernel::Value, std::string> 
ImposesAnnotation::transform_imposes_function(const parser::ast::function_definition& func_def, 
                                             const std::vector<std::string>& effect_types,
                                             MacroExpander& expander) {
    
    // Validate effect usage against @imposes declaration
    if (auto validation_result = validate_effect_usage(func_def, effect_types); !validation_result) {
        return std::unexpected(validation_result.error());
    }
    
    // Generate effect metadata for the function
    auto metadata = generate_effect_metadata(func_def.name.name, effect_types);
    
    // Create a compound AST node containing:
    // 1. Original function definition (preserved)
    // 2. Effect metadata registration
    
    std::vector<kernel::Value> elements = {
        kernel::Value(kernel::SymbolTable::instance().intern("begin")),
        
        // Original function definition (preserved)
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("function")),
            kernel::Value(kernel::SymbolTable::instance().intern(func_def.name.name)),
            // TODO: Add function parameters, return type, and body
            // For now, we'll create a placeholder
            kernel::list({}), // parameters
            kernel::Value(kernel::SymbolTable::instance().intern("void")), // return type
            kernel::list({}) // body
        }),
        
        // Effect metadata registration
        metadata,
        
        // Runtime registration call - ensure the function effects are registered when the module loads
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("call")),
            kernel::Value(kernel::SymbolTable::instance().intern("register_function_effects_at_runtime")),
            kernel::Value(std::make_shared<kernel::String>(func_def.name.name)),
            kernel::list([&effect_types]() {
                std::vector<kernel::Value> effects;
                for (const auto& effect : effect_types) {
                    effects.push_back(kernel::Value(std::make_shared<kernel::String>(effect)));
                }
                return effects;
            }())
        })
    };
    
    return kernel::list(elements);
}

std::expected<std::vector<std::string>, std::string>
ImposesAnnotation::parse_imposes_parameters(const kernel::Value& annotation_ast) {
    // Parse @imposes(EffectType1, EffectType2, ...) parameters
    
    std::vector<std::string> effect_types;
    
    // TODO: Implement proper AST parsing for annotation parameters
    // For now, return empty vector as placeholder
    
    return effect_types;
}

std::expected<void, std::string> 
ImposesAnnotation::validate_effect_usage(const parser::ast::function_definition& func_def,
                                         const std::vector<std::string>& declared_effects) {
    
    // Extract performed effects from function body
    auto performed_effects = extract_performed_effects(func_def.body.get());
    
    // Convert declared effects to set for efficient lookup
    std::set<std::string> declared_set(declared_effects.begin(), declared_effects.end());
    
    // Check that all performed effects are declared
    for (const auto& performed_effect : performed_effects) {
        if (declared_set.find(performed_effect) == declared_set.end()) {
            return std::unexpected(
                "Function '" + func_def.name.name + "' performs effect '" + 
                performed_effect + "' but does not declare it in @imposes annotation"
            );
        }
    }
    
    return {};
}

std::set<std::string> 
ImposesAnnotation::extract_performed_effects(const parser::ast::block_expression& body) {
    
    std::set<std::string> performed_effects;
    
    // Use the block search method
    find_perform_calls_in_block(body, performed_effects);
    
    return performed_effects;
}

kernel::Value ImposesAnnotation::generate_effect_metadata(
    const std::string& function_name,
    const std::vector<std::string>& effect_types) {
    
    // Generate comprehensive effect metadata that includes:
    // 1. Function effect registration for runtime querying
    // 2. Compile-time metadata for validation
    // 3. Debug information for tooling
    
    std::vector<kernel::Value> effect_list;
    for (const auto& effect : effect_types) {
        effect_list.push_back(kernel::Value(std::make_shared<kernel::String>(effect)));
    }
    
    // Create a compound metadata block
    std::vector<kernel::Value> metadata_elements = {
        kernel::Value(kernel::SymbolTable::instance().intern("begin")),
        
        // Runtime registration call
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("call")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("method")),
                kernel::Value(kernel::SymbolTable::instance().intern("FunctionEffectRegistry")),
                kernel::Value(kernel::SymbolTable::instance().intern("register_function_effects"))
            }),
            kernel::Value(std::make_shared<kernel::String>(function_name)),
            kernel::list(effect_list)
        }),
        
        // Compile-time metadata for validation and tooling
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("metadata")),
            kernel::Value(kernel::SymbolTable::instance().intern("function_effects")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("function_name")),
                kernel::Value(std::make_shared<kernel::String>(function_name))
            }),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("declared_effects")),
                kernel::list(effect_list)
            })
        }),
        
        // Debug information for IDE and tooling support
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("debug_info")),
            kernel::Value(kernel::SymbolTable::instance().intern("effect_annotation")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("annotation_type")),
                kernel::Value(std::make_shared<kernel::String>("@imposes"))
            }),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("effect_count")),
                kernel::Value::from_int(static_cast<int64_t>(effect_types.size()))
            })
        })
    };
    
    return kernel::list(metadata_elements);
}

bool ImposesAnnotation::is_perform_call(const parser::ast::expression& expr) {
    // Check if expression is a perform_expression
    return boost::get<x3::forward_ast<parser::ast::perform_expression>>(&expr) != nullptr;
}

std::expected<std::string, std::string>
ImposesAnnotation::extract_effect_from_perform(const parser::ast::expression& expr) {
    // Extract effect type from perform_expression
    
    if (!is_perform_call(expr)) {
        return std::unexpected("Expression is not a perform() call");
    }
    
    // Get the perform_expression from the variant
    const auto& perform_expr = boost::get<x3::forward_ast<parser::ast::perform_expression>>(expr).get();
    
    // Return the effect name
    return perform_expr.effect_name.name;
}

void ImposesAnnotation::find_perform_calls_in_expression(
    const parser::ast::expression& expr,
    std::set<std::string>& performed_effects) {
    
    // Check if this expression is a perform call
    if (is_perform_call(expr)) {
        auto effect_result = extract_effect_from_perform(expr);
        if (effect_result) {
            performed_effects.insert(effect_result.value());
        }
        return;
    }
    
    // Recursively search in sub-expressions based on expression type
    meld::compat::visit([&](const auto& variant_expr) {
        using T = std::decay_t<decltype(variant_expr)>;
        
        if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::function_call>>) {
            const auto& func_call = variant_expr.get();
            // Search in arguments
            for (const auto& arg : func_call.arguments) {
                find_perform_calls_in_expression(arg, performed_effects);
            }
            for (const auto& named_arg : func_call.named_arguments) {
                find_perform_calls_in_expression(named_arg.value, performed_effects);
            }
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& bin_op = variant_expr.get();
            find_perform_calls_in_expression(bin_op.left.get(), performed_effects);
            find_perform_calls_in_expression(bin_op.right.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::unary_operation>>) {
            const auto& unary_op = variant_expr.get();
            find_perform_calls_in_expression(unary_op.operand.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::list_expression>>) {
            const auto& list_expr = variant_expr.get();
            for (const auto& element : list_expr.elements) {
                find_perform_calls_in_expression(element, performed_effects);
            }
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::lambda_expression>>) {
            const auto& lambda_expr = variant_expr.get();
            find_perform_calls_in_expression(lambda_expr.body.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::match_expression>>) {
            const auto& match_expr = variant_expr.get();
            find_perform_calls_in_expression(match_expr.matched_value.get(), performed_effects);
            for (const auto& case_expr : match_expr.cases) {
                find_perform_calls_in_expression(case_expr.result_expression, performed_effects);
            }
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::pipeline_expression>>) {
            const auto& pipeline_expr = variant_expr.get();
            find_perform_calls_in_expression(pipeline_expr.value.get(), performed_effects);
            find_perform_calls_in_expression(pipeline_expr.function.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::handle_expression>>) {
            const auto& handle_expr = variant_expr.get();
            // Search in the body of the handle expression
            find_perform_calls_in_block(handle_expr.body.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::safe_navigation_expression>>) {
            const auto& safe_nav = variant_expr.get();
            find_perform_calls_in_expression(safe_nav.nullable_expr.get(), performed_effects);
        }
        else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::elvis_expression>>) {
            const auto& elvis_expr = variant_expr.get();
            find_perform_calls_in_expression(elvis_expr.nullable_expr.get(), performed_effects);
            find_perform_calls_in_expression(elvis_expr.default_value.get(), performed_effects);
        }
        // Add more expression types as needed
        // For simple types like identifier, literals, etc., no recursion needed
    }, expr);
}

void ImposesAnnotation::find_perform_calls_in_statement(
    const parser::ast::expression& stmt,
    std::set<std::string>& performed_effects) {
    
    // In this AST, statements are just expressions, so delegate to expression handler
    find_perform_calls_in_expression(stmt, performed_effects);
}

void ImposesAnnotation::find_perform_calls_in_block(
    const parser::ast::block_expression& block,
    std::set<std::string>& performed_effects) {
    
    // Search through all statements in the block
    for (const auto& stmt : block.statements) {
        find_perform_calls_in_expression(stmt, performed_effects);
    }
}

// FunctionEffectRegistry implementation

void FunctionEffectRegistry::register_function_effects(const std::string& function_name, 
                                                      const std::vector<std::string>& effects) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for duplicate registration
    if (function_effects_.find(function_name) != function_effects_.end()) {
        // In a real implementation, this might be an error or warning
        // For now, we'll allow re-registration (overwrite)
    }
    
    function_effects_[function_name] = effects;
}

std::expected<std::vector<std::string>, std::string> 
FunctionEffectRegistry::get_function_effects(const std::string& function_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = function_effects_.find(function_name);
    if (it == function_effects_.end()) {
        return std::unexpected("Function '" + function_name + "' not found in effect registry");
    }
    
    return it->second;
}

bool FunctionEffectRegistry::has_function_effects(const std::string& function_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return function_effects_.find(function_name) != function_effects_.end();
}

void FunctionEffectRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    function_effects_.clear();
}

} // namespace meld::macro