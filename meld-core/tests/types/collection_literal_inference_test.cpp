#include <gtest/gtest.h>
#include "../../include/meld/types/collection_literal_inference.hpp"
#include "../../include/meld/parser/ast.hpp"

using namespace meld::types;
using namespace meld::parser::ast;

class CollectionLiteralInferenceTest : public ::testing::Test {
protected:
    // Helper to create an integer literal expression
    expression makeIntLiteral(int value) {
        integer_literal lit;
        lit.value = value;
        return expression(lit);
    }
    
    // Helper to create a string literal expression
    expression makeStringLiteral(const std::string& value) {
        string_literal lit;
        lit.value = value;
        lit.has_interpolation = false;
        return expression(lit);
    }
    
    // Helper to create a boolean literal expression
    expression makeBoolLiteral(bool value) {
        boolean_literal lit;
        lit.value = value;
        return expression(lit);
    }
    
    // Helper to create an anonymous object field
    anonymous_object_field makeField(const std::string& name, const expression& value) {
        anonymous_object_field field;
        field.field_name.name = name;
        field.value = value;
        return field;
    }
};

// Test empty curly braces requires annotation
TEST_F(CollectionLiteralInferenceTest, EmptyCurlyBracesRequiresAnnotation) {
    std::vector<anonymous_object_field> fields;
    
    auto result = CollectionLiteralInference::inferCurlyBraces(fields);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::EMPTY_COLLECTION);
    EXPECT_TRUE(result.requires_annotation);
    EXPECT_EQ(result.inferred_type, nullptr);
}

// Test homogeneous curly braces infers to Map
TEST_F(CollectionLiteralInferenceTest, HomogeneousCurlyBracesInfersMap) {
    std::vector<anonymous_object_field> fields = {
        makeField("active", makeBoolLiteral(true)),
        makeField("checked", makeBoolLiteral(true)),
        makeField("enabled", makeBoolLiteral(false))
    };
    
    auto result = CollectionLiteralInference::inferCurlyBraces(fields);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::ANONYMOUS_MAP);
    EXPECT_FALSE(result.requires_annotation);
    ASSERT_NE(result.inferred_type, nullptr);
    EXPECT_EQ(result.inferred_type->name(), "Map<String, Bool>");
}

// Test heterogeneous curly braces infers to Anonymous Object
TEST_F(CollectionLiteralInferenceTest, HeterogeneousCurlyBracesInfersObject) {
    std::vector<anonymous_object_field> fields = {
        makeField("status", makeStringLiteral("ACTIVE")),
        makeField("count", makeIntLiteral(42)),
        makeField("active", makeBoolLiteral(true))
    };
    
    auto result = CollectionLiteralInference::inferCurlyBraces(fields);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::ANONYMOUS_OBJECT);
    EXPECT_FALSE(result.requires_annotation);
    ASSERT_NE(result.inferred_type, nullptr);
    EXPECT_EQ(result.inferred_type->name(), "{status: String, count: Int, active: Bool}");
}

// Test empty square brackets requires annotation
TEST_F(CollectionLiteralInferenceTest, EmptySquareBracketsRequiresAnnotation) {
    std::vector<expression> elements;
    
    auto result = CollectionLiteralInference::inferSquareBrackets(elements);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::EMPTY_COLLECTION);
    EXPECT_TRUE(result.requires_annotation);
    EXPECT_EQ(result.inferred_type, nullptr);
}

// Test homogeneous square brackets infers to Array
TEST_F(CollectionLiteralInferenceTest, HomogeneousSquareBracketsInfersArray) {
    std::vector<expression> elements = {
        makeIntLiteral(1),
        makeIntLiteral(2),
        makeIntLiteral(3),
        makeIntLiteral(4)
    };
    
    auto result = CollectionLiteralInference::inferSquareBrackets(elements);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::ANONYMOUS_ARRAY);
    EXPECT_FALSE(result.requires_annotation);
    ASSERT_NE(result.inferred_type, nullptr);
    EXPECT_EQ(result.inferred_type->name(), "Array<Int>");
}

// Test heterogeneous square brackets infers to Tuple
TEST_F(CollectionLiteralInferenceTest, HeterogeneousSquareBracketsInfersTuple) {
    std::vector<expression> elements = {
        makeIntLiteral(3),
        makeStringLiteral("Yes"),
        makeBoolLiteral(true)
    };
    
    auto result = CollectionLiteralInference::inferSquareBrackets(elements);
    
    EXPECT_EQ(result.kind, CollectionLiteralKind::ANONYMOUS_TUPLE);
    EXPECT_FALSE(result.requires_annotation);
}

