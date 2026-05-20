/**
 * Edge case unit tests for interpolation modifiers (Task 55.9, 56.8).
 *
 * Covers:
 *   - Template strings (`...`) with ${:debug expr}, ${expr}
 *   - Static strings ("...") do NOT evaluate interpolation
 *   - Undefined modifier produces error
 *   - Mixed interpolations, multi-line template strings
 *   - User-defined custom modifier functions
 *   - Static vs template string distinction (Task 56)
 *   - Compiler warning when ${} appears in static strings
 */

#include <gtest/gtest.h>
#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/parser/parser.hpp>
#include <meld/parser/ast.hpp>
#include <meld/kernel/primitives.hpp>
#include <boost/variant.hpp>
#include <string>
#include <vector>

using namespace meld::interpreter;
using namespace meld::parser;
using namespace meld::parser::ast;
using namespace meld::kernel;

namespace {

// Backtick character for building template string source
constexpr char BT = '`';

// Build a template string source: val s = `content`
std::string tmpl(const std::string& content) {
    return std::string("val s = ") + BT + content + BT;
}

// Build a multi-line template string source: val s = ```content```
std::string tmpl3(const std::string& content) {
    return std::string("val s = ") + BT + BT + BT + content + BT + BT + BT;
}

/// Parse and evaluate source, returning the result Value.
Value eval(const std::string& source, AstInterpreter& interp) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    TokenParser parser(tokens, ParserContext{});
    std::vector<expression> results;
    if (!parser.parse_file(results)) {
        throw std::runtime_error("Parse failed: " + parser.error_message());
    }
    return interp.evaluate_program(results);
}

/// Parse source into expressions.
bool parse_to_exprs(const std::string& source, std::vector<expression>& results) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens, ParserContext{});
    return parser.parse_file(results);
}

/// Parse source and capture warnings.
bool parse_with_warnings(const std::string& source,
                         std::vector<expression>& results,
                         std::vector<std::string>& warnings) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    warnings = lexer.warnings();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens, ParserContext{});
    return parser.parse_file(results);
}

} // anonymous namespace

class InterpolationEdgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = std::make_shared<Environment>();
        interp_ = std::make_unique<AstInterpreter>(env_);
    }

    std::shared_ptr<Environment> env_;
    std::unique_ptr<AstInterpreter> interp_;
};

// ===========================================================================
// Template strings: ${:debug expr} parses and desugars correctly
// ===========================================================================

