/**
 * Unit tests for C#-style property syntax rejection.
 *
 * Meld rejects C#-style get/set blocks on field declarations and produces
 * a clear error message suggesting @Property decorator instead.
 *
 * Requirements: 17.6, 17.7
 */

#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

struct ParseResult {
    bool success;
    std::vector<expression> ast;
    std::string error;
};

ParseResult try_parse(const std::string& source) {
    ParseResult result;
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        result.success = false;
        result.error = lexer.errors().front();
        return result;
    }
    TokenParser parser(tokens);
    result.success = parser.parse_file(result.ast);
    if (!result.success) {
        result.error = parser.error_message();
    }
    return result;
}

} // anonymous namespace

// ============================================================
// Class with getter block rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, ClassGetterBlockRejected) {
    auto result = try_parse(
        "class User {\n"
        "    var name: string {\n"
        "        get { rtn this._name }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
    EXPECT_NE(result.error.find("@Property"), std::string::npos)
        << "Actual error: " << result.error;
}

// ============================================================
// Class with getter and setter blocks rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, ClassGetterSetterBlockRejected) {
    auto result = try_parse(
        "class User {\n"
        "    var name: string {\n"
        "        get { rtn this._name }\n"
        "        set { this._name = value }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
    EXPECT_NE(result.error.find("@Property"), std::string::npos)
        << "Actual error: " << result.error;
}

// ============================================================
// Class with setter-first block rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, ClassSetterFirstBlockRejected) {
    auto result = try_parse(
        "class User {\n"
        "    var name: string {\n"
        "        set { this._name = value }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
}

// ============================================================
// Struct with getter block rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, StructGetterBlockRejected) {
    auto result = try_parse(
        "struct Point {\n"
        "    val x: int {\n"
        "        get { rtn 0 }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
    EXPECT_NE(result.error.find("@Property"), std::string::npos)
        << "Actual error: " << result.error;
}

// ============================================================
// Error message includes field name and type
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, ErrorMessageIncludesFieldNameAndType) {
    auto result = try_parse(
        "class Account {\n"
        "    var balance: int {\n"
        "        get { rtn this._balance }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("balance"), std::string::npos)
        << "Error should mention field name. Actual: " << result.error;
    EXPECT_NE(result.error.find("int"), std::string::npos)
        << "Error should mention field type. Actual: " << result.error;
}

// ============================================================
// Val field with getter block also rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, ValFieldWithGetterRejected) {
    auto result = try_parse(
        "class Config {\n"
        "    val timeout: int {\n"
        "        get { rtn 30 }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
}

// ============================================================
// Plain fields still work after rejection check
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, PlainFieldsStillWork) {
    auto result = try_parse(
        "class User {\n"
        "    var name: string\n"
        "    val age: int\n"
        "}");

    ASSERT_TRUE(result.success) << "Plain fields should parse fine. Error: " << result.error;
}

// ============================================================
// Mixed plain fields and C#-style property rejected
// Requirements: 17.6, 17.7
// ============================================================

TEST(CSharpPropertyRejectionTest, MixedFieldsWithCSharpPropertyRejected) {
    auto result = try_parse(
        "class User {\n"
        "    var name: string\n"
        "    val age: int\n"
        "    var email: string {\n"
        "        get { rtn this._email }\n"
        "    }\n"
        "}");

    ASSERT_FALSE(result.success);
    EXPECT_NE(result.error.find("C#-style property syntax is not supported in Meld"),
              std::string::npos) << "Actual error: " << result.error;
}
