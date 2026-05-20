#include "meld/parser/query_desugar.hpp"
#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace meld::parser {

namespace x3 = boost::spirit::x3;

ast::expression QueryDesugarer::desugar(const ast::query_expression& query) {
    // Start with the from clause source
    ast::expression current = query.from_clause.source.get();
    ast::identifier current_binding = query.from_clause.binding;
    
    // Apply where clauses (filter operations)
    if (!query.where_clauses.empty()) {
        current = desugar_where_clauses(current, current_binding, query.where_clauses);
    }
    
    // Apply join clauses
    if (!query.join_clauses.empty()) {
        current = desugar_join_clauses(current, current_binding, query.join_clauses);
    }
    
    // Apply group clause if present
    if (query.has_group) {
        current = desugar_group_clause(current, current_binding, query.group_clause);
        // After grouping, the binding changes to the group binding
        if (query.group_clause.has_into) {
            current_binding = query.group_clause.into_binding;
        }
    }
    
    // Apply select clause (map operation)
    current = desugar_select_clause(current, current_binding, query.select_clause);
    
    return current;
}

ast::lambda_expression QueryDesugarer::create_lambda(
    const ast::identifier& binding,
    const ast::expression& body
) {
    ast::lambda_expression lambda;
    
    ast::lambda_parameter param;
    param.name = binding;
    param.has_type = false;
    lambda.parameters.push_back(param);
    
    lambda.body = x3::forward_ast<ast::expression>(body);
    lambda.is_block = false;
    
    return lambda;
}

ast::lambda_expression QueryDesugarer::create_binary_lambda(
    const ast::identifier& binding1,
    const ast::identifier& binding2,
    const ast::expression& body
) {
    ast::lambda_expression lambda;
    
    ast::lambda_parameter param1;
    param1.name = binding1;
    param1.has_type = false;
    lambda.parameters.push_back(param1);
    
    ast::lambda_parameter param2;
    param2.name = binding2;
    param2.has_type = false;
    lambda.parameters.push_back(param2);
    
    lambda.body = x3::forward_ast<ast::expression>(body);
    lambda.is_block = false;
    
    return lambda;
}

ast::expression QueryDesugarer::create_method_call(
    const ast::expression& receiver,
    const std::string& method_name,
    const std::vector<ast::expression>& args
) {
    // Create a function call that represents a method call
    // In the AST, this would be represented as receiver.method(args)
    // For simplicity, we'll create a function_call with the method name
    // and the receiver as an implicit context
    
    ast::function_call call;
    call.function_name.name = method_name;
    // Convert vector<expression> to vector<forward_ast<expression>>
    for (const auto& arg : args) {
        call.arguments.push_back(x3::forward_ast<ast::expression>(arg));
    }
    
    // Note: In a full implementation, we'd need to track the receiver
    // For now, we'll use a binary operation to represent method calls
    // receiver.method becomes a special form
    
    // Create a pipeline-like structure: receiver |> method(args)
    ast::pipeline_expression pipeline;
    pipeline.value = x3::forward_ast<ast::expression>(receiver);
    
    // The function is the method call
    auto call_fwd = x3::forward_ast<ast::function_call>(call);
    ast::expression func_expr(call_fwd);
    pipeline.function = x3::forward_ast<ast::expression>(func_expr);
    
    return ast::expression(x3::forward_ast<ast::pipeline_expression>(pipeline));
}

ast::expression QueryDesugarer::desugar_where_clauses(
    const ast::expression& source,
    const ast::identifier& binding,
    const std::vector<ast::query_where_clause>& where_clauses
) {
    ast::expression current = source;
    
    // Each where clause becomes a filter call
    for (const auto& where_clause : where_clauses) {
        ast::lambda_expression filter_lambda = create_lambda(
            binding,
            where_clause.condition.get()
        );
        
        std::vector<ast::expression> args;
        args.push_back(ast::expression(x3::forward_ast<ast::lambda_expression>(filter_lambda)));
        
        current = create_method_call(current, "filter", args);
    }
    
    return current;
}

ast::expression QueryDesugarer::desugar_join_clauses(
    const ast::expression& source,
    const ast::identifier& left_binding,
    const std::vector<ast::query_join_clause>& join_clauses
) {
    ast::expression current = source;
    
    // Each join clause becomes a join call
    for (const auto& join_clause : join_clauses) {
        // Create the join predicate lambda: (left, right) => left_key == right_key
        ast::binary_operation join_condition;
        join_condition.op = "==";
        join_condition.left = join_clause.left_key;
        join_condition.right = join_clause.right_key;
        
        ast::lambda_expression join_lambda = create_binary_lambda(
            left_binding,
            join_clause.binding,
            ast::expression(x3::forward_ast<ast::binary_operation>(join_condition))
        );
        
        std::vector<ast::expression> args;
        args.push_back(join_clause.source.get());
        args.push_back(ast::expression(x3::forward_ast<ast::lambda_expression>(join_lambda)));
        
        current = create_method_call(current, "join", args);
    }
    
    return current;
}

ast::expression QueryDesugarer::desugar_group_clause(
    const ast::expression& source,
    const ast::identifier& binding,
    const ast::query_group_clause& group_clause
) {
    // groupBy takes a key selector lambda
    ast::lambda_expression key_lambda = create_lambda(
        binding,
        group_clause.key_expr.get()
    );
    
    std::vector<ast::expression> args;
    args.push_back(ast::expression(x3::forward_ast<ast::lambda_expression>(key_lambda)));
    
    return create_method_call(source, "groupBy", args);
}

ast::expression QueryDesugarer::desugar_select_clause(
    const ast::expression& source,
    const ast::identifier& binding,
    const ast::query_select_clause& select_clause
) {
    // select becomes a map call
    ast::lambda_expression map_lambda = create_lambda(
        binding,
        select_clause.projection.get()
    );
    
    std::vector<ast::expression> args;
    args.push_back(ast::expression(x3::forward_ast<ast::lambda_expression>(map_lambda)));
    
    return create_method_call(source, "map", args);
}

} // namespace meld::parser
