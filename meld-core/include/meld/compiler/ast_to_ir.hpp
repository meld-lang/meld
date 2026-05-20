#pragma once

#include "meld/parser/ast.hpp"
#include "meld/effects/effect_firewall.hpp"
#include "meld/compiler/ir.hpp"
#include "meld/compiler/type_checker.hpp"
#include <memory>
#include <map>
#include <expected>

namespace meld::compiler {

// AST to IR transformer
class ASTToIR {
public:
    explicit ASTToIR(std::shared_ptr<ir::Module> module);
    
    // Transform a program (list of expressions) to IR
    std::expected<std::shared_ptr<ir::Module>, TypeError>
    transform_program(const std::vector<parser::ast::expression>& expressions);
    
    // Transform a single expression to IR
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_expression(const parser::ast::expression& expr);
    
private:
    // Transform specific AST nodes
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_literal(const parser::ast::expression& expr);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_identifier(const parser::ast::identifier& id);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_binary_operation(const parser::ast::binary_operation& op);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_unary_operation(const parser::ast::unary_operation& op);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_val_declaration(const parser::ast::val_declaration& decl);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_var_declaration(const parser::ast::var_declaration& decl);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_function_definition(const parser::ast::function_definition& def);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_function_call(const parser::ast::function_call& call);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_struct_definition(const parser::ast::struct_definition& def);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_class_definition(const parser::ast::class_definition& def);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_enum_definition(const parser::ast::enum_definition& def);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_newtype_declaration(const parser::ast::newtype_declaration& decl);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_initialization_block(const parser::ast::initialization_block& init);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_lambda_expression(const parser::ast::lambda_expression& lambda);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_extension_block(const parser::ast::extension_block& ext);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_pipeline_expression(const parser::ast::pipeline_expression& pipe);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_block_expression(const parser::ast::block_expression& block);
    
    // Effect expression transforms — Task 7.1, 7.2 (implicit-effect-calls)
    // Requirements 3.1, 3.2, 3.3
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_perform_expression(const parser::ast::perform_expression& perform);
    
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    transform_implicit_effect_call(const parser::ast::implicit_effect_call& call);
    
    // Shared helper: both perform_expression and implicit_effect_call lower to the
    // same IR sequence — evaluate arguments, emit a Call to the runtime's
    // perform_effect (which internally invokes primitive_suspend + continuation).
    // Requirement 3.1: identical primitive_suspend + continuation machinery.
    // Requirement 3.3: the returned ir::Value carries the resumed value for
    //                   value bindings (val x = Effect.op(args)).
    std::expected<std::shared_ptr<ir::Value>, TypeError>
    emit_effect_suspend(const std::string& effect_name,
                        const std::string& operation_name,
                        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& arguments);

    // ─── Effect Firewall integration (Req 11.1, 11.2, 11.5) ────────
    //
    // Emit a call to the EffectFirewall runtime check before each perform().
    // The same check function is used by both ORC JIT (Tier 2) and AOT (Tier 3).
    void emit_firewall_check(const std::string& effect_name,
                             const std::string& module_name);

public:
    // Set the module name used for firewall checks (from meld.toml or file path).
    void set_module_name(const std::string& module_name);

    // Enable/disable firewall check emission.
    void set_effect_firewall_enabled(bool enabled);

    // Debug info helpers (Req 12B)
    void set_source_file(const std::string& file, const std::string& directory = "");
    void set_debug_info_enabled(bool enabled);

private:
    
    // Helper methods
    ir::ValueType ast_type_to_ir_type(const parser::ast::type_annotation& annotation);
    std::string generate_label(const std::string& prefix);

    ir::DIType make_debug_type(const parser::ast::type_annotation& annotation);
    ir::DIType make_debug_type_from_ir(ir::ValueType type);
    void attach_debug_location(std::shared_ptr<ir::Instruction> inst,
                               size_t line, size_t column);
    
    // State
    std::shared_ptr<ir::Module> module_;
    ir::IRBuilder builder_;
    std::shared_ptr<ir::Function> current_function_;
    std::map<std::string, std::shared_ptr<ir::Value>> symbol_table_;
    int label_counter_ = 0;

    // Debug info state (Req 12B)
    bool debug_info_enabled_ = false;
    std::string source_file_;
    std::string source_directory_;

    // Effect Firewall state (Req 11.1, 11.2, 11.5)
    bool effect_firewall_enabled_ = false;
    std::string module_name_;
};

} // namespace meld::compiler
