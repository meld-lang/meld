#include "meld/api/asg_builder.hpp"
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>

namespace meld::api {

// Visitor class for AST traversal
class ASTVisitor : public boost::static_visitor<NodeId> {
public:
    ASTVisitor(ASGBuilder& builder) : builder_(builder) {}
    
    template<typename T>
    NodeId operator()(const boost::recursive_wrapper<T>& wrapped) const {
        return (*this)(wrapped.get());
    }
    
    NodeId operator()(const parser::ast::identifier& id) const {
        return builder_.visitIdentifier(id);
    }
    
    NodeId operator()(const parser::ast::integer_literal& lit) const {
        return builder_.visitIntegerLiteral(lit);
    }
    
    NodeId operator()(const parser::ast::float_literal& lit) const {
        return builder_.visitFloatLiteral(lit);
    }
    
    NodeId operator()(const parser::ast::string_literal& lit) const {
        return builder_.visitStringLiteral(lit);
    }
    
    NodeId operator()(const parser::ast::boolean_literal& lit) const {
        return builder_.visitBooleanLiteral(lit);
    }
    
    NodeId operator()(const parser::ast::function_call& call) const {
        return builder_.visitFunctionCall(call);
    }
    
    NodeId operator()(const parser::ast::val_declaration& decl) const {
        return builder_.visitValDeclaration(decl);
    }
    
    NodeId operator()(const parser::ast::var_declaration& decl) const {
        return builder_.visitVarDeclaration(decl);
    }
    
    NodeId operator()(const parser::ast::binary_operation& op) const {
        return builder_.visitBinaryOperation(op);
    }
    
    NodeId operator()(const parser::ast::unary_operation& op) const {
        return builder_.visitUnaryOperation(op);
    }
    
    NodeId operator()(const parser::ast::function_definition& func) const {
        return builder_.visitFunctionDefinition(func);
    }
    
