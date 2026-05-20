#include "meld/macro/effect_annotation.hpp"
#include "meld/parser/ast.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <sstream>
#include <algorithm>

namespace meld::macro {

void EffectAnnotation::register_annotation() {
    auto decorator = make_decorator("effect", 
        [](const parser::ast::class_definition& class_def, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
            
            // Apply @effect transformation to class definition (representing a trait)
            return transform_effect_class(class_def, expander);
        }
    );
    
    DecoratorRegistry::instance().register_decorator(decorator);
}

std::expected<kernel::Value, std::string> 
EffectAnnotation::transform_effect_class(const parser::ast::class_definition& class_def, 
                                        MacroExpander& expander) {
    
    // Validate that @effect is applied correctly
    if (auto validation_result = validate_effect_class(class_def); !validation_result) {
        return std::unexpected(validation_result.error());
    }
    
    // Extract effect operations from class methods
    auto operations = extract_effect_operations(class_def);
    
    // Generate effect registration code
    auto registration_code = generate_effect_registration(class_def.name.name, operations);
    
    // Generate effect handler infrastructure
    auto handler_code = generate_effect_handler_class(class_def.name.name, operations);
    
    // Create a compound AST node containing:
    // 1. Original class definition (preserved as trait)
    // 2. Effect registration code
    // 3. Handler infrastructure
    
    std::vector<kernel::Value> elements = {
        kernel::Value(kernel::SymbolTable::instance().intern("begin")),
        
        // Original class definition (preserved as trait)
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("trait")),
            kernel::Value(kernel::SymbolTable::instance().intern(class_def.name.name))
            // Add trait methods here - for now, we'll preserve the structure
        }),
        
        // Effect registration - this will be called at compile time
        registration_code,
        
        // Handler infrastructure
        handler_code,
        
        // Runtime registration call - ensure the effect is registered when the module loads
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("call")),
            kernel::Value(kernel::SymbolTable::instance().intern("register_effect_at_runtime")),
            kernel::Value(std::make_shared<kernel::String>(class_def.name.name))
        })
    };
    
    return kernel::list(elements);
}

kernel::Value EffectAnnotation::generate_effect_registration(
    const std::string& effect_name,
    const std::vector<parser::ast::function_definition>& operations) {
    
    // Generate EffectRegistry.register() call
    std::vector<kernel::Value> operation_descriptors;
    
    for (const auto& op : operations) {
        // Create EffectOperation descriptor
        std::vector<kernel::Value> params;
        for (const auto& param : op.parameters) {
            params.push_back(kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("Parameter")),
                kernel::Value(std::make_shared<kernel::String>(param.name.name)),
                kernel::Value(std::make_shared<kernel::String>(!param.type.type_name.name.empty() ? param.type.type_name.name : "any"))
            }));
        }
        
        operation_descriptors.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("EffectOperation")),
            kernel::Value(std::make_shared<kernel::String>(op.name.name)),
            kernel::list(params),
            kernel::Value(std::make_shared<kernel::String>(op.has_return_type ? op.return_type.type_name.name : "void"))
        }));
    }
    
    return kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("call")),
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("method")),
            kernel::Value(kernel::SymbolTable::instance().intern("EffectRegistry")),
            kernel::Value(kernel::SymbolTable::instance().intern("register"))
        }),
        kernel::Value(std::make_shared<kernel::String>(effect_name)),
        kernel::list(operation_descriptors)
    });
}

