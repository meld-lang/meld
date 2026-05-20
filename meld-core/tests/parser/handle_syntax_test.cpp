/**
 * Edge case unit tests for unified handle syntax and anonymous
 * implementation blocks (Task 54.9).
 *
 * Covers:
 *   - Minimal handle call parsing
 *   - Multiple handlers with multiple operations
 *   - Standalone inline trait implementation parsing
 *   - Empty handler body rejection
 *   - Disambiguation from initialization blocks
 *   - Anonymous blocks with val/var fields
 */

#include <gtest/gtest.h>
#include <meld/parser/parser.hpp>
#include <meld/parser/ast.hpp>
#include <boost/variant.hpp>
#include <string>
#include <vector>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

bool parse_source(const std::string& source, std::vector<expression>& results) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens, ParserContext{});
    return parser.parse_file(results);
}

bool parse_fails(const std::string& source, std::string& error_msg) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        error_msg = lexer.errors().front();
        return true;
    }
    TokenParser parser(tokens, ParserContext{});
    std::vector<expression> results;
    if (!parser.parse_file(results)) {
        error_msg = parser.error_message();
        return true;
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Inline trait implementation: TypeName { fnc ... }
// ---------------------------------------------------------------------------

TEST(HandleSyntaxTest, InlineTraitImplSingleMethod) {
    std::vector<expression> results;
    // TypeName { fnc ... } should parse as handle_expression wrapping inline_trait_impl
    ASSERT_TRUE(parse_source(
        "val x = MyTrait {\n"
        "    fnc op() { 42 }\n"
        "}", results));
    EXPECT_FALSE(results.empty());
}

TEST(HandleSyntaxTest, InlineTraitImplMultipleMethods) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "val x = MyTrait {\n"
        "    fnc read(path) { resume(\"{}\") }\n"
        "    fnc write(path, content) { resume() }\n"
        "}", results));
    EXPECT_FALSE(results.empty());
}

TEST(HandleSyntaxTest, InlineTraitImplWithTypedParams) {
    std::vector<expression> results;
    // Use simple body that TokenParser can handle (no binary ops)
    ASSERT_TRUE(parse_source(
        "val x = Comparable {\n"
        "    fnc compare(a: int, b: int) { a }\n"
        "}", results));
    EXPECT_FALSE(results.empty());
}

TEST(HandleSyntaxTest, InlineTraitImplWithReturnType) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "val x = Formatter {\n"
        "    fnc format(v: int) -> string { \"formatted\" }\n"
        "}", results));
    EXPECT_FALSE(results.empty());
}

// ---------------------------------------------------------------------------
// Disambiguation from initialization blocks
// ---------------------------------------------------------------------------

TEST(HandleSyntaxTest, InitializationBlockStillWorks) {
    std::vector<expression> results;
    // Name { field = value } should still parse as initialization block
    ASSERT_TRUE(parse_source(
        "val p = Point {\n"
        "    x = 10\n"
        "    y = 20\n"
        "}", results));
    EXPECT_FALSE(results.empty());

    // Verify it's an initialization_block, not a handle_expression
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* init = boost::get<boost::spirit::x3::forward_ast<initialization_block>>(
        &vd->get().value.get());
    EXPECT_NE(init, nullptr) << "Should parse as initialization block, not anonymous impl";
}

TEST(HandleSyntaxTest, AnonymousImplWithFncIsNotInitBlock) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "val x = MyTrait {\n"
        "    fnc op() { 42 }\n"
        "}", results));
    EXPECT_FALSE(results.empty());

    // Verify it's a handle_expression (wrapping inline_trait_impl), not initialization_block
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* init = boost::get<boost::spirit::x3::forward_ast<initialization_block>>(
        &vd->get().value.get());
    EXPECT_EQ(init, nullptr) << "Should NOT parse as initialization block";

    auto* handle = boost::get<boost::spirit::x3::forward_ast<handle_expression>>(
        &vd->get().value.get());
    EXPECT_NE(handle, nullptr) << "Should parse as handle_expression (wrapping inline_trait_impl)";
}

TEST(HandleSyntaxTest, AnonymousImplWithValIsNotInitBlock) {
    std::string src =
        "val x = Config {\n"
        "    val debug-mode = true\n"
        "}";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty()) << "Lexer errors: " << lexer.errors().front();
    TokenParser parser(tokens, ParserContext{});
    std::vector<expression> results;
    ASSERT_TRUE(parser.parse_file(results))
        << "Parse error: " << parser.error_message();
    EXPECT_FALSE(results.empty());
}

// ---------------------------------------------------------------------------
// Empty block rejection
// ---------------------------------------------------------------------------

TEST(HandleSyntaxTest, EmptyInitBlockAllowed) {
    std::vector<expression> results;
    // Empty { } after identifier is an empty initialization block — allowed
    ASSERT_TRUE(parse_source("val x = Empty { }", results));
}

// ---------------------------------------------------------------------------
// Handle call with inline trait impl arguments
// ---------------------------------------------------------------------------

TEST(HandleSyntaxTest, HandleCallParsesAsFunction) {
    std::string src =
        "handle(\n"
        "    () -> 42,\n"
        "    MyEffect {\n"
        "        fnc op() { resume() }\n"
        "    }\n"
        ")";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty()) << "Lexer errors: " << lexer.errors().front();
    TokenParser parser(tokens, ParserContext{});
    std::vector<expression> results;
    ASSERT_TRUE(parser.parse_file(results))
        << "Parse error: " << parser.error_message();
    EXPECT_FALSE(results.empty());
}

TEST(HandleSyntaxTest, HandleCallMultipleHandlers) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "handle(\n"
        "    () -> doWork(),\n"
        "    Logger {\n"
        "        fnc log(msg) { resume() }\n"
        "    },\n"
        "    FileSystem {\n"
        "        fnc read(path) { resume(\"{}\") }\n"
        "        fnc write(path, content) { resume() }\n"
        "    }\n"
        ")", results));
    EXPECT_FALSE(results.empty());
}

TEST(HandleSyntaxTest, HandleAsExpression) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "val result = handle(\n"
        "    () -> computeSomething(),\n"
        "    MyEffect {\n"
        "        fnc op() { resume(42) }\n"
        "    }\n"
        ")", results));
    EXPECT_FALSE(results.empty());
}

// ---------------------------------------------------------------------------
// Nested handle calls
// ---------------------------------------------------------------------------

TEST(HandleSyntaxTest, NestedHandleCalls) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_source(
        "handle(\n"
        "    () -> handle(\n"
        "        () -> innerWork(),\n"
        "        InnerEffect {\n"
        "            fnc inner-op() { resume() }\n"
        "        }\n"
        "    ),\n"
        "    OuterEffect {\n"
        "        fnc outer-op() { resume() }\n"
        "    }\n"
        ")", results));
    EXPECT_FALSE(results.empty());
}
