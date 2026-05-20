#pragma once

#include "ast.hpp"
#include <memory>

namespace meld::parser {

/**
 * Query desugaring - transforms query expressions into fluent collection API calls
 * 
 * Example transformation:
 * 
 * query {
 *   from u in users
 *   where u.age >= 18
 *   join o in orders on u.id == o.userId
 *   group o by u.name into g
 *   select { name = g.key, total = g.sum { it.amount } }
 * }
 * 
 * Desugars to:
 * 
 * users
 *   .filter { u => u.age >= 18 }
 *   .join(orders, { u, o => u.id == o.userId })
 *   .groupBy { u => u.name }
 *   .map { g => { name = g.key, total = g.sum { it.amount } } }
 */
class QueryDesugarer {
public:
    /**
     * Desugar a query expression into fluent API calls
     * @param query The query expression to desugar
     * @return An expression representing the desugared fluent API calls
     */
    static ast::expression desugar(const ast::query_expression& query);
    
private:
    /**
     * Create a lambda expression from a binding and body
     * @param binding The parameter name
     * @param body The lambda body expression
     * @return A lambda expression
     */
    static ast::lambda_expression create_lambda(
        const ast::identifier& binding,
        const ast::expression& body
    );
    
    /**
     * Create a two-parameter lambda expression
     * @param binding1 First parameter name
     * @param binding2 Second parameter name
     * @param body The lambda body expression
     * @return A lambda expression
     */
    static ast::lambda_expression create_binary_lambda(
        const ast::identifier& binding1,
        const ast::identifier& binding2,
        const ast::expression& body
    );
    
    /**
     * Create a function call expression
     * @param receiver The object to call the method on
     * @param method_name The method name
     * @param args The arguments to pass
     * @return A function call expression
     */
    static ast::expression create_method_call(
        const ast::expression& receiver,
        const std::string& method_name,
        const std::vector<ast::expression>& args
    );
    
    /**
     * Desugar where clauses into filter calls
     * @param source The source expression
     * @param binding The current binding variable
     * @param where_clauses The where clauses to desugar
     * @return The expression with filter calls applied
     */
    static ast::expression desugar_where_clauses(
        const ast::expression& source,
        const ast::identifier& binding,
        const std::vector<ast::query_where_clause>& where_clauses
    );
    
    /**
     * Desugar join clauses into join calls
     * @param source The source expression
     * @param left_binding The left binding variable
     * @param join_clauses The join clauses to desugar
     * @return The expression with join calls applied
     */
    static ast::expression desugar_join_clauses(
        const ast::expression& source,
        const ast::identifier& left_binding,
        const std::vector<ast::query_join_clause>& join_clauses
    );
    
    /**
     * Desugar group clause into groupBy call
     * @param source The source expression
     * @param binding The current binding variable
     * @param group_clause The group clause to desugar
     * @return The expression with groupBy call applied
     */
    static ast::expression desugar_group_clause(
        const ast::expression& source,
        const ast::identifier& binding,
        const ast::query_group_clause& group_clause
    );
    
    /**
     * Desugar select clause into map call
     * @param source The source expression
     * @param binding The current binding variable
     * @param select_clause The select clause to desugar
     * @return The expression with map call applied
     */
    static ast::expression desugar_select_clause(
        const ast::expression& source,
        const ast::identifier& binding,
        const ast::query_select_clause& select_clause
    );
};

} // namespace meld::parser
