#include "meld/macro/bootstrap.hpp"
#include "meld/macro/blueprint.hpp"
#include "meld/macro/extern_macro.hpp"
#include "meld/macro/handle_macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/effects/effect.hpp"
#include <format>

namespace meld::macro {

// Register all bootstrap macros
void register_bootstrap_macros() {
    auto& registry = MacroRegistry::instance();
    
    registry.register_macro(create_class_macro());
    registry.register_macro(create_struct_macro());
    registry.register_macro(create_trait_macro());
    registry.register_macro(create_effect_macro());
    
    // Register @extern macro for FFI
    register_extern_macro();
    register_extern_decorator();
    
    // Register handle macro for algebraic effects
    register_handle_macro();
}

// Register all AI-native macros (including @blueprint)
void register_ai_macros() {
    register_blueprint_macros();
}

// class macro implementation
std::shared_ptr<Macro> create_class_macro() {
    auto transformer = [](const kernel::Value& ast_node, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
        
        // Extract arguments: (class ClassName fields... methods...)
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        auto& args = *args_result;
        if (args.empty()) {
            return std::unexpected("class macro requires at least a name");
        }
        
        // First argument is the class name
        if (!args[0].is<kernel::Symbol>()) {
            return std::unexpected("class name must be a symbol");
        }
        std::string class_name = args[0].as<kernel::Symbol>()->name();
        
        // Separate fields and methods
        std::vector<kernel::Value> field_defs;
        std::vector<kernel::Value> method_defs;
        
        for (size_t i = 1; i < args.size(); ++i) {
            // Check if it's a field or method definition
            if (args[i].is<kernel::Cons>()) {
                auto list_result = kernel::list_to_array(args[i]);
                if (list_result && !list_result->empty()) {
                    auto& first = (*list_result)[0];
                    if (first.is<kernel::Symbol>()) {
                        std::string tag = first.as<kernel::Symbol>()->name();
                        if (tag == "field") {
                            field_defs.push_back(args[i]);
                        } else if (tag == "method") {
                            method_defs.push_back(args[i]);
                        }
                    }
                }
            }
        }
        
        // Extract fields and methods
        auto fields_result = extract_fields(field_defs);
        if (!fields_result) {
            return std::unexpected(fields_result.error());
        }
        
        auto methods_result = extract_methods(method_defs);
        if (!methods_result) {
            return std::unexpected(methods_result.error());
        }
        
        // Create ClassMetaType
        auto class_type = meta::MetaType::create_class(
            class_name,
            *fields_result,
            *methods_result
        );
        
        // Register the type
        meta::TypeRegistry::instance().register_type(class_name, class_type);
        
        // Generate constructor
        auto ctor = generate_constructor(class_name, *fields_result);
        
        // Generate method functions
        auto method_fns = generate_methods(class_name, *methods_result);
        
        // Return AST that defines the class
        // (begin (val ClassName <type>) <ctor> <methods>...)
        std::vector<kernel::Value> result_elements;
        result_elements.push_back(kernel::Value(kernel::SymbolTable::instance().intern("begin")));
        
        // (val ClassName <type>)
        result_elements.push_back(kernel::make_val_decl(class_name, 
            kernel::Value(std::make_shared<kernel::String>(class_name))));
        
        result_elements.push_back(ctor);
        
        for (auto& method_fn : method_fns) {
            result_elements.push_back(method_fn);
        }
        
        return kernel::list(result_elements);
    };
    
    return make_macro("class", {"name", "..."}, transformer);
}

// struct macro implementation
std::shared_ptr<Macro> create_struct_macro() {
    auto transformer = [](const kernel::Value& ast_node, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
        
        // Extract arguments: (struct StructName fields...)
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        auto& args = *args_result;
        if (args.empty()) {
            return std::unexpected("struct macro requires at least a name");
        }
        
        // First argument is the struct name
        if (!args[0].is<kernel::Symbol>()) {
            return std::unexpected("struct name must be a symbol");
        }
        std::string struct_name = args[0].as<kernel::Symbol>()->name();
        
        // Rest are field definitions
        std::vector<kernel::Value> field_defs(args.begin() + 1, args.end());
        
        // Extract fields
        auto fields_result = extract_fields(field_defs);
        if (!fields_result) {
            return std::unexpected(fields_result.error());
        }
        
        // Create StructMetaType
        auto struct_type = meta::MetaType::create_struct(struct_name, *fields_result);
        
        // Register the type
        meta::TypeRegistry::instance().register_type(struct_name, struct_type);
        
        // Generate constructor
        auto ctor = generate_constructor(struct_name, *fields_result);
        
        // Return AST that defines the struct
        // (begin (val StructName <type>) <ctor>)
        return kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("begin")),
            kernel::make_val_decl(struct_name, 
                kernel::Value(std::make_shared<kernel::String>(struct_name))),
            ctor
        });
    };
    
    return make_macro("struct", {"name", "..."}, transformer);
}