    // Add more operator() overloads for other AST node types as needed
    template<typename T>
    NodeId operator()(const T& node) const {
        // Default case - create a generic expression node
        SourceLocation loc = builder_.extractLocation(parser::ast::expression(node));
        return builder_.graph_.addNode(NodeType::EXPRESSION, parser::ast::expression(node), loc);
    }
    
private:
    ASGBuilder& builder_;
};

void ASGBuilder::buildFromAST(const parser::ast::expression& root_ast) {
    // Create root scope
    SourceLocation root_location(file_path_, 1, 1, 0);
    NodeId root_scope_node = graph_.addNode(NodeType::SCOPE_BOUNDARY, root_ast, root_location);
    
    enterScope(root_scope_node);
    
    // Visit the root AST node
    visitExpression(root_ast);
    
    exitScope();
}

NodeId ASGBuilder::visitExpression(const parser::ast::expression& expr) {
    ASTVisitor visitor(*this);
    return boost::apply_visitor(visitor, expr);
}

NodeId ASGBuilder::visitIdentifier(const parser::ast::identifier& id) {
    SourceLocation location = extractLocation(parser::ast::expression(id));
    NodeId node_id = graph_.addNode(NodeType::IDENTIFIER, parser::ast::expression(id), location);
    
    // Track variable use
    trackVariableUse(id.name, node_id);
    
    return node_id;
}

NodeId ASGBuilder::visitIntegerLiteral(const parser::ast::integer_literal& lit) {
    SourceLocation location = extractLocation(parser::ast::expression(lit));
    NodeId node_id = graph_.addNode(NodeType::LITERAL, parser::ast::expression(lit), location);
    
    // Create type edge to int type
    createTypeEdge(node_id, "int");
    
    return node_id;
}

NodeId ASGBuilder::visitFloatLiteral(const parser::ast::float_literal& lit) {
    SourceLocation location = extractLocation(parser::ast::expression(lit));
    NodeId node_id = graph_.addNode(NodeType::LITERAL, parser::ast::expression(lit), location);
    
    // Create type edge to float type
    createTypeEdge(node_id, "float");
    
    return node_id;
}

NodeId ASGBuilder::visitStringLiteral(const parser::ast::string_literal& lit) {
    SourceLocation location = extractLocation(parser::ast::expression(lit));
    NodeId node_id = graph_.addNode(NodeType::LITERAL, parser::ast::expression(lit), location);
    
    // Create type edge to string type
    createTypeEdge(node_id, "string");
    
    return node_id;
}

NodeId ASGBuilder::visitBooleanLiteral(const parser::ast::boolean_literal& lit) {
    SourceLocation location = extractLocation(parser::ast::expression(lit));
    NodeId node_id = graph_.addNode(NodeType::LITERAL, parser::ast::expression(lit), location);
    
    // Create type edge to bool type
    createTypeEdge(node_id, "bool");
    
    return node_id;
}

NodeId ASGBuilder::visitFunctionCall(const parser::ast::function_call& call) {
    SourceLocation location = extractLocation(parser::ast::expression(call));
    NodeId node_id = graph_.addNode(NodeType::FUNCTION_CALL, parser::ast::expression(call), location);
    
    // Create call edge
    createCallEdge(node_id, call.function_name.name);
    
    // Visit arguments and create data flow edges
    for (const auto& arg : call.arguments) {
        NodeId arg_node = visitExpression(arg);
        createControlFlowEdge(arg_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    // Visit named arguments
    for (const auto& named_arg : call.named_arguments) {
        NodeId arg_node = visitExpression(named_arg.value);
        createControlFlowEdge(arg_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    // Create control flow edge from last statement
    if (last_statement_node_ != 0) {
        createControlFlowEdge(last_statement_node_, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    last_statement_node_ = node_id;
    
    return node_id;
}

NodeId ASGBuilder::visitValDeclaration(const parser::ast::val_declaration& decl) {
    SourceLocation location = extractLocation(parser::ast::expression(decl));
    NodeId node_id = graph_.addNode(NodeType::VARIABLE_DECLARATION, parser::ast::expression(decl), location);
    
    // Visit the value expression
    NodeId value_node = visitExpression(decl.value);
    
    // Create data flow edge from value to declaration
    createControlFlowEdge(value_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    
    // Track variable definition
    trackVariableDefinition(decl.name.name, node_id);
    
    // Create scope edge
    createScopeEdge(node_id, decl.name.name);
    
    // Create control flow edge from last statement
    if (last_statement_node_ != 0) {
        createControlFlowEdge(last_statement_node_, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    last_statement_node_ = node_id;
    
    return node_id;
}

NodeId ASGBuilder::visitVarDeclaration(const parser::ast::var_declaration& decl) {
    SourceLocation location = extractLocation(parser::ast::expression(decl));
    NodeId node_id = graph_.addNode(NodeType::VARIABLE_DECLARATION, parser::ast::expression(decl), location);
    
    // Visit the value expression
    NodeId value_node = visitExpression(decl.value);
    
    // Create data flow edge from value to declaration
    createControlFlowEdge(value_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    
    // Track variable definition
    trackVariableDefinition(decl.name.name, node_id);
    
    // Create scope edge
    createScopeEdge(node_id, decl.name.name);
    
    // Create control flow edge from last statement
    if (last_statement_node_ != 0) {
        createControlFlowEdge(last_statement_node_, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    last_statement_node_ = node_id;
    
    return node_id;
}

NodeId ASGBuilder::visitBinaryOperation(const parser::ast::binary_operation& op) {
    SourceLocation location = extractLocation(parser::ast::expression(op));
    NodeId node_id = graph_.addNode(NodeType::BINARY_OPERATION, parser::ast::expression(op), location);
    
    // Visit operands
    NodeId left_node = visitExpression(op.left);
    NodeId right_node = visitExpression(op.right);
    
    // Create control flow edges
    createControlFlowEdge(left_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    createControlFlowEdge(right_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    
    return node_id;
}

NodeId ASGBuilder::visitUnaryOperation(const parser::ast::unary_operation& op) {
    SourceLocation location = extractLocation(parser::ast::expression(op));
    NodeId node_id = graph_.addNode(NodeType::UNARY_OPERATION, parser::ast::expression(op), location);
    
    // Visit operand
    NodeId operand_node = visitExpression(op.operand);
    
    // Create control flow edge
    createControlFlowEdge(operand_node, node_id, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    
    return node_id;
}

NodeId ASGBuilder::visitFunctionDefinition(const parser::ast::function_definition& func) {
    SourceLocation location = extractLocation(parser::ast::expression(func));
    NodeId node_id = graph_.addNode(NodeType::FUNCTION_DECLARATION, parser::ast::expression(func), location);
    
    // Track function definition
    trackVariableDefinition(func.name.name, node_id);
    
    // Create scope edge for function name
    createScopeEdge(node_id, func.name.name);
    
    // Enter function scope
    enterScope(node_id);
    current_function_node_ = node_id;
    
    // Visit parameters
    for (const auto& param : func.parameters) {
        SourceLocation param_location = extractLocation(parser::ast::expression(func));
        NodeId param_node = graph_.addNode(NodeType::PARAMETER, parser::ast::expression(func), param_location);
        
        // Track parameter as variable definition
        trackVariableDefinition(param.name.name, param_node);
        createScopeEdge(param_node, param.name.name);
    }
    
    // Visit function body
    {
        NodeId body_node = visitBlockExpression(func.body.get());
        createControlFlowEdge(node_id, body_node, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
    }
    
    // Exit function scope
    current_function_node_ = 0;
    exitScope();
    
    return node_id;
}

NodeId ASGBuilder::visitBlockExpression(const parser::ast::block_expression& block) {
    SourceLocation location;
    parser::ast::expression placeholder;
    // Use the first statement's location if available
    if (!block.statements.empty()) {
        location = extractLocation(block.statements.front());
        placeholder = block.statements.front().get();
    }
    NodeId node_id = graph_.addNode(NodeType::BLOCK, placeholder, location);
    
    // Enter block scope
    enterScope(node_id);
    
    NodeId prev_node = node_id;
    
    // Visit statements in sequence
    for (const auto& stmt : block.statements) {
        NodeId stmt_node = visitExpression(stmt);
        
        // Create control flow edge from previous statement
        createControlFlowEdge(prev_node, stmt_node, ControlFlowEdge::FlowCondition::UNCONDITIONAL);
        prev_node = stmt_node;
    }
    
    // Exit block scope
    exitScope();
    
    return node_id;
}

// Scope management methods

void ASGBuilder::enterScope(NodeId scope_node_id) {
    ScopeInfo* new_scope = new ScopeInfo(scope_node_id, current_scope_);
    scope_stack_.push(current_scope_);
    current_scope_ = new_scope;
}

void ASGBuilder::exitScope() {
    if (current_scope_) {
        delete current_scope_;
        current_scope_ = scope_stack_.empty() ? nullptr : scope_stack_.top();
        if (!scope_stack_.empty()) {
            scope_stack_.pop();
        }
    }
}

// Edge creation methods

void ASGBuilder::createDataFlowEdges(const std::string& symbol, NodeId usage_node, 
                                   DataFlowEdge::FlowType flow_type) {
    if (current_scope_) {
        NodeId def_node = current_scope_->lookupSymbol(symbol);
        if (def_node != 0) {
            graph_.addDataFlowEdge(def_node, usage_node, flow_type, symbol);
        }
    }
}

void ASGBuilder::createControlFlowEdge(NodeId from, NodeId to, 
                                     ControlFlowEdge::FlowCondition condition) {
    graph_.addControlFlowEdge(from, to, condition);
}

void ASGBuilder::createScopeEdge(NodeId declaration_node, const std::string& symbol) {
    if (current_scope_) {
        graph_.addScopeEdge(declaration_node, current_scope_->scope_node_id, 
                           ScopeEdge::ScopeType::DECLARATION, symbol);
    }
}

void ASGBuilder::createTypeEdge(NodeId value_node, const std::string& type_name) {
    // For now, create a dummy type node - in a full implementation, 
    // this would reference actual type nodes
    SourceLocation dummy_location(file_path_, 0, 0, 0);
    parser::ast::identifier type_id;
    type_id.name = type_name;
    NodeId type_node = graph_.addNode(NodeType::TYPE_DECLARATION, 
                                     parser::ast::expression(type_id), dummy_location);
    
    graph_.addTypeEdge(value_node, type_node, TypeEdge::TypeRelation::INSTANCE_OF, type_name);
}

void ASGBuilder::createCallEdge(NodeId call_node, const std::string& function_name) {
    if (current_scope_) {
        NodeId func_node = current_scope_->lookupSymbol(function_name);
        if (func_node != 0) {
            graph_.addCallEdge(call_node, func_node, CallEdge::CallType::DIRECT_CALL, function_name);
        }
    }
}

// Helper methods

SourceLocation ASGBuilder::extractLocation(const parser::ast::expression& expr) const {
    // This is a simplified implementation - in practice, you'd extract 
    // position information from the AST node
    return SourceLocation(file_path_, 1, 1, 0);
}

void ASGBuilder::trackVariableDefinition(const std::string& symbol, NodeId node_id) {
    if (current_scope_) {
        current_scope_->defineSymbol(symbol, node_id);
    }
    
    createDataFlowEdges(symbol, node_id, DataFlowEdge::FlowType::DEFINITION);
}

void ASGBuilder::trackVariableUse(const std::string& symbol, NodeId node_id) {
    createDataFlowEdges(symbol, node_id, DataFlowEdge::FlowType::USE);
}

void ASGBuilder::trackVariableModification(const std::string& symbol, NodeId node_id) {
    createDataFlowEdges(symbol, node_id, DataFlowEdge::FlowType::MODIFICATION);
}

} // namespace meld::api