#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// Helper: tokenize + parse a source string into a list of expressions
class ExplicitValTest : public ::testing::Test {
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
        warnings_ = parser.warnings();
        errors_ = parser.errors();
        // Annotation errors are non-fatal but still count as failure
        if (!errors_.empty()) {
            return false;
        }
        return true;
    }

    // Parse and capture errors but still return AST (for recovery testing)
    bool parse_source_with_recovery(const std::string& source,
                                     std::vector<expression>& results) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) {
            last_error_ = lexer.errors().front();
            return false;
        }
        TokenParser parser(tokens);
        bool ok = parser.parse_file(results);
        warnings_ = parser.warnings();
        errors_ = parser.errors();
        if (!ok) {
            last_error_ = parser.error_message();
        }
        return ok;  // structural success (AST built)
    }

    bool parse_single(const std::string& source, expression& result) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) {
            last_error_ = lexer.errors().front();
            return false;
        }
        TokenParser parser(tokens);
        if (!parser.parse_expression(result)) {
            last_error_ = parser.error_message();
            return false;
        }
        warnings_ = parser.warnings();
        return true;
    }

    // Try to parse and expect failure; capture error message
    bool parse_fails(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) {
            last_error_ = lexer.errors().front();
            return true; // lexer error counts as failure
        }
        TokenParser parser(tokens);
        std::vector<expression> results;
        if (!parser.parse_file(results)) {
            last_error_ = parser.error_message();
            warnings_ = parser.warnings();
            return true;
        }
        warnings_ = parser.warnings();
        return false;
    }

    std::string last_error_;
    std::vector<std::string> warnings_;
    std::vector<std::string> errors_;
};

// ============================================================
// 1. Struct with explicit val fields
//    Validates: Requirement 1.1
// ============================================================
TEST_F(ExplicitValTest, StructWithExplicitValFields) {
    std::string source = R"(
        struct Point {
            val x: float
            val y: float
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();

    ASSERT_EQ(def.fields.size(), 2u);
    EXPECT_EQ(def.fields[0].name.name, "x");
    EXPECT_FALSE(def.fields[0].is_mutable);
    EXPECT_TRUE(def.fields[0].has_explicit_val);

    EXPECT_EQ(def.fields[1].name.name, "y");
    EXPECT_FALSE(def.fields[1].is_mutable);
    EXPECT_TRUE(def.fields[1].has_explicit_val);
}

// ============================================================
// 2. Struct with var fields
//    Validates: Requirement 1.2
// ============================================================
TEST_F(ExplicitValTest, StructWithVarFields) {
    std::string source = R"(
        struct Counter {
            var count: int
            var label: string
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();

    ASSERT_EQ(def.fields.size(), 2u);
    EXPECT_TRUE(def.fields[0].is_mutable);
    EXPECT_FALSE(def.fields[0].has_explicit_val);

    EXPECT_TRUE(def.fields[1].is_mutable);
    EXPECT_FALSE(def.fields[1].has_explicit_val);
}

// ============================================================
// 3. Struct with no keyword (bare declaration — now produces error)
//    Validates: Requirement 1.3 (mandatory-val-var-annotations)
// ============================================================
TEST_F(ExplicitValTest, StructWithNoKeyword) {
    std::string source = R"(
        struct Pair {
            first: int
            second: int
        }
    )";

    std::vector<expression> results;
    // parse_source returns false because annotation errors exist
    ASSERT_FALSE(parse_source(source, results));

    // Use recovery parser to get AST + errors
    results.clear();
    ASSERT_TRUE(parse_source_with_recovery(source, results));
    ASSERT_EQ(errors_.size(), 2u);
    EXPECT_NE(errors_[0].find("Missing mutability annotation"), std::string::npos);
    EXPECT_NE(errors_[1].find("Missing mutability annotation"), std::string::npos);

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();

    ASSERT_EQ(def.fields.size(), 2u);
    // Recovery: treated as immutable, not explicit
    EXPECT_FALSE(def.fields[0].is_mutable);
    EXPECT_FALSE(def.fields[0].has_explicit_val);
    EXPECT_FALSE(def.fields[1].is_mutable);
    EXPECT_FALSE(def.fields[1].has_explicit_val);
}

// ============================================================
// 4. Struct with mixed val/var/bare fields
//    Validates: Requirements 1.1, 1.2, 1.3 (mandatory-val-var-annotations)
// ============================================================
TEST_F(ExplicitValTest, StructWithMixedFields) {
    std::string source = R"(
        struct Config {
            val version: int
            api_key: string
            var retry_count: int
        }
    )";

    std::vector<expression> results;
    // Has one bare field → annotation error
    ASSERT_TRUE(parse_source_with_recovery(source, results));
    ASSERT_EQ(errors_.size(), 1u);
    EXPECT_NE(errors_[0].find("Missing mutability annotation"), std::string::npos);

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();

    ASSERT_EQ(def.fields.size(), 3u);

    // val version: explicit val
    EXPECT_EQ(def.fields[0].name.name, "version");
    EXPECT_FALSE(def.fields[0].is_mutable);
    EXPECT_TRUE(def.fields[0].has_explicit_val);

    // api_key: bare → error, recovered as immutable
    EXPECT_EQ(def.fields[1].name.name, "api_key");
    EXPECT_FALSE(def.fields[1].is_mutable);
    EXPECT_FALSE(def.fields[1].has_explicit_val);

    // var retry_count: mutable
    EXPECT_EQ(def.fields[2].name.name, "retry_count");
    EXPECT_TRUE(def.fields[2].is_mutable);
    EXPECT_FALSE(def.fields[2].has_explicit_val);
}

