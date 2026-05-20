/**
 * Unit tests for mandatory val/var annotation enforcement.
 *
 * Tests specific examples, exact error messages, edge cases, and error locations
 * for the mandatory mutability annotation feature.
 *
 * Requirements: 1.3, 1.4, 2.3, 2.4, 3.3, 3.4, 4.3, 4.6, 5.3, 6.1, 6.2
 */

#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <boost/variant.hpp>
#include <string>
#include <vector>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// Parse source and return errors (non-fatal annotation errors).
// Returns true if parsing succeeded structurally (AST was built),
// even if annotation errors were collected.
struct ParseResult {
    bool structural_success;  // TokenParser::parse_file returned true
    std::vector<expression> ast;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string fatal_error;
};

ParseResult parse_with_errors(const std::string& source) {
    ParseResult result;
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        result.structural_success = false;
        result.fatal_error = lexer.errors().front();
        return result;
    }
    TokenParser parser(tokens);
    result.structural_success = parser.parse_file(result.ast);
    result.errors = parser.errors();
    result.warnings = parser.warnings();
    if (!result.structural_success) {
        result.fatal_error = parser.error_message();
    }
    return result;
}

} // anonymous namespace

// ============================================================
// Struct field: bare declaration produces error
// Requirements: 1.3, 1.4
// ============================================================

TEST(MandatoryValVarTest, BareStructFieldProducesError) {
    std::string source =
        "struct Point {\n"
        "    x: float\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("Missing mutability annotation"), std::string::npos);
    EXPECT_NE(result.errors[0].find("line 2"), std::string::npos);
}

TEST(MandatoryValVarTest, ValStructFieldNoError) {
    std::string source =
        "struct Point {\n"
        "    val x: float\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

TEST(MandatoryValVarTest, VarStructFieldNoError) {
    std::string source =
        "struct Point {\n"
        "    var x: float\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

// ============================================================
// Class field: bare declaration produces error
// Requirements: 2.3, 2.4
// ============================================================

TEST(MandatoryValVarTest, BareClassFieldProducesError) {
    std::string source =
        "class User {\n"
        "    name: string\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("Missing mutability annotation"), std::string::npos);
}

// ============================================================
// Property: C#-style get/set syntax is rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(MandatoryValVarTest, BarePropertyWithGetSetRejected) {
    std::string source =
        "class Rect {\n"
        "    val width: float\n"
        "    area: float {\n"
        "        get { width * width }\n"
        "    }\n"
        "}";

    auto result = parse_with_errors(source);
    // C#-style property syntax now produces a fatal error
    ASSERT_FALSE(result.structural_success);
    EXPECT_NE(result.fatal_error.find("C#-style property syntax is not supported in Meld"), std::string::npos);
    EXPECT_NE(result.fatal_error.find("@Property"), std::string::npos);
}

// ============================================================
// Function parameter: bare declaration produces error
// Requirements: 4.3, 4.6
// ============================================================

TEST(MandatoryValVarTest, BareFunctionParameterProducesError) {
    std::string source =
        "fn test_func(x: Int) -> Int {\n"
        "    return x\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("Missing mutability annotation"), std::string::npos);
}

TEST(MandatoryValVarTest, ValFunctionParameterNoError) {
    std::string source =
        "fn test_func(val x: Int) -> Int {\n"
        "    return x\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

TEST(MandatoryValVarTest, VarFunctionParameterNoError) {
    std::string source =
        "fn test_func(x: var Int) -> Int {\n"
        "    return x\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

// ============================================================
// Decorators serve as mutability annotation
// Requirements: 4.4, 4.5
// ============================================================

TEST(MandatoryValVarTest, ConstDecoratorNoError) {
    std::string source =
        "fn test_func(@const x: Int) -> Int {\n"
        "    return x\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

TEST(MandatoryValVarTest, MutDecoratorNoError) {
    std::string source =
        "fn test_func(@mut x: Int) -> Int {\n"
        "    return x\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

// ============================================================
// Enum variant fields: bare declaration produces error
// Requirements: 5.3
// ============================================================

TEST(MandatoryValVarTest, BareEnumVariantFieldProducesError) {
    std::string source =
        "enum Shape {\n"
        "    Circle(radius: float),\n"
        "    Rectangle(width: float, height: float)\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    // 3 bare fields: radius, width, height
    ASSERT_EQ(result.errors.size(), 3u);
    for (const auto& err : result.errors) {
        EXPECT_NE(err.find("Missing mutability annotation"), std::string::npos);
    }
}

TEST(MandatoryValVarTest, ValEnumVariantFieldNoError) {
    std::string source =
        "enum Shape {\n"
        "    Circle(val radius: float),\n"
        "    Rectangle(val width: float, val height: float)\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    EXPECT_TRUE(result.errors.empty());
}

// ============================================================
// Multiple errors in a single file
// Requirements: 6.1, 6.2
// ============================================================

TEST(MandatoryValVarTest, MultipleErrorsReportedInSinglePass) {
    std::string source =
        "struct Config {\n"
        "    host: string\n"
        "    port: int\n"
        "    var timeout: int\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    // 2 bare fields: host, port (timeout has var)
    ASSERT_EQ(result.errors.size(), 2u);
    EXPECT_NE(result.errors[0].find("Missing mutability annotation"), std::string::npos);
    EXPECT_NE(result.errors[1].find("Missing mutability annotation"), std::string::npos);
}

TEST(MandatoryValVarTest, MixedValidAndInvalidDeclarations) {
    std::string source =
        "struct Mixed {\n"
        "    val a: Int\n"
        "    b: Int\n"
        "    var c: Int\n"
        "    d: Int\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    // 2 bare fields: b, d
    ASSERT_EQ(result.errors.size(), 2u);

    // Verify AST was still built with recovery
    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&result.ast[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();
    ASSERT_EQ(def.fields.size(), 4u);

    // val a: immutable, explicit
    EXPECT_FALSE(def.fields[0].is_mutable);
    EXPECT_TRUE(def.fields[0].has_explicit_val);

    // b: recovered as immutable, not explicit
    EXPECT_FALSE(def.fields[1].is_mutable);
    EXPECT_FALSE(def.fields[1].has_explicit_val);

    // var c: mutable
    EXPECT_TRUE(def.fields[2].is_mutable);

    // d: recovered as immutable, not explicit
    EXPECT_FALSE(def.fields[3].is_mutable);
    EXPECT_FALSE(def.fields[3].has_explicit_val);
}

// ============================================================
// Error message format verification
// Requirements: 1.4, 2.4, 4.6
// ============================================================

TEST(MandatoryValVarTest, ErrorMessageContainsLineAndColumn) {
    std::string source =
        "struct Point {\n"
        "    x: float\n"
        "}";

    auto result = parse_with_errors(source);
    ASSERT_TRUE(result.structural_success) << result.fatal_error;
    ASSERT_EQ(result.errors.size(), 1u);
    // Error should contain "Error at line N:M"
    EXPECT_NE(result.errors[0].find("Error at line"), std::string::npos);
    EXPECT_NE(result.errors[0].find("Missing mutability annotation: use 'val' for immutable or 'var' for mutable"), std::string::npos);
}
// main() provided by gtest_main