// trait macro implementation
std::shared_ptr<Macro> create_trait_macro() {
    auto transformer = [](const kernel::Value& ast_node, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
        
        // Extract arguments: (trait TraitName methods...)
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        auto& args = *args_result;
        if (args.empty()) {
            return std::unexpected("trait macro requires at least a name");
        }
        
        // First argument is the trait name
        if (!args[0].is<kernel::Symbol>()) {
            return std::unexpected("trait name must be a symbol");
        }
        std::string trait_name = args[0].as<kernel::Symbol>()->name();
        
        // Rest are method definitions
        std::vector<kernel::Value> method_defs(args.begin() + 1, args.end());
        
        // Extract methods
        auto methods_result = extract_methods(method_defs);
        if (!methods_result) {
            return std::unexpected(methods_result.error());
        }
        
        // Create TraitMetaType
        auto trait_type = meta::MetaType::create_trait(trait_name, *methods_result);
        
        // Register the type
        meta::TypeRegistry::instance().register_type(trait_name, trait_type);
        
        // Generate method functions
        auto method_fns = generate_methods(trait_name, *methods_result);
        
        // Return AST that defines the trait
        // (begin (val TraitName <type>) <methods>...)
        std::vector<kernel::Value> result_elements;
        result_elements.push_back(kernel::Value(kernel::SymbolTable::instance().intern("begin")));
        
        result_elements.push_back(kernel::make_val_decl(trait_name, 
            kernel::Value(std::make_shared<kernel::String>(trait_name))));
        
        for (auto& method_fn : method_fns) {
            result_elements.push_back(method_fn);
        }
        
        return kernel::list(result_elements);
    };
    
    return make_macro("trait", {"name", "..."}, transformer);
}

// effect macro implementation
std::shared_ptr<Macro> create_effect_macro() {
    auto transformer = [](const kernel::Value& ast_node, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
        
        // Extract arguments: (effect EffectName operations...)
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        auto& args = *args_result;
        if (args.empty()) {
            return std::unexpected("effect macro requires at least a name");
        }
        
        // First argument is the effect name
        if (!args[0].is<kernel::Symbol>()) {
            return std::unexpected("effect name must be a symbol");
        }
        std::string effect_name = args[0].as<kernel::Symbol>()->name();
        
        // Rest are operation definitions
        std::vector<kernel::Value> operation_defs(args.begin() + 1, args.end());
        
        // Extract operations
        auto operations_result = extract_effect_operations(operation_defs);
        if (!operations_result) {
            return std::unexpected(operations_result.error());
        }
        
        // Create EffectDefinition
        auto effect_def = std::make_shared<effects::EffectDefinition>(effect_name);
        for (const auto& op : *operations_result) {
            effect_def->add_operation(op.name, op.parameter_types, op.return_type);
        }
        
        // Generate dispatch logic
        auto dispatch = generate_effect_dispatch(effect_name, *operations_result);
        
        // Return AST that defines the effect
        // (begin (val EffectName <effect-def>) <dispatch>)
        return kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("begin")),
            kernel::make_val_decl(effect_name, 
                kernel::Value(std::make_shared<kernel::String>(effect_name))),
            dispatch
        });
    };
    
    return make_macro("effect", {"name", "..."}, transformer);
}

// Helper function implementations