TEST_F(InterpolationEdgeTest, DebugModifierWithInteger) {
    auto result = eval(tmpl("${:debug 42}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "int(42)");
}

TEST_F(InterpolationEdgeTest, DebugModifierWithString) {
    env_->define("name", Value(std::make_shared<String>("Alice")), false);
    auto result = eval(tmpl("${:debug name}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "string(\"Alice\")");
}

TEST_F(InterpolationEdgeTest, DebugModifierWithBoolean) {
    env_->define("flag", Value(Boolean::from(true)), false);
    auto result = eval(tmpl("${:debug flag}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "bool(true)");
}

TEST_F(InterpolationEdgeTest, PrettyModifierWithVec) {
    auto vec = std::make_shared<Vec>();
    vec->push_back(Value(std::make_shared<Integer>(1)));
    vec->push_back(Value(std::make_shared<Integer>(2)));
    env_->define("v", Value(vec), false);
    auto result = eval(tmpl("${:pretty v}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    std::string expected =
        "vec([\n"
        "  int(1),\n"
        "  int(2)\n"
        "])";
    EXPECT_EQ(result.as<String>()->value(), expected);
}

// ===========================================================================
// Template strings: ${expr} without modifier (default interpolation)
// ===========================================================================

TEST_F(InterpolationEdgeTest, DefaultInterpolationInteger) {
    auto result = eval(tmpl("value: ${42}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "value: 42");
}

TEST_F(InterpolationEdgeTest, DefaultInterpolationString) {
    env_->define("name", Value(std::make_shared<String>("Bob")), false);
    auto result = eval(tmpl("hello ${name}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "hello Bob");
}

TEST_F(InterpolationEdgeTest, DefaultInterpolationBoolean) {
    env_->define("flag", Value(Boolean::from(false)), false);
    auto result = eval(tmpl("is: ${flag}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "is: false");
}

// ===========================================================================
// Undefined modifier produces error
// ===========================================================================

TEST_F(InterpolationEdgeTest, UndefinedModifierThrowsError) {
    EXPECT_THROW({
        eval(tmpl("${:nonexistent 42}"), *interp_);
    }, InterpreterError);
}

TEST_F(InterpolationEdgeTest, UndefinedModifierErrorMentionsModifierName) {
    try {
        eval(tmpl("${:nonexistent 42}"), *interp_);
        FAIL() << "Expected InterpreterError";
    } catch (const InterpreterError& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("nonexistent"), std::string::npos)
            << "Error should mention the modifier name, got: " << msg;
        EXPECT_NE(msg.find("__interpolate-nonexistent__"), std::string::npos)
            << "Error should mention the function name, got: " << msg;
    }
}

// ===========================================================================
// Mixed default and modifier interpolations in one template string
// ===========================================================================

TEST_F(InterpolationEdgeTest, MixedDefaultAndModifierInOneString) {
    env_->define("x", Value(std::make_shared<Integer>(10)), false);
    auto result = eval(tmpl("val=${x} dbg=${:debug x}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "val=10 dbg=int(10)");
}

TEST_F(InterpolationEdgeTest, MultipleModifiersInOneString) {
    env_->define("x", Value(std::make_shared<Integer>(5)), false);
    auto result = eval(tmpl("${:debug x} and ${:debug x}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "int(5) and int(5)");
}

// ===========================================================================
// Modifier with variable reference
// ===========================================================================

TEST_F(InterpolationEdgeTest, DebugModifierWithVariable) {
    env_->define("x", Value(std::make_shared<Integer>(3)), false);
    auto result = eval(tmpl("${:debug x}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "int(3)");
}

// ===========================================================================
// Multi-line template strings (```)
// ===========================================================================

TEST_F(InterpolationEdgeTest, ModifierInMultilineTemplateString) {
    env_->define("x", Value(std::make_shared<Integer>(99)), false);
    auto result = eval(tmpl3("line1\n${:debug x}\nline3"), *interp_);
    ASSERT_TRUE(result.is<String>());
    std::string val = result.as<String>()->value();
    EXPECT_NE(val.find("int(99)"), std::string::npos)
        << "Multi-line template should contain debug output, got: " << val;
}

// ===========================================================================
// User-defined custom modifier
// ===========================================================================

TEST_F(InterpolationEdgeTest, UserDefinedModifierFunction) {
    auto params = std::vector<std::shared_ptr<Symbol>>{
        std::make_shared<Symbol>("value")};
    Function::NativeImpl impl =
        [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return Value(std::make_shared<String>(""));
            std::string s;
            if (args[0].is<String>()) {
                s = args[0].as<String>()->value();
            } else {
                s = args[0].to_string();
            }
            for (auto& c : s) c = static_cast<char>(std::toupper(c));
            return Value(std::make_shared<String>(s));
        };
    env_->define("__interpolate-upper__",
        Value(std::make_shared<Function>(
            std::move(params), Value{},
            std::move(impl), "__interpolate-upper__")),
        false);

    env_->define("msg", Value(std::make_shared<String>("hello")), false);
    auto result = eval(tmpl("${:upper msg}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "HELLO");
}

// ===========================================================================
// Literal text around interpolations preserved
// ===========================================================================

TEST_F(InterpolationEdgeTest, LiteralTextPreservedAroundInterpolation) {
    auto result = eval(tmpl("prefix-${:debug 1}-suffix"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "prefix-int(1)-suffix");
}

TEST_F(InterpolationEdgeTest, EmptyStringAroundInterpolation) {
    auto result = eval(tmpl("${:debug 7}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "int(7)");
}

// ===========================================================================
// Parser flags on template strings
// ===========================================================================

TEST_F(InterpolationEdgeTest, TemplateStringWithInterpolationHasFlags) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs(tmpl("${42}"), results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->has_interpolation);
    EXPECT_FALSE(lit->has_modifier_interpolation);
    EXPECT_TRUE(lit->is_template);
}

TEST_F(InterpolationEdgeTest, TemplateStringWithModifierHasBothFlags) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs(tmpl("${:debug 42}"), results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->has_interpolation);
    EXPECT_TRUE(lit->has_modifier_interpolation);
    EXPECT_TRUE(lit->is_template);
}

TEST_F(InterpolationEdgeTest, PlainTemplateStringHasNoInterpolationFlags) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs(tmpl("no interpolation here"), results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_FALSE(lit->has_interpolation);
    EXPECT_TRUE(lit->is_template);
}

// ===========================================================================
// Nil value handling
// ===========================================================================

TEST_F(InterpolationEdgeTest, DebugModifierWithNil) {
    env_->define("n", Value(Empty::instance()), false);
    auto result = eval(tmpl("${:debug n}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "nil");
}

TEST_F(InterpolationEdgeTest, DefaultModifierWithNil) {
    env_->define("n", Value(Empty::instance()), false);
    auto result = eval(tmpl("${n}"), *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_FALSE(result.as<String>()->value().empty());
}

// ===========================================================================
// Task 56: Static vs Template string distinction
// ===========================================================================

TEST_F(InterpolationEdgeTest, StaticStringDoesNotEvaluateInterpolation) {
    // "hello ${name}" should produce literal text with ${name} unevaluated
    env_->define("name", Value(std::make_shared<String>("Alice")), false);
    auto result = eval("val s = \"hello ${name}\"", *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "hello ${name}");
}

TEST_F(InterpolationEdgeTest, StaticStringHasNoInterpolationFlags) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs("val s = \"plain text\"", results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_FALSE(lit->has_interpolation);
    EXPECT_FALSE(lit->is_template);
}

TEST_F(InterpolationEdgeTest, StaticStringWithDollarBraceIsLiteral) {
    // Even with ${...} syntax, static strings treat it as literal text
    std::vector<expression> results;
    std::vector<std::string> warnings;
    ASSERT_TRUE(parse_with_warnings("val s = \"${42}\"", results, warnings));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_FALSE(lit->has_interpolation);
    EXPECT_FALSE(lit->is_template);
    // Should produce a warning
    ASSERT_FALSE(warnings.empty());
    EXPECT_NE(warnings[0].find("template string"), std::string::npos)
        << "Warning should suggest template string, got: " << warnings[0];
}

TEST_F(InterpolationEdgeTest, EscapedDollarInStaticStringNoWarning) {
    // "\${name}" should not produce a warning
    std::vector<expression> results;
    std::vector<std::string> warnings;
    ASSERT_TRUE(parse_with_warnings("val s = \"\\${name}\"", results, warnings));
    EXPECT_TRUE(warnings.empty()) << "Escaped $ should not produce warning";
}

TEST_F(InterpolationEdgeTest, StaticMultilineStringDoesNotEvaluate) {
    env_->define("x", Value(std::make_shared<Integer>(42)), false);
    auto result = eval("val s = \"\"\"hello ${x} world\"\"\"", *interp_);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "hello ${x} world");
}

TEST_F(InterpolationEdgeTest, TemplateStringIsTemplate) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs(tmpl("hello"), results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->is_template);
    EXPECT_FALSE(lit->is_multiline);
}

TEST_F(InterpolationEdgeTest, MultilineTemplateStringIsTemplateAndMultiline) {
    std::vector<expression> results;
    ASSERT_TRUE(parse_to_exprs(tmpl3("hello\nworld"), results));
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    auto* lit = boost::get<string_literal>(&vd->get().value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->is_template);
    EXPECT_TRUE(lit->is_multiline);
}
