#include "meld/macro/handle_macro.hpp"
#include "meld/macro/macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/stdlib/effects.hpp"
#include <format>

namespace meld::macro {

// Handle macro implementation (unified syntax)
//
// The handle macro expands from:
//   handle({ body }, Effect1 { fnc op1() { ... } }, Effect2 { fnc op2() { ... } })
//
// To:
//   {
//       // Install handlers (one EffectScope per effect)
//       auto handler1 = create_effect_handler("Effect1");
//       handler1->set_handler("op1", [](args, cont) { ... });
//       EffectScope scope1("Effect1", handler1);
//
//       auto handler2 = create_effect_handler("Effect2");
//       handler2->set_handler("op2", [](args, cont) { ... });
//       EffectScope scope2("Effect2", handler2);
//
//       // Execute body and return result
//       val result = body()
//       result
//   }

std::expected<kernel::Value, std::string> 
expand_handle_macro(const kernel::Value& ast_node, MacroExpander& expander) {
    auto args_result = expander.get_macro_args(ast_node);
    if (!args_result) {
        return std::unexpected("handle macro: " + args_result.error());
    }
    
    const auto& args = *args_result;
    
    // Unified syntax: handle(body, handler1, handler2, ...)
    // First arg is the computation body, rest are effect handler groups
    if (args.size() < 2) {
        return std::unexpected(
            "handle macro expects at least 2 arguments: "
            "handle({ body }, EffectName { fnc op() { ... } })");
    }
    
    const auto& body = args[0];
    
    // Parse each handler argument into an EffectHandlerGroup
    std::vector<EffectHandlerGroup> handler_groups;
    
    for (size_t i = 1; i < args.size(); ++i) {
        const auto& handler_arg = args[i];
        
        // Each handler arg should be a cons cell: (effect_name . handlers_list)
        // The parser wraps inline_trait_impl as: (effect_name (op1 op2 ...))
        if (!handler_arg.is<kernel::Cons>()) {
            return std::unexpected(std::format(
                "handle macro: argument {} must be an inline trait implementation "
                "(EffectName {{ fnc op() {{ ... }} }})", i + 1));
        }
        
        // Extract effect name
        auto effect_name_val = kernel::car(handler_arg);
        if (!effect_name_val || !effect_name_val->is<kernel::Symbol>()) {
            return std::unexpected(std::format(
                "handle macro: argument {} must start with an effect name", i + 1));
        }
        
        std::string effect_name = effect_name_val->as<kernel::Symbol>()->name();
        
        // Parse handler definitions from the rest
        auto handlers_ast = kernel::cdr(handler_arg);
        if (!handlers_ast) {
            return std::unexpected(std::format(
                "handle macro: error getting handler body for '{}'", effect_name));
        }
        auto handler_defs = parse_handler_definitions(*handlers_ast);
        if (!handler_defs) {
            return std::unexpected(std::format(
                "handle macro: error in handler for '{}': {}", 
                effect_name, handler_defs.error()));
        }
        
        handler_groups.push_back(EffectHandlerGroup{
            .effect_name = effect_name,
            .operations = *handler_defs
        });
    }
    
    return generate_handle_expansion(body, handler_groups, expander);
}

std::expected<std::vector<HandlerDefinition>, std::string>
parse_handler_definitions(const kernel::Value& handlers_ast) {
    std::vector<HandlerDefinition> handlers;
    
    if (!handlers_ast.is<kernel::Cons>()) {
        return std::unexpected("Handler definitions must be a list");
    }
    
    auto handlers_list = kernel::list_to_array(handlers_ast);
    if (!handlers_list) {
        return std::unexpected("Failed to parse handlers list: " + handlers_list.error());
    }
    
    for (const auto& handler_ast : *handlers_list) {
        auto handler_def = parse_single_handler(handler_ast);
        if (!handler_def) {
            return std::unexpected("Failed to parse handler: " + handler_def.error());
        }
        handlers.push_back(*handler_def);
    }
    
    return handlers;
}

std::expected<HandlerDefinition, std::string>
parse_single_handler(const kernel::Value& handler_ast) {
    if (!handler_ast.is<kernel::Cons>()) {
        return std::unexpected("Handler must be a list");
    }
    
    auto handler_list = kernel::list_to_array(handler_ast);
    if (!handler_list || handler_list->size() != 3) {
        return std::unexpected("Handler must have format: (operation_name (params...) body)");
    }
    
    const auto& op_name_ast = (*handler_list)[0];
    const auto& params_ast = (*handler_list)[1];
    const auto& body_ast = (*handler_list)[2];
    
    if (!op_name_ast.is<kernel::Symbol>()) {
        return std::unexpected("Handler operation name must be a symbol");
    }
    
    std::string operation_name = op_name_ast.as<kernel::Symbol>()->name();
    
    std::vector<std::string> params;
    if (params_ast.is<kernel::Cons>()) {
        auto params_list = kernel::list_to_array(params_ast);
        if (!params_list) {
            return std::unexpected("Failed to parse handler parameters: " + params_list.error());
        }
        
        for (const auto& param : *params_list) {
            if (!param.is<kernel::Symbol>()) {
                return std::unexpected("Handler parameter must be a symbol");
            }
            params.push_back(param.as<kernel::Symbol>()->name());
        }
    }
    
    return HandlerDefinition{
        .operation_name = operation_name,
        .parameters = params,
        .body = body_ast
    };
}

kernel::Value generate_handle_expansion(
    const kernel::Value& body,
    const std::vector<EffectHandlerGroup>& handler_groups,
    MacroExpander& expander) {
    
    std::vector<kernel::Value> block_contents;
    
    // For each effect handler group, generate handler setup + scope
    for (const auto& group : handler_groups) {
        auto handler_var = expander.gensym("handler");
        auto scope_var = expander.gensym("scope");
        
        // Create handler: val handler = create_effect_handler("EffectName")
        auto create_handler_call = kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("create_effect_handler")),
            kernel::Value(std::make_shared<kernel::String>(group.effect_name))
        });
        
        block_contents.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("val")),
            kernel::Value(handler_var),
            create_handler_call
        }));
        
        // Register each operation handler
        for (const auto& op : group.operations) {
            block_contents.push_back(
                generate_set_handler_call(handler_var, op, expander));
        }
        
        // Create RAII scope: val scope = EffectScope("EffectName", handler)
        block_contents.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("val")),
            kernel::Value(scope_var),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("EffectScope")),
                kernel::Value(std::make_shared<kernel::String>(group.effect_name)),
                kernel::Value(handler_var)
            })
        }));
    }
    
    // Execute body and capture result
    auto result_var = expander.gensym("result");
    block_contents.push_back(kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("val")),
        kernel::Value(result_var),
        body
    }));
    
    // Return result
    block_contents.push_back(kernel::Value(result_var));
    
    return kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("block")),
        kernel::list(block_contents)
    });
}