std::expected<std::vector<meta::Field>, std::string>
extract_fields(const std::vector<kernel::Value>& field_defs) {
    std::vector<meta::Field> fields;
    
    for (const auto& field_def : field_defs) {
        // Field definition: (field name type [mutable])
        if (!field_def.is<kernel::Cons>()) {
            return std::unexpected("Field definition must be a list");
        }
        
        auto list_result = kernel::list_to_array(field_def);
        if (!list_result) {
            return std::unexpected(list_result.error());
        }
        
        auto& elements = *list_result;
        if (elements.size() < 3) {
            return std::unexpected("Field definition requires at least (field name type)");
        }
        
        // elements[0] is "field" tag
        // elements[1] is field name
        // elements[2] is field type
        // elements[3] (optional) is mutability flag
        
        if (!elements[1].is<kernel::Symbol>()) {
            return std::unexpected("Field name must be a symbol");
        }
        std::string field_name = elements[1].as<kernel::Symbol>()->name();
        
        if (!elements[2].is<kernel::Symbol>()) {
            return std::unexpected("Field type must be a symbol");
        }
        std::string type_name = elements[2].as<kernel::Symbol>()->name();
        
        // Look up type
        auto type_result = meta::TypeRegistry::instance().get_type(type_name);
        if (!type_result) {
            return std::unexpected(std::format("Unknown type: {}", type_name));
        }
        
        bool is_mutable = false;
        if (elements.size() > 3 && elements[3].is<kernel::Boolean>()) {
            is_mutable = elements[3].as<kernel::Boolean>()->value();
        }
        
        fields.emplace_back(field_name, *type_result, is_mutable);
    }
    
    return fields;
}

std::expected<std::vector<meta::Method>, std::string>
extract_methods(const std::vector<kernel::Value>& method_defs) {
    std::vector<meta::Method> methods;
    
    for (const auto& method_def : method_defs) {
        // Method definition: (method name (param-types...) return-type body)
        if (!method_def.is<kernel::Cons>()) {
            return std::unexpected("Method definition must be a list");
        }
        
        auto list_result = kernel::list_to_array(method_def);
        if (!list_result) {
            return std::unexpected(list_result.error());
        }
        
        auto& elements = *list_result;
        if (elements.size() < 4) {
            return std::unexpected("Method definition requires (method name params return-type body)");
        }
        
        // elements[0] is "method" tag
        // elements[1] is method name
        // elements[2] is parameter types list
        // elements[3] is return type
        // elements[4] is body (optional for trait methods)
        
        if (!elements[1].is<kernel::Symbol>()) {
            return std::unexpected("Method name must be a symbol");
        }
        std::string method_name = elements[1].as<kernel::Symbol>()->name();
        
        // Extract parameter types
        std::vector<std::shared_ptr<meta::MetaType>> param_types;
        if (elements[2].is<kernel::Cons>()) {
            auto params_result = kernel::list_to_array(elements[2]);
            if (params_result) {
                for (const auto& param : *params_result) {
                    if (!param.is<kernel::Symbol>()) {
                        return std::unexpected("Parameter type must be a symbol");
                    }
                    std::string type_name = param.as<kernel::Symbol>()->name();
                    auto type_result = meta::TypeRegistry::instance().get_type(type_name);
                    if (!type_result) {
                        return std::unexpected(std::format("Unknown type: {}", type_name));
                    }
                    param_types.push_back(*type_result);
                }
            }
        }
        
        // Extract return type
        if (!elements[3].is<kernel::Symbol>()) {
            return std::unexpected("Return type must be a symbol");
        }
        std::string return_type_name = elements[3].as<kernel::Symbol>()->name();
        auto return_type_result = meta::TypeRegistry::instance().get_type(return_type_name);
        if (!return_type_result) {
            return std::unexpected(std::format("Unknown type: {}", return_type_name));
        }
        
        // Body (if present)
        kernel::Value body = kernel::Value(kernel::Empty::instance());
        if (elements.size() > 4) {
            body = elements[4];
        }
        
        methods.emplace_back(method_name, param_types, *return_type_result, body);
    }
    
    return methods;
}

kernel::Value generate_constructor(
    const std::string& name,
    const std::vector<meta::Field>& fields) {
    
    // Generate: (fn <name>.new (field1 field2 ...) (begin ...))
    std::vector<kernel::Value> params;
    for (const auto& field : fields) {
        params.push_back(kernel::Value(kernel::SymbolTable::instance().intern(field.name)));
    }
    
    auto ctor_name = std::format("{}.new", name);
    
    return kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("fn")),
        kernel::Value(kernel::SymbolTable::instance().intern(ctor_name)),
        kernel::list(params),
        kernel::Value(kernel::SymbolTable::instance().intern("begin"))
    });
}