// ============================================================
// 5. Class with explicit val fields
//    Validates: Requirement 2.1
// ============================================================
TEST_F(ExplicitValTest, ClassWithExplicitValFields) {
    std::string source = R"(
        class Color {
            val r: int
            val g: int
            val b: int
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* cd = boost::get<boost::spirit::x3::forward_ast<class_definition>>(&results[0]);
    ASSERT_NE(cd, nullptr);
    const auto& def = cd->get();

    ASSERT_EQ(def.fields.size(), 3u);
    for (const auto& field : def.fields) {
        EXPECT_FALSE(field.is_mutable);
        EXPECT_TRUE(field.has_explicit_val);
    }
}

// ============================================================
// 6. C#-style property with explicit val and getter is rejected
//    Validates: Requirements 17.6, 17.7
// ============================================================
TEST_F(ExplicitValTest, PropertyWithExplicitValAndGetterRejected) {
    std::string source = R"(
        class Rectangle {
            val width: float
            val height: float

            val area: float {
                get { width * height }
            }
        }
    )";

    std::vector<expression> results;
    ASSERT_FALSE(parse_source(source, results));
    EXPECT_NE(last_error_.find("C#-style property syntax is not supported in Meld"), std::string::npos)
        << "Actual error: " << last_error_;
    EXPECT_NE(last_error_.find("@Property"), std::string::npos)
        << "Actual error: " << last_error_;
}

// ============================================================
// 7. Function with val parameters
//    Validates: Requirement 3.1
// ============================================================
TEST_F(ExplicitValTest, FunctionWithValParameters) {
    std::string source = R"(
        fn add(val a: int, val b: int) -> int {
            return a + b
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 2u);
    EXPECT_EQ(def.parameters[0].name.name, "a");
    EXPECT_TRUE(def.parameters[0].has_explicit_val);

    EXPECT_EQ(def.parameters[1].name.name, "b");
    EXPECT_TRUE(def.parameters[1].has_explicit_val);
}