// Test type homogeneity check for integers
TEST_F(CollectionLiteralInferenceTest, TypeHomogeneityIntegers) {
    std::vector<expression> elements = {
        makeIntLiteral(1),
        makeIntLiteral(2),
        makeIntLiteral(3)
    };
    
    EXPECT_TRUE(CollectionLiteralInference::areTypesHomogeneous(elements));
}

// Test type homogeneity check for strings
TEST_F(CollectionLiteralInferenceTest, TypeHomogeneityStrings) {
    std::vector<expression> elements = {
        makeStringLiteral("No"),
        makeStringLiteral("Yes"),
        makeStringLiteral("Maybe")
    };
    
    EXPECT_TRUE(CollectionLiteralInference::areTypesHomogeneous(elements));
}

// Test type heterogeneity check
TEST_F(CollectionLiteralInferenceTest, TypeHeterogeneity) {
    std::vector<expression> elements = {
        makeIntLiteral(1),
        makeStringLiteral("hello"),
        makeBoolLiteral(true)
    };
    
    EXPECT_FALSE(CollectionLiteralInference::areTypesHomogeneous(elements));
}

// Test field type homogeneity check
TEST_F(CollectionLiteralInferenceTest, FieldTypeHomogeneity) {
    std::vector<anonymous_object_field> fields = {
        makeField("a", makeIntLiteral(1)),
        makeField("b", makeIntLiteral(2)),
        makeField("c", makeIntLiteral(3))
    };
    
    EXPECT_TRUE(CollectionLiteralInference::areFieldTypesHomogeneous(fields));
}

// Test field type heterogeneity check
TEST_F(CollectionLiteralInferenceTest, FieldTypeHeterogeneity) {
    std::vector<anonymous_object_field> fields = {
        makeField("status", makeStringLiteral("ACTIVE")),
        makeField("count", makeIntLiteral(42))
    };
    
    EXPECT_FALSE(CollectionLiteralInference::areFieldTypesHomogeneous(fields));
}

// Test type annotation resolver for Map
TEST_F(CollectionLiteralInferenceTest, ResolveMapAnnotation) {
    auto type = TypeAnnotationResolver::resolveAnnotation("Map<String, Int>");
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Map<String, Int>");
}

// Test type annotation resolver for Set
TEST_F(CollectionLiteralInferenceTest, ResolveSetAnnotation) {
    auto type = TypeAnnotationResolver::resolveAnnotation("Set<String>");
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Set<String>");
}

// Test type annotation resolver for Array
TEST_F(CollectionLiteralInferenceTest, ResolveArrayAnnotation) {
    auto type = TypeAnnotationResolver::resolveAnnotation("Array<Int>");
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Array<Int>");
}

// Test collection type check
TEST_F(CollectionLiteralInferenceTest, IsCollectionType) {
    EXPECT_TRUE(TypeAnnotationResolver::isCollectionType("Map<String, Int>"));
    EXPECT_TRUE(TypeAnnotationResolver::isCollectionType("Set<String>"));
    EXPECT_TRUE(TypeAnnotationResolver::isCollectionType("Array<Int>"));
    EXPECT_FALSE(TypeAnnotationResolver::isCollectionType("Int"));
    EXPECT_FALSE(TypeAnnotationResolver::isCollectionType("String"));
}

// Test expression type inference for integer
TEST_F(CollectionLiteralInferenceTest, InferIntegerType) {
    auto expr = makeIntLiteral(42);
    auto type = CollectionLiteralInference::inferExpressionType(expr);
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Int");
}

// Test expression type inference for string
TEST_F(CollectionLiteralInferenceTest, InferStringType) {
    auto expr = makeStringLiteral("hello");
    auto type = CollectionLiteralInference::inferExpressionType(expr);
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "String");
}

// Test expression type inference for boolean
TEST_F(CollectionLiteralInferenceTest, InferBooleanType) {
    auto expr = makeBoolLiteral(true);
    auto type = CollectionLiteralInference::inferExpressionType(expr);
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Bool");
}

// Test common type extraction
TEST_F(CollectionLiteralInferenceTest, GetCommonType) {
    std::vector<expression> elements = {
        makeIntLiteral(1),
        makeIntLiteral(2),
        makeIntLiteral(3)
    };
    
    auto type = CollectionLiteralInference::getCommonType(elements);
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "Int");
}

// Test common field type extraction
TEST_F(CollectionLiteralInferenceTest, GetCommonFieldType) {
    std::vector<anonymous_object_field> fields = {
        makeField("a", makeStringLiteral("hello")),
        makeField("b", makeStringLiteral("world"))
    };
    
    auto type = CollectionLiteralInference::getCommonFieldType(fields);
    
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->name(), "String");
}