kernel::Value generate_set_handler_call(
    std::shared_ptr<kernel::Symbol> handler_var,
    const HandlerDefinition& handler_def,
    MacroExpander& expander) {
    
    // Generate: handler.set_handler("operation_name", [](args, cont) { body })
    
    std::vector<kernel::Value> lambda_params;
    auto args_param = expander.gensym("args");
    lambda_params.push_back(kernel::Value(args_param));
    auto cont_param = expander.gensym("cont");
    lambda_params.push_back(kernel::Value(cont_param));
    
    std::vector<kernel::Value> lambda_body_stmts;
    
    // Extract individual arguments from args vector
    for (size_t i = 0; i < handler_def.parameters.size(); ++i) {
        auto param_name = kernel::SymbolTable::instance().intern(handler_def.parameters[i]);
        lambda_body_stmts.push_back(kernel::list({
            kernel::Value(kernel::SymbolTable::instance().intern("val")),
            kernel::Value(param_name),
            kernel::list({
                kernel::Value(kernel::SymbolTable::instance().intern("get")),
                kernel::Value(args_param),
                kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(i)))
            })
        }));
    }
    
    lambda_body_stmts.push_back(handler_def.body);
    
    auto lambda_expr = kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("lambda")),
        kernel::list(lambda_params),
        kernel::list(lambda_body_stmts)
    });
    
    return kernel::list({
        kernel::Value(kernel::SymbolTable::instance().intern("call")),
        kernel::Value(handler_var),
        kernel::Value(kernel::SymbolTable::instance().intern("set_handler")),
        kernel::Value(std::make_shared<kernel::String>(handler_def.operation_name)),
        lambda_expr
    });
}

void register_handle_macro() {
    auto handle_macro = make_macro(
        "handle",
        {"body", "...handlers"},  // varargs: body + N handler groups
        expand_handle_macro
    );
    
    MacroRegistry::instance().register_macro(handle_macro);
}

} // namespace meld::macro