kernel::Value EffectAnnotation::generate_effect_handler_class(
    const std::string& effect_name,
    const std::vector<parser::ast::function_definition>& operations) {
    
    std::string handler_class_name = effect_name + "Handler";
    
    // Generate handler methods for each operation
    std::vector<kernel::Value> handler_methods;
    
    // Add constructor
    handler_methods.push_back(kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("constructor")),
        kernel::list({}), // No parameters
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("block")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("call")),
                kernel::Value(kernel::SymbolTable::instance().intern("super"))
            })
        })
    }));
    
    // Add effect_name method
    handler_methods.push_back(kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("method")),
        kernel::Value(kernel::SymbolTable::instance().intern("effect_name")),
        kernel::list({}), // No parameters
        kernel::Value(kernel::SymbolTable::instance().intern("string")),
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("block")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("return")),
                kernel::Value(std::make_shared<kernel::String>(effect_name))
            })
        })
    }));
    
    for (const auto& op : operations) {
        // Generate a handler method that dispatches to user-provided handlers
        std::vector<kernel::Value> params;
        for (const auto& param : op.parameters) {
            params.push_back(kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("param")),
                kernel::Value(kernel::SymbolTable::instance().intern(param.name.name)),
                kernel::Value(kernel::SymbolTable::instance().intern(!param.type.type_name.name.empty() ? param.type.type_name.name : "any"))
            }));
        }
        
        handler_methods.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("method")),
            kernel::Value(kernel::SymbolTable::instance().intern(op.name.name)),
            kernel::list(params),
            kernel::Value(kernel::SymbolTable::instance().intern(op.has_return_type ? op.return_type.type_name.name : "void")),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("block")),
                // Handler dispatch logic - delegate to user-provided handler
                kernel::list({
                    kernel::Value(kernel::SymbolTable::instance().intern("call")),
                    kernel::Value(kernel::SymbolTable::instance().intern("dispatch_operation")),
                    kernel::Value(std::make_shared<kernel::String>(op.name.name)),
                    kernel::Value(kernel::SymbolTable::instance().intern("args"))
                })
            })
        }));
    }
    
    return kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("class")),
        kernel::Value(kernel::SymbolTable::instance().intern(handler_class_name)),
        kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("extends")),
            kernel::Value(kernel::SymbolTable::instance().intern("EffectHandler"))
        }),
        kernel::list(handler_methods)
    });
}

kernel::Value EffectAnnotation::generate_effect_operations(
    const std::vector<parser::ast::function_definition>& operations) {
    
    std::vector<kernel::Value> operation_list;
    
    for (const auto& op : operations) {
        std::vector<kernel::Value> params;
        for (const auto& param : op.parameters) {
            params.push_back(kernel::list({
                kernel::Value(std::make_shared<kernel::String>(param.name.name)),
                kernel::Value(std::make_shared<kernel::String>(!param.type.type_name.name.empty() ? param.type.type_name.name : "any"))
            }));
        }
        
        operation_list.push_back(kernel::list({
            kernel::Value(std::make_shared<kernel::String>(op.name.name)),
            kernel::list(params),
            kernel::Value(std::make_shared<kernel::String>(op.has_return_type ? op.return_type.type_name.name : "void"))
        }));
    }
    
    return kernel::list(operation_list);
}

std::expected<void, std::string> 
EffectAnnotation::validate_effect_class(const parser::ast::class_definition& class_def) {
    
    // Check that class name is valid
    if (class_def.name.name.empty()) {
        return std::unexpected("@effect class must have a valid name");
    }
    
    // Check that the class name follows naming conventions (starts with uppercase)
    if (!std::isupper(class_def.name.name[0])) {
        return std::unexpected("@effect class name must start with an uppercase letter");
    }
    
    // Check that the class doesn't have fields (effects should be pure interfaces)
    if (!class_def.fields.empty()) {
        return std::unexpected("@effect classes cannot have fields - they must be pure interfaces");
    }
    
    // Check that the class doesn't have properties (effects should be pure interfaces)
    if (!class_def.properties.empty()) {
        return std::unexpected("@effect classes cannot have properties - they must be pure interfaces");
    }
    
    // Note: In the current AST structure, we can't check for methods since class_definition
    // doesn't include methods. In a real implementation, we would:
    // 1. Check that all methods are abstract (no implementation)
    // 2. Check that methods have proper signatures
    // 3. Ensure at least one method exists
    
    return {};
}

std::vector<parser::ast::function_definition> 
EffectAnnotation::extract_effect_operations(const parser::ast::class_definition& class_def) {
    
    // For now, return empty vector since class_definition doesn't have methods
    // In a real implementation, we'd need to extend the AST to include methods in classes
    // or use a different approach to represent traits
    
    std::vector<parser::ast::function_definition> operations;
    
    // This is a placeholder - in a real implementation, we'd extract methods from the class
    // that are marked as abstract or have no body
    
    return operations;
}

// EffectRegistry implementation

void EffectRegistry::register_effect(const std::string& name, 
                                   const std::vector<EffectOperation>& operations) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for duplicate registration
    if (effects_.find(name) != effects_.end()) {
        // In a real implementation, this might be an error or warning
        // For now, we'll allow re-registration (overwrite)
    }
    
    effects_[name] = EffectDescriptor(name, operations);
}

std::expected<EffectDescriptor, std::string> 
EffectRegistry::get_effect(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = effects_.find(name);
    if (it == effects_.end()) {
        return std::unexpected("Effect '" + name + "' not found in registry");
    }
    
    return it->second;
}

bool EffectRegistry::has_effect(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return effects_.find(name) != effects_.end();
}

void EffectRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    effects_.clear();
}

} // namespace meld::macro