std::vector<kernel::Value> generate_methods(
    const std::string& class_name,
    const std::vector<meta::Method>& methods) {
    
    std::vector<kernel::Value> method_fns;
    
    for (const auto& method : methods) {
        // Generate: (fn <class_name>.<method_name> (self params...) body)
        std::vector<kernel::Value> params;
        params.push_back(kernel::Value(kernel::SymbolTable::instance().intern("self")));
        
        // Add method parameters (we don't have names, so use generic names)
        for (size_t i = 0; i < method.param_types.size(); ++i) {
            params.push_back(kernel::Value(
                kernel::SymbolTable::instance().intern(std::format("arg{}", i))));
        }
        
        auto method_name = std::format("{}.{}", class_name, method.name);
        
        method_fns.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("fn")),
            kernel::Value(kernel::SymbolTable::instance().intern(method_name)),
            kernel::list(params),
            method.implementation
        }));
    }
    
    return method_fns;
}

std::expected<std::vector<effects::EffectOperation>, std::string>
extract_effect_operations(const std::vector<kernel::Value>& operation_defs) {
    std::vector<effects::EffectOperation> operations;
    
    for (const auto& operation_def : operation_defs) {
        // Operation definition: (operation name (param-types...) return-type)
        if (!operation_def.is<kernel::Cons>()) {
            return std::unexpected("Effect operation definition must be a list");
        }
        
        auto list_result = kernel::list_to_array(operation_def);
        if (!list_result) {
            return std::unexpected(list_result.error());
        }
        
        auto& elements = *list_result;
        if (elements.size() < 3) {
            return std::unexpected("Effect operation definition requires (operation name params return-type)");
        }
        
        // elements[0] is "operation" tag (or could be "fnc" for function-style)
        // elements[1] is operation name
        // elements[2] is parameter types list
        // elements[3] is return type
        
        if (!elements[1].is<kernel::Symbol>()) {
            return std::unexpected("Operation name must be a symbol");
        }
        std::string operation_name = elements[1].as<kernel::Symbol>()->name();
        
        // Extract parameter types
        std::vector<std::string> param_types;
        if (elements[2].is<kernel::Cons>()) {
            auto params_result = kernel::list_to_array(elements[2]);
            if (params_result) {
                for (const auto& param : *params_result) {
                    if (!param.is<kernel::Symbol>()) {
                        return std::unexpected("Parameter type must be a symbol");
                    }
                    std::string type_name = param.as<kernel::Symbol>()->name();
                    param_types.push_back(type_name);
                }
            }
        }
        
        // Extract return type
        std::string return_type = "void"; // Default return type
        if (elements.size() > 3) {
            if (!elements[3].is<kernel::Symbol>()) {
                return std::unexpected("Return type must be a symbol");
            }
            return_type = elements[3].as<kernel::Symbol>()->name();
        }
        
        // Validate that this is an abstract operation (no body)
        if (elements.size() > 4) {
            return std::unexpected("Effect operations must be abstract (no implementation body allowed)");
        }
        
        operations.emplace_back(operation_name, param_types, return_type);
    }
    
    return operations;
}

kernel::Value generate_effect_dispatch(
    const std::string& effect_name,
    const std::vector<effects::EffectOperation>& operations) {
    
    // Generate dispatch logic for the effect
    // This creates helper functions for each operation that call perform()
    
    std::vector<kernel::Value> dispatch_functions;
    
    for (const auto& op : operations) {
        // Generate: (fn <effect_name>.<operation_name> (args...) 
        //             (perform "<effect_name>" "<operation_name>" (list args...)))
        
        std::vector<kernel::Value> params;
        std::vector<kernel::Value> arg_list;
        
        for (size_t i = 0; i < op.parameter_types.size(); ++i) {
            std::string param_name = std::format("arg{}", i);
            params.push_back(kernel::Value(kernel::SymbolTable::instance().intern(param_name)));
            arg_list.push_back(kernel::Value(kernel::SymbolTable::instance().intern(param_name)));
        }
        
        auto function_name = std::format("{}.{}", effect_name, op.name);
        
        // Create the perform call: (perform "EffectName" "operation" (list args...))
        auto perform_call = kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("perform")),
            kernel::Value(std::make_shared<kernel::String>(effect_name)),
            kernel::Value(std::make_shared<kernel::String>(op.name)),
            kernel::list(arg_list)
        });
        
        dispatch_functions.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("fn")),
            kernel::Value(kernel::SymbolTable::instance().intern(function_name)),
            kernel::list(params),
            perform_call
        }));
    }
    
    // Return a begin block with all dispatch functions
    std::vector<kernel::Value> result_elements;
    result_elements.push_back(kernel::Value(kernel::SymbolTable::instance().intern("begin")));
    
    for (auto& dispatch_fn : dispatch_functions) {
        result_elements.push_back(dispatch_fn);
    }
    
    return kernel::list(result_elements);
}

} // namespace meld::macro
