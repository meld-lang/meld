#include <gtest/gtest.h>
#include "meld/parser/ast.hpp"
#include "meld/parser/query_desugar.hpp"
#include <boost/spirit/home/x3/support/ast/variant.hpp>

using namespace meld::parser;
using namespace meld::parser::ast;

// Helper function to create an identifier
identifier make_id(const std::string& name) {
    identifier id;
    id.name = name;
    return id;
}

// Helper function to create an integer literal
expression make_int(int64_t value) {
    integer_literal lit;
    lit.value = value;
    lit.suffix = "";
    return expression(lit);
}

// Helper function to create a binary operation
expression make_binop(const std::string& op, const expression& left, const expression& right) {
    binary_operation binop;
    binop.op = op;
    binop.left = left;
    binop.right = right;
    return expression(boost::spirit::x3::forward_ast<binary_operation>(binop));
}

// Test basic query structure creation
TEST(QueryTest, BasicQueryStructure) {
    query_expression query;
    
    // from u in users
    query.from_clause.binding = make_id("u");
    query.from_clause.source = make_id("users");
    
    // where u.age >= 18
    query_where_clause where_clause;
    // For simplicity, just use a placeholder expression
    where_clause.condition = make_int(1);
    query.where_clauses.push_back(where_clause);
    
    // select u.name
    query.select_clause.projection = make_id("u");
    
    EXPECT_EQ(query.from_clause.binding.name, "u");
    EXPECT_EQ(query.where_clauses.size(), 1);
}

