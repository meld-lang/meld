#pragma once

#include "meld/api/semantic_graph.hpp"
#include "meld/parser/ast.hpp"
#include <stack>
#include <unordered_map>

namespace meld::api {

/**
 * Scope information for tracking variable declarations
 */
struct ScopeInfo {
    NodeId scope_node_id;
    std::unordered_map<std::string, NodeId> symbol_definitions;
    ScopeInfo* parent;
    
    ScopeInfo(NodeId node_id, ScopeInfo* p = nullptr) 
        : scope_node_id(node_id), parent(p) {}
    
    // Look up a symbol in this scope or parent scopes
    NodeId lookupSymbol(const std::string& symbol) const {
        auto it = symbol_definitions.find(symbol);
        if (it != symbol_definitions.end()) {
            return it->second;
        }
        
        if (parent) {
            return parent->lookupSymbol(symbol);
        }
        
        return 0; // Not found
    }
    
    // Define a symbol in this scope
    void defineSymbol(const std::string& symbol, NodeId node_id) {
        symbol_definitions[symbol] = node_id;
    }
};

/**
 * ASG Builder constructs the Abstract Syntax Graph during semantic analysis
 */
class ASGBuilder {
public:
    ASGBuilder(SemanticGraph& graph, const std::string& file_path)
        : graph_(graph), file_path_(file_path), current_scope_(nullptr) {}
    
    // Build ASG from AST
    void buildFromAST(const parser::ast::expression& root_ast);
    
private:
    friend class ASTVisitor;
    SemanticGraph& graph_;
    std::string file_path_;
    ScopeInfo* current_scope_;
    std::stack<ScopeInfo*> scope_stack_;
    
    // Last statement node for control flow edges
    NodeId last_statement_node_;
    
    // Current function context for tracking returns
    NodeId current_function_node_;
    
    // Visitor methods for different AST node types
    NodeId visitExpression(const parser::ast::expression& expr);
    NodeId visitIdentifier(const parser::ast::identifier& id);
    NodeId visitIntegerLiteral(const parser::ast::integer_literal& lit);
    NodeId visitFloatLiteral(const parser::ast::float_literal& lit);
    NodeId visitStringLiteral(const parser::ast::string_literal& lit);
    NodeId visitBooleanLiteral(const parser::ast::boolean_literal& lit);
    NodeId visitFunctionCall(const parser::ast::function_call& call);
    NodeId visitValDeclaration(const parser::ast::val_declaration& decl);
    NodeId visitVarDeclaration(const parser::ast::var_declaration& decl);
    NodeId visitBinaryOperation(const parser::ast::binary_operation& op);
    NodeId visitUnaryOperation(const parser::ast::unary_operation& op);
    NodeId visitFunctionDefinition(const parser::ast::function_definition& func);
    NodeId visitBlockExpression(const parser::ast::block_expression& block);
    NodeId visitReturnStatement(const parser::ast::return_statement& ret);
    NodeId visitTupleDestructuring(const parser::ast::tuple_destructuring& dest);
    NodeId visitLambdaExpression(const parser::ast::lambda_expression& lambda);
    NodeId visitPipelineExpression(const parser::ast::pipeline_expression& pipeline);
    NodeId visitMatchExpression(const parser::ast::match_expression& match);
    NodeId visitQueryExpression(const parser::ast::query_expression& query);
    NodeId visitNamespaceDeclaration(const parser::ast::namespace_declaration& ns);
    NodeId visitImportDeclaration(const parser::ast::import_declaration& import);
    NodeId visitStructDefinition(const parser::ast::struct_definition& struct_def);
    NodeId visitClassDefinition(const parser::ast::class_definition& class_def);
    NodeId visitEffectDefinition(const parser::ast::effect_definition& effect);
    NodeId visitPerformExpression(const parser::ast::perform_expression& perform);
    NodeId visitImplicitEffectCall(const parser::ast::implicit_effect_call& call);
    NodeId visitHandleExpression(const parser::ast::handle_expression& handle);
    
    // Scope management
    void enterScope(NodeId scope_node_id);
    void exitScope();
    
    // Data flow edge construction
    void createDataFlowEdges(const std::string& symbol, NodeId usage_node, 
                            DataFlowEdge::FlowType flow_type);
    
    // Control flow edge construction
    void createControlFlowEdge(NodeId from, NodeId to, 
                              ControlFlowEdge::FlowCondition condition);
    
    // Scope edge construction
    void createScopeEdge(NodeId declaration_node, const std::string& symbol);
    
    // Type edge construction
    void createTypeEdge(NodeId value_node, const std::string& type_name);
    
    // Call edge construction
    void createCallEdge(NodeId call_node, const std::string& function_name);
    
    // Helper methods
    SourceLocation extractLocation(const parser::ast::expression& expr) const;
    std::string extractTypeName(const parser::ast::type_annotation& type) const;
    
    // Track variable definitions and uses
    void trackVariableDefinition(const std::string& symbol, NodeId node_id);
    void trackVariableUse(const std::string& symbol, NodeId node_id);
    void trackVariableModification(const std::string& symbol, NodeId node_id);
};

} // namespace meld::api