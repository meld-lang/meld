#include <gtest/gtest.h>
#include "../../include/meld/compiler/type_checker.hpp"
#include "../../include/meld/parser/ast.hpp"
#include "../../include/meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;

class TypeCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<TypeChecker>();
        env = std::make_shared<TypeEnvironment>();
    }
    
    std::unique_ptr<TypeChecker> checker;
    std::shared_ptr<TypeEnvironment> env;
};

// Test literal type inference
TEST_F(TypeCheckerTest, IntegerLiteralInference) {
    integer_literal lit;
    lit.value = 42;
    lit.suffix = "";
    
    expression expr(lit);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int");
}

TEST_F(TypeCheckerTest, BooleanLiteralInference) {
    boolean_literal lit;
    lit.value = true;
    
    expression expr(lit);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Bool");
}

TEST_F(TypeCheckerTest, StringLiteralInference) {
    string_literal lit;
    lit.value = "hello";
    lit.has_interpolation = false;
    
    expression expr(lit);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "String");
}

// Test variable declarations
TEST_F(TypeCheckerTest, ValDeclaration) {
    val_declaration decl;
    decl.name.name = "x";
    
    integer_literal lit;
    lit.value = 42;
    decl.value = expression(lit);
    
    expression expr(decl);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that variable is bound in environment
    auto var_type = env->lookup("x");
    ASSERT_TRUE(var_type.has_value());
    EXPECT_EQ((*var_type)->name(), "Int");
}

TEST_F(TypeCheckerTest, UndefinedVariable) {
    identifier id;
    id.name = "undefined_var";
    
    expression expr(id);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("Undefined variable") != std::string::npos);
}

// Test binary operations
TEST_F(TypeCheckerTest, ArithmeticOperation) {
    binary_operation op;
    op.op = "+";
    
    integer_literal left;
    left.value = 10;
    op.left = expression(left);
    
    integer_literal right;
    right.value = 20;
    op.right = expression(right);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int");
}

TEST_F(TypeCheckerTest, ComparisonOperation) {
    binary_operation op;
    op.op = "==";
    
    integer_literal left;
    left.value = 10;
    op.left = expression(left);
    
    integer_literal right;
    right.value = 20;
    op.right = expression(right);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Bool");
}

TEST_F(TypeCheckerTest, LogicalOperation) {
    binary_operation op;
    op.op = "&&";
    
    boolean_literal left;
    left.value = true;
    op.left = expression(left);
    
    boolean_literal right;
    right.value = false;
    op.right = expression(right);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Bool");
}

TEST_F(TypeCheckerTest, TypeMismatchInLogicalOperation) {
    binary_operation op;
    op.op = "&&";
    
    boolean_literal left;
    left.value = true;
    op.left = expression(left);
    
    integer_literal right;
    right.value = 42;
    op.right = expression(right);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("boolean operands") != std::string::npos);
}

// Test unary operations
TEST_F(TypeCheckerTest, UnaryMinus) {
    unary_operation op;
    op.op = "-";
    
    integer_literal operand;
    operand.value = 42;
    op.operand = expression(operand);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int");
}

TEST_F(TypeCheckerTest, LogicalNot) {
    unary_operation op;
    op.op = "!";
    
    boolean_literal operand;
    operand.value = true;
    op.operand = expression(operand);
    
    expression expr(op);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Bool");
}

// Test struct definition
TEST_F(TypeCheckerTest, StructDefinition) {
    struct_definition def;
    def.name.name = "Point";
    
    field_declaration field1;
    field1.is_mutable = false;
    field1.name.name = "x";
    field1.type.type_name.name = "Int";
    field1.type.is_nullable = false;
    
    field_declaration field2;
    field2.is_mutable = false;
    field2.name.name = "y";
    field2.type.type_name.name = "Int";
    field2.type.is_nullable = false;
    
    def.fields.push_back(field1);
    def.fields.push_back(field2);
    
    expression expr(def);
    auto result = checker->check_expression(expr, env);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Point");
    
    // Check that type is registered
    auto& registry = TypeRegistry::instance();
    auto point_type = registry.get_type("Point");
    ASSERT_TRUE(point_type.has_value());
}

// Test type compatibility
TEST_F(TypeCheckerTest, TypeCompatibility) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    auto bool_type = registry.get_bool_type();
    
    // Same types are compatible
    EXPECT_TRUE(checker->is_compatible(*int_type, *int_type));
    
    // Different types are not compatible
    EXPECT_FALSE(checker->is_compatible(*int_type, *bool_type));
}

// Test nullable types
TEST_F(TypeCheckerTest, NullableTypeAnnotation) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    type_annotation annotation;
    annotation.type_name.name = "Int";
    annotation.is_nullable = true;
    
    // This would be tested through a full expression that uses the annotation
    // For now, we just verify the registry can create optional types
    auto optional_int = registry.create_optional_type(int_type);
    ASSERT_NE(optional_int, nullptr);
}

// Test error formatting
TEST_F(TypeCheckerTest, ErrorFormatting) {
    TypeError error("Test error message", "test.meld", 10, 5, "Additional context");
    std::string formatted = error.format();
    
    EXPECT_TRUE(formatted.find("test.meld") != std::string::npos);
    EXPECT_TRUE(formatted.find("10:5") != std::string::npos);
    EXPECT_TRUE(formatted.find("Test error message") != std::string::npos);
    EXPECT_TRUE(formatted.find("Additional context") != std::string::npos);
}

// Test type environment
TEST_F(TypeCheckerTest, TypeEnvironmentScoping) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    auto bool_type = registry.get_bool_type();
    
    auto parent_env = std::make_shared<TypeEnvironment>();
    parent_env->bind("x", int_type);
    
    auto child_env = parent_env->create_child();
    child_env->bind("y", bool_type);
    
    // Child can see parent's bindings
    auto x_type = child_env->lookup("x");
    ASSERT_TRUE(x_type.has_value());
    EXPECT_EQ((*x_type)->name(), "Int");
    
    // Child can see its own bindings
    auto y_type = child_env->lookup("y");
    ASSERT_TRUE(y_type.has_value());
    EXPECT_EQ((*y_type)->name(), "Bool");
    
    // Parent cannot see child's bindings
    auto y_in_parent = parent_env->lookup("y");
    EXPECT_FALSE(y_in_parent.has_value());
}

// Test program checking
TEST_F(TypeCheckerTest, CheckProgram) {
    std::vector<expression> program;
    
    // val x = 42
    val_declaration decl1;
    decl1.name.name = "x";
    integer_literal lit1;
    lit1.value = 42;
    decl1.value = expression(lit1);
    program.push_back(expression(decl1));
    
    // val y = true
    val_declaration decl2;
    decl2.name.name = "y";
    boolean_literal lit2;
    lit2.value = true;
    decl2.value = expression(lit2);
    program.push_back(expression(decl2));
    
    auto result = checker->check_program(program);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);
}