// Test query desugaring - simple filter and select
TEST(QueryTest, DesugarSimpleFilterSelect) {
    query_expression query;
    
    // from x in numbers
    query.from_clause.binding = make_id("x");
    query.from_clause.source = make_id("numbers");
    
    // where x > 5
    query_where_clause where_clause;
    where_clause.condition = make_binop(">", make_id("x"), make_int(5));
    query.where_clauses.push_back(where_clause);
    
    // select x * 2
    query.select_clause.projection = make_binop("*", make_id("x"), make_int(2));
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // The result should be a pipeline of operations
    // numbers.filter { x => x > 5 }.map { x => x * 2 }
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query desugaring - multiple where clauses
TEST(QueryTest, DesugarMultipleWhere) {
    query_expression query;
    
    // from x in numbers
    query.from_clause.binding = make_id("x");
    query.from_clause.source = make_id("numbers");
    
    // where x > 5
    query_where_clause where1;
    where1.condition = make_binop(">", make_id("x"), make_int(5));
    query.where_clauses.push_back(where1);
    
    // where x < 20
    query_where_clause where2;
    where2.condition = make_binop("<", make_id("x"), make_int(20));
    query.where_clauses.push_back(where2);
    
    // select x
    query.select_clause.projection = make_id("x");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should create two filter calls
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query desugaring - join clause
TEST(QueryTest, DesugarJoin) {
    query_expression query;
    
    // from u in users
    query.from_clause.binding = make_id("u");
    query.from_clause.source = make_id("users");
    
    // join o in orders on u.id == o.userId
    query_join_clause join_clause;
    join_clause.binding = make_id("o");
    join_clause.source = make_id("orders");
    join_clause.left_key = make_id("u_id");
    join_clause.right_key = make_id("o_userId");
    query.join_clauses.push_back(join_clause);
    
    // select u
    query.select_clause.projection = make_id("u");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should create a join call
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query desugaring - group by clause
TEST(QueryTest, DesugarGroupBy) {
    query_expression query;
    
    // from u in users
    query.from_clause.binding = make_id("u");
    query.from_clause.source = make_id("users");
    
    // group u by u.age into g
    query.has_group = true;
    query.group_clause.value_expr = make_id("u");
    query.group_clause.key_expr = make_id("u_age");
    query.group_clause.into_binding = make_id("g");
    query.group_clause.has_into = true;
    
    // select g
    query.select_clause.projection = make_id("g");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should create a groupBy call
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query desugaring - complex query with all clauses
TEST(QueryTest, DesugarComplexQuery) {
    query_expression query;
    
    // from u in users
    query.from_clause.binding = make_id("u");
    query.from_clause.source = make_id("users");
    
    // where u.age >= 18
    query_where_clause where_clause;
    where_clause.condition = make_binop(">=", make_id("u_age"), make_int(18));
    query.where_clauses.push_back(where_clause);
    
    // join o in orders on u.id == o.userId
    query_join_clause join_clause;
    join_clause.binding = make_id("o");
    join_clause.source = make_id("orders");
    join_clause.left_key = make_id("u_id");
    join_clause.right_key = make_id("o_userId");
    query.join_clauses.push_back(join_clause);
    
    // group o by u.name into g
    query.has_group = true;
    query.group_clause.value_expr = make_id("o");
    query.group_clause.key_expr = make_id("u_name");
    query.group_clause.into_binding = make_id("g");
    query.group_clause.has_into = true;
    
    // select g
    query.select_clause.projection = make_id("g");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should create a pipeline with filter, join, groupBy, and map
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test lambda creation helper
TEST(QueryTest, CreateLambda) {
    identifier binding = make_id("x");
    expression body = make_int(42);
    
    lambda_expression lambda = QueryDesugarer::create_lambda(binding, body);
    
    EXPECT_EQ(lambda.parameters.size(), 1);
    EXPECT_EQ(lambda.parameters[0].name.name, "x");
    EXPECT_FALSE(lambda.is_block);
}

// Test binary lambda creation helper
TEST(QueryTest, CreateBinaryLambda) {
    identifier binding1 = make_id("x");
    identifier binding2 = make_id("y");
    expression body = make_int(42);
    
    lambda_expression lambda = QueryDesugarer::create_binary_lambda(binding1, binding2, body);
    
    EXPECT_EQ(lambda.parameters.size(), 2);
    EXPECT_EQ(lambda.parameters[0].name.name, "x");
    EXPECT_EQ(lambda.parameters[1].name.name, "y");
    EXPECT_FALSE(lambda.is_block);
}

// Test query without where clause
TEST(QueryTest, DesugarNoWhere) {
    query_expression query;
    
    // from x in numbers
    query.from_clause.binding = make_id("x");
    query.from_clause.source = make_id("numbers");
    
    // select x
    query.select_clause.projection = make_id("x");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should just be a map call
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query without group clause
TEST(QueryTest, DesugarNoGroup) {
    query_expression query;
    
    // from x in numbers
    query.from_clause.binding = make_id("x");
    query.from_clause.source = make_id("numbers");
    
    // where x > 0
    query_where_clause where_clause;
    where_clause.condition = make_binop(">", make_id("x"), make_int(0));
    query.where_clauses.push_back(where_clause);
    
    // select x
    query.select_clause.projection = make_id("x");
    
    query.has_group = false;
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should be filter and map
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

// Test query with multiple joins
TEST(QueryTest, DesugarMultipleJoins) {
    query_expression query;
    
    // from u in users
    query.from_clause.binding = make_id("u");
    query.from_clause.source = make_id("users");
    
    // join o in orders on u.id == o.userId
    query_join_clause join1;
    join1.binding = make_id("o");
    join1.source = make_id("orders");
    join1.left_key = make_id("u_id");
    join1.right_key = make_id("o_userId");
    query.join_clauses.push_back(join1);
    
    // join p in products on o.productId == p.id
    query_join_clause join2;
    join2.binding = make_id("p");
    join2.source = make_id("products");
    join2.left_key = make_id("o_productId");
    join2.right_key = make_id("p_id");
    query.join_clauses.push_back(join2);
    
    // select u
    query.select_clause.projection = make_id("u");
    
    // Desugar
    expression result = QueryDesugarer::desugar(query);
    
    // Should create two join calls
    EXPECT_TRUE(boost::get<boost::spirit::x3::forward_ast<pipeline_expression>>(&result) != nullptr);
}

