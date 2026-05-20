/**
 * Property-based tests for C#-style property syntax rejection.
 *
 * Validates that the parser rejects C#-style get/set blocks on field
 * declarations in both classes and structs, with a clear error message
 * suggesting @Property decorator instead.
 *
 * Uses rapidcheck for property-based testing.
 *
 * Requirements: 17.6, 17.7
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genFieldName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "f_" + std::to_string(n); }
    );
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"),
        std::string("String"), std::string("Bool"),
        std::string("Double"));
}

rc::Gen<std::string> genClassName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestClass" + std::to_string(n); }
    );
}

rc::Gen<std::string> genStructName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestStruct" + std::to_string(n); }
    );
}

/// Generate "get", "set", or both in either order.
rc::Gen<std::string> genGetSetBlock() {
    return rc::gen::element(
        std::string("        get { 42 }\n"),
        std::string("        set { }\n"),
        std::string("        get { 42 }\n        set { }\n"),
        std::string("        set { }\n        get { 42 }\n"));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct ParseResult {
    bool success;
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
    std::vector<expression> ast;
    result.success = parser.parse_file(ast);
    if (!result.success) {
        result.error = parser.error_message();
    }
    return result;
}

} // anonymous namespace

// ===========================================================================
// Property 1: C#-style get/set on class fields always rejected
//
// For any class with a field declaration followed by { get { ... } } or
// { set { ... } } blocks, the parser SHALL reject with an error containing
// "C#-style property syntax is not supported in Meld" and "@Property".
//
// **Validates: Requirements 17.6, 17.7**
// ===========================================================================

TEST(CSharpPropertyRejectionPropertyTest, ClassFieldWithGetSetAlwaysRejected) {
    rc::check("C#-style get/set on class fields always rejected with @Property suggestion",
        []() {
            auto class_name = *genClassName();
            auto field_name = *genFieldName();
            auto type_name = *genTypeName();
            auto get_set_block = *genGetSetBlock();

            std::string source =
                "class " + class_name + " {\n"
                "    var " + field_name + ": " + type_name + " {\n"
                + get_set_block +
                "    }\n"
                "}";

            auto result = try_parse(source);
            RC_ASSERT(!result.success);
            RC_ASSERT(result.error.find("C#-style property syntax is not supported in Meld") != std::string::npos);
            RC_ASSERT(result.error.find("@Property") != std::string::npos);
        }
    );
}

// ===========================================================================
// Property 2: C#-style get/set on struct fields always rejected
//
// For any struct with a field declaration followed by { get { ... } } or
// { set { ... } } blocks, the parser SHALL reject with the same error.
//
// **Validates: Requirements 17.6, 17.7**
// ===========================================================================

TEST(CSharpPropertyRejectionPropertyTest, StructFieldWithGetSetAlwaysRejected) {
    rc::check("C#-style get/set on struct fields always rejected with @Property suggestion",
        []() {
            auto struct_name = *genStructName();
            auto field_name = *genFieldName();
            auto type_name = *genTypeName();
            auto get_set_block = *genGetSetBlock();

            std::string source =
                "struct " + struct_name + " {\n"
                "    val " + field_name + ": " + type_name + " {\n"
                + get_set_block +
                "    }\n"
                "}";

            auto result = try_parse(source);
            RC_ASSERT(!result.success);
            RC_ASSERT(result.error.find("C#-style property syntax is not supported in Meld") != std::string::npos);
            RC_ASSERT(result.error.find("@Property") != std::string::npos);
        }
    );
}

// ===========================================================================
// Property 3: Error message includes field name in @Property suggestion
//
// The error message SHALL include the field name so the developer knows
// which field to annotate with @Property.
//
// **Validates: Requirements 17.6, 17.7**
// ===========================================================================

TEST(CSharpPropertyRejectionPropertyTest, ErrorMessageIncludesFieldName) {
    rc::check("Error message includes field name in @Property suggestion",
        []() {
            auto class_name = *genClassName();
            auto field_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "class " + class_name + " {\n"
                "    var " + field_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "    }\n"
                "}";

            auto result = try_parse(source);
            RC_ASSERT(!result.success);
            RC_ASSERT(result.error.find(field_name) != std::string::npos);
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