// ============================================================
// 8. Function with no keyword parameters (bare — now produces error)
//    Validates: Requirement 3.3 (mandatory-val-var-annotations)
// ============================================================
TEST_F(ExplicitValTest, FunctionWithNoKeywordParameters) {
    std::string source = R"(
        fn greet(name: string) -> string {
            return name
        }
    )";

    std::vector<expression> results;
    // Has one bare parameter → annotation error
    ASSERT_TRUE(parse_source_with_recovery(source, results));
    ASSERT_EQ(errors_.size(), 1u);
    EXPECT_NE(errors_[0].find("Missing mutability annotation"), std::string::npos);

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_EQ(def.parameters[0].name.name, "name");
    EXPECT_FALSE(def.parameters[0].has_explicit_val);
}

// ============================================================
// 9. Error: val var on same declaration (struct field)
//    Validates: Requirements 6.1, 6.3
// ============================================================
TEST_F(ExplicitValTest, ErrorValVarOnField) {
    std::string source = R"(
        struct Bad {
            val var x: int
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("Cannot use both 'val' and 'var' on the same declaration"),
              std::string::npos)
        << "Actual error: " << last_error_;
}

// ============================================================
// 10. Error: var val on same declaration (struct field)
//     Validates: Requirements 6.1, 6.3
// ============================================================
TEST_F(ExplicitValTest, ErrorVarValOnField) {
    std::string source = R"(
        struct Bad {
            var val x: int
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("Cannot use both 'val' and 'var' on the same declaration"),
              std::string::npos)
        << "Actual error: " << last_error_;
}

// ============================================================
// 11. Warning: val + @const on parameter (redundancy)
//     Validates: Requirement 6.2
// ============================================================
TEST_F(ExplicitValTest, WarningValConstOnParameter) {
    std::string source = R"(
        fn process(val @const x: int) -> int {
            return x
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results))
        << "Parse failed: " << last_error_;

    ASSERT_FALSE(warnings_.empty()) << "Expected a redundancy warning";
    EXPECT_NE(warnings_[0].find("Redundant annotation: 'val' and '@const' both indicate immutability"),
              std::string::npos)
        << "Actual warning: " << warnings_[0];
}

// ============================================================
// 12. Error: val + @mut on parameter (conflicting)
//     Validates: Requirement 6.2
// ============================================================
TEST_F(ExplicitValTest, ErrorValMutOnParameter) {
    std::string source = R"(
        fn process(val @mut x: int) -> int {
            return x
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("Conflicting mutability annotations: 'val' and '@mut' cannot be combined"),
              std::string::npos)
        << "Actual error: " << last_error_;
}

// ============================================================
// 13. Edge case: single-field struct with val
//     Validates: Requirement 1.1
// ============================================================
TEST_F(ExplicitValTest, SingleFieldStructWithVal) {
    std::string source = R"(
        struct Wrapper {
            val value: int
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
    ASSERT_NE(sd, nullptr);
    const auto& def = sd->get();

    ASSERT_EQ(def.fields.size(), 1u);
    EXPECT_EQ(def.fields[0].name.name, "value");
    EXPECT_FALSE(def.fields[0].is_mutable);
    EXPECT_TRUE(def.fields[0].has_explicit_val);
}

// ============================================================
// 14. Edge case: single-parameter function with val
//     Validates: Requirement 3.1
// ============================================================
TEST_F(ExplicitValTest, SingleParameterFunctionWithVal) {
    std::string source = R"(
        fn identity(val x: int) -> int {
            return x
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_EQ(def.parameters[0].name.name, "x");
    EXPECT_TRUE(def.parameters[0].has_explicit_val);
}

// ============================================================
// Error: val var on function parameter
//    Validates: Requirements 6.1, 6.3
// ============================================================
TEST_F(ExplicitValTest, ErrorValVarOnParameter) {
    std::string source = R"(
        fn bad(val var x: int) -> int {
            return x
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("Cannot use both 'val' and 'var' on the same declaration"),
              std::string::npos)
        << "Actual error: " << last_error_;
}
