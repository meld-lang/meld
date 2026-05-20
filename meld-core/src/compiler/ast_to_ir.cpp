#include "meld/compiler/ast_to_ir.hpp"
#include "meld/compat/visit.hpp"
#include <sstream>

namespace meld::compiler {

ASTToIR::ASTToIR(std::shared_ptr<ir::Module> module)
    : module_(std::move(module))
    , builder_(module_) {
}

std::expected<std::shared_ptr<ir::Module>, TypeError>
ASTToIR::transform_program(const std::vector<parser::ast::expression>& expressions) {
    // Create main function
    auto main_func = module_->create_function("main");
    current_function_ = main_func;
    
    auto entry = main_func->create_block("entry");
    builder_.set_current_function(main_func);
    builder_.set_insert_point(entry);
    
    // Transform each expression
    for (const auto& expr : expressions) {
        auto result = transform_expression(expr);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    
    // Add return if not already terminated
    if (!entry->is_terminated()) {
        builder_.build_return();
    }
    
    return module_;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_expression(const parser::ast::expression& expr) {
    // Use meld::compat::visit with a generic lambda — MSVC forbids template
    // members in local classes (C2892) but generic lambdas are fine.
    using R = std::expected<std::shared_ptr<ir::Value>, TypeError>;
    return meld::compat::visit<R>([&](auto const& node) -> R {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return transform_identifier(node);
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal> ||
                          std::is_same_v<T, parser::ast::float_literal> ||
                          std::is_same_v<T, parser::ast::string_literal> ||
                          std::is_same_v<T, parser::ast::boolean_literal>) {
            return transform_literal(expr);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            return transform_binary_operation(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            return transform_unary_operation(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            return transform_val_declaration(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            return transform_var_declaration(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            return transform_function_definition(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            return transform_function_call(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            return transform_struct_definition(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            return transform_class_definition(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::enum_definition>>) {
            return transform_enum_definition(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::newtype_declaration>>) {
            return transform_newtype_declaration(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::initialization_block>>) {
            return transform_initialization_block(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
            return transform_lambda_expression(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::extension_block>>) {
            return transform_extension_block(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::pipeline_expression>>) {
            return transform_pipeline_expression(node.get());
        }
        // Task 7.1 / 7.2 (implicit-effect-calls): handle both perform_expression
        // and implicit_effect_call, lowering them through the shared emit_effect_suspend helper.
        // Requirements 3.1, 3.2, 3.3.
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::perform_expression>>) {
            return transform_perform_expression(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>>) {
            return transform_implicit_effect_call(node.get());
        }
        else {
            return std::unexpected(TypeError("Unsupported expression type in AST to IR transformation"));
        }
    }, expr);
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_literal(const parser::ast::expression& expr) {
    using R = std::expected<std::shared_ptr<ir::Value>, TypeError>;
    return meld::compat::visit<R>([&](auto const& node) -> R {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return builder_.build_const_int(node.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            // For now, treat floats as ints (simplified)
            return builder_.build_const_int(static_cast<int64_t>(node.value));
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return builder_.build_const_string(node.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return builder_.build_const_bool(node.value);
        }
        else {
            return std::unexpected(TypeError("Unknown literal type"));
        }
    }, expr);
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_identifier(const parser::ast::identifier& id) {
    auto it = symbol_table_.find(id.name);
    if (it == symbol_table_.end()) {
        return std::unexpected(TypeError("Undefined variable '" + id.name + "'"));
    }
    
    // Load the value from the address
    return builder_.build_load(it->second);
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_binary_operation(const parser::ast::binary_operation& op) {
    auto left = transform_expression(op.left);
    if (!left) return std::unexpected(left.error());
    
    auto right = transform_expression(op.right);
    if (!right) return std::unexpected(right.error());
    
    // Arithmetic operators
    if (op.op == "+") {
        return builder_.build_add(*left, *right);
    } else if (op.op == "-") {
        return builder_.build_sub(*left, *right);
    } else if (op.op == "*") {
        return builder_.build_mul(*left, *right);
    } else if (op.op == "/") {
        return builder_.build_div(*left, *right);
    }
    
    // Comparison operators
    else if (op.op == "==") {
        return builder_.build_eq(*left, *right);
    } else if (op.op == "!=") {
        return builder_.build_ne(*left, *right);
    } else if (op.op == "<") {
        return builder_.build_lt(*left, *right);
    } else if (op.op == "<=") {
        return builder_.build_le(*left, *right);
    } else if (op.op == ">") {
        return builder_.build_gt(*left, *right);
    } else if (op.op == ">=") {
        return builder_.build_ge(*left, *right);
    }
    
    // Logical operators
    else if (op.op == "&&") {
        return builder_.build_and(*left, *right);
    } else if (op.op == "||") {
        return builder_.build_or(*left, *right);
    }
    
    return std::unexpected(TypeError("Unknown binary operator '" + op.op + "'"));
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_unary_operation(const parser::ast::unary_operation& op) {
    auto operand = transform_expression(op.operand);
    if (!operand) return std::unexpected(operand.error());
    
    if (op.op == "-") {
        return builder_.build_neg(*operand);
    } else if (op.op == "!") {
        return builder_.build_not(*operand);
    }
    
    return std::unexpected(TypeError("Unknown unary operator '" + op.op + "'"));
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_val_declaration(const parser::ast::val_declaration& decl) {
    // Evaluate the value
    auto value = transform_expression(decl.value);
    if (!value) return std::unexpected(value.error());
    
    // Allocate storage
    auto storage = builder_.build_alloca((*value)->type);
    
    // Store the value
    builder_.build_store(*value, storage);
    
    // Add to symbol table
    symbol_table_[decl.name.name] = storage;

    // Emit DILocalVariable (Req 12B)
    if (debug_info_enabled_ && current_function_ && current_function_->debug_info) {
        ir::DILocalVariable dv;
        dv.name = decl.name.name;
        dv.type = make_debug_type_from_ir((*value)->type);
        dv.location = {source_file_, 0, 0};
        dv.is_parameter = false;
        current_function_->debug_info->variables.push_back(std::move(dv));
    }
    
    return *value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_var_declaration(const parser::ast::var_declaration& decl) {
    // Same as val for IR purposes (mutability is a type system concern)
    auto value = transform_expression(decl.value);
    if (!value) return std::unexpected(value.error());
    
    auto storage = builder_.build_alloca((*value)->type);
    builder_.build_store(*value, storage);
    symbol_table_[decl.name.name] = storage;

    // Emit DILocalVariable (Req 12B)
    if (debug_info_enabled_ && current_function_ && current_function_->debug_info) {
        ir::DILocalVariable dv;
        dv.name = decl.name.name;
        dv.type = make_debug_type_from_ir((*value)->type);
        dv.location = {source_file_, 0, 0};
        dv.is_parameter = false;
        current_function_->debug_info->variables.push_back(std::move(dv));
    }
    
    return *value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_function_definition(const parser::ast::function_definition& def) {
    // Handle curried function syntax: fn foo(a: int)(b: int) -> result
    if (def.is_curried) {
        // For curried functions, create a single function with all parameters flattened
        // and then apply curry() to it
        
        // Flatten all parameter groups into a single parameter list
        std::vector<parser::ast::function_parameter> all_params = def.parameters;
        for (const auto& group : def.curried_parameter_groups) {
            all_params.insert(all_params.end(), group.begin(), group.end());
        }
        
        // Create the underlying function with all parameters
        auto func = module_->create_function(def.name.name + "_uncurried");
        
        // Add all parameters
        for (const auto& param : all_params) {
            auto param_type = ast_type_to_ir_type(param.type);
            auto param_value = std::make_shared<ir::Value>(param.name.name, param_type);
            func->parameters.push_back(param_value);
        }
        
        // Create entry block and transform body
        auto entry = func->create_block("entry");
        
        // Save current state
        auto prev_function = current_function_;
        auto prev_symbol_table = symbol_table_;
        
        // Set up for function body
        current_function_ = func;
        builder_.set_current_function(func);
        builder_.set_insert_point(entry);
        symbol_table_.clear();
        
        // Add parameters to symbol table
        for (const auto& param : func->parameters) {
            auto storage = builder_.build_alloca(param->type);
            builder_.build_store(param, storage);
            symbol_table_[param->name] = storage;
        }
        
        // Transform function body
        auto body_result = transform_block_expression(def.body.get());
        if (!body_result) {
            current_function_ = prev_function;
            symbol_table_ = prev_symbol_table;
            if (prev_function) builder_.set_current_function(prev_function);
            return std::unexpected(body_result.error());
        }
        
        // Add return if not already terminated
        if (!entry->is_terminated()) {
            builder_.build_return(*body_result);
        }
        
        // Restore state
        current_function_ = prev_function;
        symbol_table_ = prev_symbol_table;
        if (prev_function) {
            builder_.set_current_function(prev_function);
        }
        
        // Create the curried version by calling curry() on the uncurried function
        // This would need to be done at runtime or through a compiler intrinsic
        // For now, we'll create a regular function value and handle currying at runtime
        auto func_value = std::make_shared<ir::Value>(def.name.name, ir::ValueType::Function);
        symbol_table_[def.name.name] = func_value;
        
        return func_value;
    } else {
        // Regular function definition
        auto func = module_->create_function(def.name.name);
        
        // Add parameters
        for (const auto& param : def.parameters) {
            auto param_type = ast_type_to_ir_type(param.type);
            auto param_value = std::make_shared<ir::Value>(param.name.name, param_type);
            func->parameters.push_back(param_value);
        }
        
        // Attach DISubprogram debug info (Req 12B)
        if (debug_info_enabled_) {
            ir::DISubprogram di;
            di.name = def.name.name;
            di.linkage_name = def.name.name;
            di.location = {source_file_, 0, 0}; // line/col from AST if available
            di.return_type = def.has_return_type
                ? make_debug_type(def.return_type)
                : ir::DIType{"void", "DW_ATE_address", 0};
            di.is_definition = true;
            // Register parameters as debug variables
            for (size_t i = 0; i < def.parameters.size(); ++i) {
                ir::DILocalVariable dv;
                dv.name = def.parameters[i].name.name;
                dv.type = make_debug_type(def.parameters[i].type);
                dv.location = {source_file_, 0, 0};
                dv.is_parameter = true;
                dv.arg_index = static_cast<int>(i);
                di.variables.push_back(std::move(dv));
            }
            func->debug_info = std::move(di);
        }

        // Create entry block
        auto entry = func->create_block("entry");
        
        // Save current state
        auto prev_function = current_function_;
        auto prev_symbol_table = symbol_table_;
        
        // Set up for function body
        current_function_ = func;
        builder_.set_current_function(func);
        builder_.set_insert_point(entry);
        symbol_table_.clear();
        
        // Add parameters to symbol table
        for (const auto& param : func->parameters) {
            auto storage = builder_.build_alloca(param->type);
            builder_.build_store(param, storage);
            symbol_table_[param->name] = storage;
        }
        
        // Transform function body
        auto body_result = transform_block_expression(def.body.get());
        if (!body_result) {
            current_function_ = prev_function;
            symbol_table_ = prev_symbol_table;
            if (prev_function) builder_.set_current_function(prev_function);
            return std::unexpected(body_result.error());
        }
        
        // Add return if not already terminated
        if (!entry->is_terminated()) {
            builder_.build_return(*body_result);
        }
        
        // Restore state
        current_function_ = prev_function;
        symbol_table_ = prev_symbol_table;
        if (prev_function) {
            builder_.set_current_function(prev_function);
        }
        
        // Return function as a value
        auto func_value = std::make_shared<ir::Value>(def.name.name, ir::ValueType::Function);
        symbol_table_[def.name.name] = func_value;
        
        return func_value;
    }
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_function_call(const parser::ast::function_call& call) {
    // Look up function
    auto it = symbol_table_.find(call.function_name.name);
    if (it == symbol_table_.end()) {
        return std::unexpected(TypeError("Undefined function '" + call.function_name.name + "'"));
    }
    
    // Transform arguments
    std::vector<std::shared_ptr<ir::Value>> args;
    for (const auto& arg : call.arguments) {
        auto arg_value = transform_expression(arg);
        if (!arg_value) return std::unexpected(arg_value.error());
        args.push_back(*arg_value);
    }
    
    // Generate call
    return builder_.build_call(it->second, args);
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_struct_definition(const parser::ast::struct_definition& def) {
    // Register the struct type in the module
    // For now, just return a placeholder value
    auto struct_value = std::make_shared<ir::Value>(def.name.name, ir::ValueType::Struct);
    symbol_table_[def.name.name] = struct_value;
    return struct_value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_class_definition(const parser::ast::class_definition& def) {
    // Register the class type in the module
    // For now, just return a placeholder value
    auto class_value = std::make_shared<ir::Value>(def.name.name, ir::ValueType::Struct);
    symbol_table_[def.name.name] = class_value;
    return class_value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_enum_definition(const parser::ast::enum_definition& def) {
    // Register the enum type in the module
    // For now, just return a placeholder value
    auto enum_value = std::make_shared<ir::Value>(def.name.name, ir::ValueType::Struct);
    symbol_table_[def.name.name] = enum_value;
    return enum_value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_newtype_declaration(const parser::ast::newtype_declaration& decl) {
    // Newtype declarations create zero-cost type wrappers
    // At IR level, they are transparent to the wrapped type for optimization
    // but maintain distinct type identity for safety
    
    // Register the newtype in the symbol table
    // For zero-cost abstraction, we use the same IR representation as the wrapped type
    auto newtype_value = std::make_shared<ir::Value>(decl.wrapper_name.name, ir::ValueType::Struct);
    symbol_table_[decl.wrapper_name.name] = newtype_value;
    
    // The newtype is transparent at runtime but distinct at compile time
    return newtype_value;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_initialization_block(const parser::ast::initialization_block& init) {
    // Initialization blocks are transformed into constructor calls
    // 1. Allocate memory for the object
    // 2. Initialize each field with the provided values
    // 3. Return the initialized object
    
    // Look up the type being initialized
    auto type_iter = symbol_table_.find(init.type_name.name);
    if (type_iter == symbol_table_.end()) {
        return std::unexpected(TypeError(
            "Unknown type '" + init.type_name.name + "' in initialization block"
        ));
    }
    
    // Allocate memory for the object
    auto object_ptr = builder_.build_alloca(ir::ValueType::Struct);
    
    // Initialize each field
    for (const auto& param : init.parameters) {
        // Transform the parameter value (this handles nested initialization blocks)
        auto value_result = transform_expression(param.value);
        if (!value_result) {
            return std::unexpected(value_result.error());
        }
        
        // Create a SetField instruction to set the field
        auto set_field_inst = std::make_shared<ir::Instruction>(ir::Opcode::SetField);
        set_field_inst->operands.push_back(object_ptr);
        set_field_inst->operands.push_back(*value_result);
        set_field_inst->metadata.push_back(param.name.name);  // Field name
        
        // Insert the instruction into the current basic block
        if (current_function_ && !current_function_->basic_blocks.empty()) {
            current_function_->basic_blocks.back()->add_instruction(set_field_inst);
        }
    }
    
    // Load the initialized object
    auto result = builder_.build_load(object_ptr);
    return result;
}

ir::ValueType ASTToIR::ast_type_to_ir_type(const parser::ast::type_annotation& annotation) {
    if (annotation.type_name.name == "Int") {
        return ir::ValueType::Int;
    } else if (annotation.type_name.name == "Bool") {
        return ir::ValueType::Bool;
    } else if (annotation.type_name.name == "String") {
        return ir::ValueType::String;
    } else if (annotation.type_name.name == "Float" || annotation.type_name.name == "Double") {
        return ir::ValueType::Float;
    }
    return ir::ValueType::Void;
}

std::string ASTToIR::generate_label(const std::string& prefix) {
    return prefix + std::to_string(label_counter_++);
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_lambda_expression(const parser::ast::lambda_expression& lambda) {
    // Create a new function for the lambda
    std::string lambda_name = "lambda_" + std::to_string(label_counter_++);
    auto lambda_func = module_->create_function(lambda_name);
    
    // Save current function context
    auto saved_function = current_function_;
    auto saved_symbol_table = symbol_table_;
    
    // Set up lambda function context
    current_function_ = lambda_func;
    builder_.set_current_function(lambda_func);
    
    // Create entry block for lambda
    auto entry = lambda_func->create_block("entry");
    builder_.set_insert_point(entry);
    
    // Create parameters
    std::vector<std::shared_ptr<ir::Value>> param_values;
    for (const auto& param : lambda.parameters) {
        auto param_value = std::make_shared<ir::Value>(
            ir::ValueType::Pointer,
            "param_" + param.name.name
        );
        param_values.push_back(param_value);
        lambda_func->parameters.push_back(param_value);
        
        // Bind parameter in symbol table
        symbol_table_[param.name.name] = param_value;
    }
    
    // Transform lambda body
    auto body_result = transform_expression(lambda.body.get());
    if (!body_result) {
        // Restore context
        current_function_ = saved_function;
        symbol_table_ = saved_symbol_table;
        return std::unexpected(body_result.error());
    }
    
    // Add return statement
    builder_.build_return(*body_result);
    
    // Restore context
    current_function_ = saved_function;
    builder_.set_current_function(saved_function);
    symbol_table_ = saved_symbol_table;
    
    // Create function pointer value
    auto func_ptr = std::make_shared<ir::Value>(
        ir::ValueType::Function,
        lambda_name
    );
    
    return func_ptr;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_extension_block(const parser::ast::extension_block& ext) {
    // Extension blocks register methods but don't produce runtime values
    // The actual method implementations are generated when the extension methods are called
    
    // For each extension method, we could generate IR functions and register them
    // For now, we'll just return a unit value since extension blocks are primarily
    // about registering methods in the extension registry
    
    for (const auto& method : ext.methods) {
        // Create a function for each extension method
        std::string method_name = ext.target_type.type_name.name + "_" + method.name.name;
        auto method_func = module_->create_function(method_name);
        
        // Save current function context
        auto saved_function = current_function_;
        auto saved_symbol_table = symbol_table_;
        
        // Set up method function context
        current_function_ = method_func;
        builder_.set_current_function(method_func);
        
        // Create entry block
        auto entry = method_func->create_block("entry");
        builder_.set_insert_point(entry);
        
        // Create 'this' parameter (first parameter is always the receiver)
        auto this_param = std::make_shared<ir::Value>(
            ir::ValueType::Pointer,
            "this"
        );
        method_func->parameters.push_back(this_param);
        symbol_table_["this"] = this_param;
        
        // Create other parameters
        for (const auto& param : method.parameters) {
            auto param_value = std::make_shared<ir::Value>(
                ir::ValueType::Pointer,
                "param_" + param.name.name
            );
            method_func->parameters.push_back(param_value);
            symbol_table_[param.name.name] = param_value;
        }
        
        // Transform method body (block_expression contains statements)
        const auto& body = method.body.get();
        std::shared_ptr<ir::Value> body_result_val;
        for (const auto& stmt : body.statements) {
            auto stmt_result = transform_expression(stmt.get());
            if (!stmt_result) {
                current_function_ = saved_function;
                symbol_table_ = saved_symbol_table;
                return std::unexpected(stmt_result.error());
            }
            body_result_val = *stmt_result;
        }
        if (!body_result_val) {
            body_result_val = std::make_shared<ir::Value>(ir::ValueType::Unit, "unit");
        }
        
        // Add return statement
        builder_.build_return(body_result_val);
        
        // Restore context
        current_function_ = saved_function;
        builder_.set_current_function(saved_function);
        symbol_table_ = saved_symbol_table;
    }
    
    // Return unit value
    return std::make_shared<ir::Value>(ir::ValueType::Unit, "unit");
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_pipeline_expression(const parser::ast::pipeline_expression& pipe) {
    // Transform the value being piped
    auto value_result = transform_expression(pipe.value.get());
    if (!value_result) return std::unexpected(value_result.error());
    
    // Transform the function
    auto func_result = transform_expression(pipe.function.get());
    if (!func_result) return std::unexpected(func_result.error());
    
    // Create a function call with the value as the first argument
    // Pipeline: value |> function becomes function(value)
    std::vector<std::shared_ptr<ir::Value>> args = { *value_result };
    
    // Generate the function call
    auto call_result = builder_.build_call(*func_result, args);
    
    return call_result;
}

std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_block_expression(const parser::ast::block_expression& block) {
    // Evaluate each statement sequentially; the last expression's value is the block result
    std::shared_ptr<ir::Value> last_value = nullptr;
    
    for (const auto& stmt : block.statements) {
        auto result = transform_expression(stmt.get());
        if (!result) return std::unexpected(result.error());
        last_value = *result;
    }
    
    // If the block was empty, return a void/unit value
    if (!last_value) {
        last_value = std::make_shared<ir::Value>(ir::ValueType::Unit, "unit");
    }
    
    return last_value;
}

// ─── Effect suspend helper — Task 7.1 (implicit-effect-calls) ───────
// Shared lowering for both perform_expression and implicit_effect_call.
// Requirement 3.1: identical primitive_suspend + continuation machinery.
// Requirement 3.3: the returned Value carries the resumed value so that
//   val x = Effect.op(args) correctly binds x.
std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::emit_effect_suspend(
        const std::string& effect_name,
        const std::string& operation_name,
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& arguments) {
    // ── Effect Firewall check (Req 11.1, 11.2, 11.5) ───────────────
    // Emit a runtime check BEFORE the perform call so that unauthorized
    // effects are caught at the same point in Tiers 2 & 3 as in Tier 1.
    if (effect_firewall_enabled_) {
        emit_firewall_check(effect_name, module_name_);
    }

    // 1. Build constant strings for effect name and operation name so the
    //    runtime can locate the correct handler at execution time.
    auto effect_name_val  = builder_.build_const_string(effect_name);
    auto op_name_val      = builder_.build_const_string(operation_name);

    // 2. Evaluate each argument expression left-to-right.
    std::vector<std::shared_ptr<ir::Value>> arg_values;
    arg_values.reserve(arguments.size());
    for (const auto& arg : arguments) {
        auto arg_result = transform_expression(arg.get());
        if (!arg_result) return std::unexpected(arg_result.error());
        arg_values.push_back(*arg_result);
    }

    // 3. Emit a Call instruction targeting the runtime perform_effect entry
    //    point.  At link time this resolves to
    //    EffectRuntime::instance().perform_effect(effect_name, operation_name, args)
    //    which internally calls kernel::primitive_suspend with the appropriate
    //    delimiter and handler callback.
    //
    //    The call returns the value that the handler resumes with, so the
    //    ir::Value we produce here is the "resumed value" — exactly what a
    //    val binding needs (Requirement 3.3).
    auto perform_fn = std::make_shared<ir::Value>("__meld_perform_effect", ir::ValueType::Function);

    // Pack operands: perform_fn, effect_name, operation_name, arg0, arg1, …
    std::vector<std::shared_ptr<ir::Value>> call_args;
    call_args.push_back(effect_name_val);
    call_args.push_back(op_name_val);
    call_args.insert(call_args.end(), arg_values.begin(), arg_values.end());

    return builder_.build_call(perform_fn, call_args);
}

// ─── perform_expression handler — Task 7.1 ─────────────────────────
// Delegates entirely to emit_effect_suspend so that the generated IR is
// identical regardless of whether the source used explicit perform { … }
// or the new implicit syntax.  Requirement 3.1, 3.2.
std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_perform_expression(const parser::ast::perform_expression& perform) {
    return emit_effect_suspend(perform.effect_name.name,
                               perform.operation_name.name,
                               perform.arguments);
}

// ─── implicit_effect_call handler — Task 7.2 ────────────────────────
// Produces the exact same IR as perform_expression by calling the shared
// emit_effect_suspend helper.  Requirements 3.1, 3.2, 3.3.
std::expected<std::shared_ptr<ir::Value>, TypeError>
ASTToIR::transform_implicit_effect_call(const parser::ast::implicit_effect_call& call) {
    return emit_effect_suspend(call.effect_name.name,
                               call.operation_name.name,
                               call.arguments);
}

// ─── Effect Firewall — compiler integration (Req 11.1, 11.2, 11.5) ──

void ASTToIR::set_module_name(const std::string& module_name) {
    module_name_ = module_name;
}

void ASTToIR::set_effect_firewall_enabled(bool enabled) {
    effect_firewall_enabled_ = enabled;
}

void ASTToIR::emit_firewall_check(const std::string& effect_name,
                                   const std::string& module_name) {
    // Emit a call to the C-linkage runtime check function:
    //   int __meld_effect_firewall_check(
    //       const char* effect_name, const char* module_name,
    //       const char* file, size_t line, size_t column);
    //
    // This resolves at link time to EffectFirewall::runtime_check(),
    // which delegates to the process-wide singleton — ensuring identical
    // enforcement in ORC JIT (Tier 2) and AOT (Tier 3) as in the
    // AST interpreter (Tier 1).  Requirement 11.5.

    auto firewall_fn = std::make_shared<ir::Value>(
        "__meld_effect_firewall_check", ir::ValueType::Function);

    auto effect_name_val = builder_.build_const_string(effect_name);
    auto module_name_val = builder_.build_const_string(module_name);
    auto file_val        = builder_.build_const_string(source_file_);
    auto line_val        = builder_.build_const_int(0);  // TODO: extract from AST position
    auto col_val         = builder_.build_const_int(0);

    std::vector<std::shared_ptr<ir::Value>> check_args = {
        effect_name_val, module_name_val, file_val, line_val, col_val
    };

    builder_.build_call(firewall_fn, check_args);
}

// ─── Debug info helpers (Req 12B) ───────────────────────────────────

void ASTToIR::set_source_file(const std::string& file, const std::string& directory) {
    source_file_ = file;
    source_directory_ = directory;
}

void ASTToIR::set_debug_info_enabled(bool enabled) {
    debug_info_enabled_ = enabled;
    module_->emit_debug_info = enabled;
    if (enabled && !source_file_.empty()) {
        ir::DICompileUnit cu;
        cu.file = source_file_;
        cu.directory = source_directory_;
        cu.producer = "meld";
        cu.is_optimized = false;
        module_->compile_unit = cu;

        // Register kernel debug types
        module_->debug_types.push_back({"kernel::Integer", "DW_ATE_signed", 64});
        module_->debug_types.push_back({"kernel::Float",   "DW_ATE_float",  64});
        module_->debug_types.push_back({"kernel::Boolean", "DW_ATE_boolean", 8});
        module_->debug_types.push_back({"kernel::String",  "DW_ATE_UTF",    0});
        module_->debug_types.push_back({"kernel::Vec",     "DW_ATE_address", 0});
        module_->debug_types.push_back({"kernel::Function","DW_ATE_address", 0});
        module_->debug_types.push_back({"kernel::Optional","DW_ATE_address", 0});
        module_->debug_types.push_back({"kernel::Cons",    "DW_ATE_address", 0});
    }
}

ir::DIType ASTToIR::make_debug_type(const parser::ast::type_annotation& annotation) {
    const auto& name = annotation.type_name.name;
    if (name == "Int")    return {"kernel::Integer", "DW_ATE_signed",  64};
    if (name == "Float" || name == "Double")
                          return {"kernel::Float",   "DW_ATE_float",   64};
    if (name == "Bool")   return {"kernel::Boolean", "DW_ATE_boolean", 8};
    if (name == "String") return {"kernel::String",  "DW_ATE_UTF",     0};
    // Default: pointer-sized opaque type
    return {name, "DW_ATE_address", 0};
}

ir::DIType ASTToIR::make_debug_type_from_ir(ir::ValueType type) {
    switch (type) {
        case ir::ValueType::Int:      return {"kernel::Integer", "DW_ATE_signed",  64};
        case ir::ValueType::Float:    return {"kernel::Float",   "DW_ATE_float",   64};
        case ir::ValueType::Bool:     return {"kernel::Boolean", "DW_ATE_boolean", 8};
        case ir::ValueType::String:   return {"kernel::String",  "DW_ATE_UTF",     0};
        case ir::ValueType::Function: return {"kernel::Function","DW_ATE_address", 0};
        case ir::ValueType::Struct:   return {"kernel::Struct",  "DW_ATE_address", 0};
        default:                      return {"void",            "DW_ATE_address", 0};
    }
}

void ASTToIR::attach_debug_location(std::shared_ptr<ir::Instruction> inst,
                                     size_t line, size_t column) {
    if (debug_info_enabled_ && inst) {
        inst->debug_loc = ir::DILocation{source_file_, line, column};
    }
}

} // namespace meld::compiler
