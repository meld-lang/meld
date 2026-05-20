#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

namespace x3 = boost::spirit::x3;
using namespace meld::parser;
using namespace meld::parser::ast;

// Helper fixture for generic mutability qualifier tests (Req 165.1)
class GenericMutabilityQualifierTest : public ::testing::Test {
protected:
    bool parse_source(const std::string& source, std::vector<expression>& results) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) {
            last_error_ = lexer.errors().front();
            return false;
        }
        TokenParser parser(tokens);
        if (!parser.parse_file(results)) {
            last_error_ = parser.error_message();
            return false;
        }
        return true;
    }

    // Extract the type_annotation from a val declaration expression
    const type_annotation& get_val_type(const expression& expr) {
        auto* fwd = boost::get<x3::forward_ast<val_declaration>>(&expr);
        return fwd->get().type_ann.get();
    }

    std::string last_error_;
};

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, HoldValUser) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: Hold[val User] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "Hold");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 1u);

    const auto& arg = type.type_arguments[0].get();
    EXPECT_EQ(arg.type_name.name, "User");
    EXPECT_EQ(arg.mutability_qualifier, type_annotation::MutabilityQualifier::VAL);
}

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, HoldVarUser) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: Hold[var User] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "Hold");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 1u);

    const auto& arg = type.type_arguments[0].get();
    EXPECT_EQ(arg.type_name.name, "User");
    EXPECT_EQ(arg.mutability_qualifier, type_annotation::MutabilityQualifier::VAR);
}

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, MapValKVarV) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: Map[val K, var V] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "Map");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 2u);

    const auto& arg0 = type.type_arguments[0].get();
    EXPECT_EQ(arg0.type_name.name, "K");
    EXPECT_EQ(arg0.mutability_qualifier, type_annotation::MutabilityQualifier::VAL);

    const auto& arg1 = type.type_arguments[1].get();
    EXPECT_EQ(arg1.type_name.name, "V");
    EXPECT_EQ(arg1.mutability_qualifier, type_annotation::MutabilityQualifier::VAR);
}

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, ListUserNoQualifier) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: List[User] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "List");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 1u);

    const auto& arg = type.type_arguments[0].get();
    EXPECT_EQ(arg.type_name.name, "User");
    EXPECT_EQ(arg.mutability_qualifier, type_annotation::MutabilityQualifier::NONE);
}

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, NestedQualifiers) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: View[val Map[val K, var V]] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "View");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 1u);

    // Outer arg: val Map[val K, var V]
    const auto& outer_arg = type.type_arguments[0].get();
    EXPECT_EQ(outer_arg.type_name.name, "Map");
    EXPECT_EQ(outer_arg.mutability_qualifier, type_annotation::MutabilityQualifier::VAL);
    ASSERT_TRUE(outer_arg.has_type_arguments);
    ASSERT_EQ(outer_arg.type_arguments.size(), 2u);

    // Inner arg 0: val K
    const auto& inner0 = outer_arg.type_arguments[0].get();
    EXPECT_EQ(inner0.type_name.name, "K");
    EXPECT_EQ(inner0.mutability_qualifier, type_annotation::MutabilityQualifier::VAL);

    // Inner arg 1: var V
    const auto& inner1 = outer_arg.type_arguments[1].get();
    EXPECT_EQ(inner1.type_name.name, "V");
    EXPECT_EQ(inner1.mutability_qualifier, type_annotation::MutabilityQualifier::VAR);
}

// Validates: Requirements 165.1
TEST_F(GenericMutabilityQualifierTest, BackwardCompatibility) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source("val x: Hold[User] = 0", results)) << last_error_;
    ASSERT_EQ(results.size(), 1u);

    const auto& type = get_val_type(results[0]);
    EXPECT_EQ(type.type_name.name, "Hold");
    ASSERT_TRUE(type.has_type_arguments);
    ASSERT_EQ(type.type_arguments.size(), 1u);

    const auto& arg = type.type_arguments[0].get();
    EXPECT_EQ(arg.type_name.name, "User");
    EXPECT_EQ(arg.mutability_qualifier, type_annotation::MutabilityQualifier::NONE);
}
