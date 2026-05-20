/**
 * Property-style tests for interpolation modifier desugaring (Task 55.8).
 *
 * Uses parameterized GTest cases across modifier names and expressions.
 * All interpolation tests use backtick template strings (`...`).
 *
 * Properties verified:
 *   - Default desugaring: ${expr} → __interpolate-default__(expr)
 *   - Modifier desugaring: ${:sym expr} → __interpolate-sym__(expr)
 *   - Naming convention: function name is always __interpolate-<sym>__
 *   - No compiler knowledge: identical AST structure regardless of modifier symbol
 *   - Backward compatibility: template strings without modifiers work
 *   - Undefined modifier produces runtime error mentioning the modifier name
 */

#include <gtest/gtest.h>
#include <meld/parser/parser.hpp>
#include <meld/parser/ast.hpp>
#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/kernel/primitives.hpp>
#include <boost/variant.hpp>
#include <string>
#include <vector>

using namespace meld::parser;
using namespace meld::parser::ast;
using namespace meld::interpreter;
using namespace meld::kernel;

namespace {

constexpr char BT = '`';

// Build template string source: val s = `content`
std::string tmpl(const std::string& content) {
    return std::string("val s = ") + BT + content + BT;
}

const std::vector<std::string> kModifierNames = {
    "debug", "pretty", "hex", "json", "xml", "csv",
    "upper", "lower", "trim", "pad", "base64",
    "url-encode", "html-escape", "sql-safe",
    "left-pad", "right-pad", "zero-fill",
    "camel-case", "snake-case", "kebab-case"
};

const std::vector<int> kTestIntegers = {
    0, 1, 42, 100, 999, 7, 256, 1024, 65535
};

bool parse_source(const std::string& source, std::vector<expression>& results) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens, ParserContext{});
    return parser.parse_file(results);
}

const string_literal* extract_string_literal(const expression& expr) {
    auto* vd = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(&expr);
    if (!vd) return nullptr;
    const auto& val_expr = vd->get().value.get();
    return boost::get<string_literal>(&val_expr);
}

} // anonymous namespace

// ===========================================================================
// Property: Default desugaring — ${expr} calls __interpolate-default__
// ===========================================================================

class DefaultDesugaringTest : public ::testing::TestWithParam<int> {};

TEST_P(DefaultDesugaringTest, CallsDefaultFunction) {
    int n = GetParam();
    std::string source = tmpl("value is ${" + std::to_string(n) + "}");

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));
    ASSERT_FALSE(results.empty());

    auto* lit = extract_string_literal(results[0]);
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->has_interpolation);
    EXPECT_TRUE(lit->is_template);

    auto env = std::make_shared<Environment>();
    AstInterpreter interp(env);
    auto result = interp.evaluate_program(results);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "value is " + std::to_string(n));
}

INSTANTIATE_TEST_SUITE_P(Integers, DefaultDesugaringTest, ::testing::ValuesIn(kTestIntegers));

// ===========================================================================
// Property: Modifier desugaring — ${:sym expr} calls __interpolate-sym__
// ===========================================================================

class ModifierDesugaringTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ModifierDesugaringTest, CallsNamedFunction) {
    const auto& modifier = GetParam();

    for (int n : {0, 42, 7, 999}) {
        std::string source = tmpl("${:" + modifier + " " + std::to_string(n) + "}");

        std::vector<expression> results;
        ASSERT_TRUE(parse_source(source, results)) << "Failed to parse with modifier: " << modifier;

        auto* lit = extract_string_literal(results[0]);
        ASSERT_NE(lit, nullptr);
        EXPECT_TRUE(lit->has_interpolation);
        EXPECT_TRUE(lit->has_modifier_interpolation);
        EXPECT_TRUE(lit->is_template);

        auto env = std::make_shared<Environment>();
        AstInterpreter interp(env);

        std::string func_name = "__interpolate-" + modifier + "__";
        auto params = std::vector<std::shared_ptr<Symbol>>{
            std::make_shared<Symbol>("value")};
        Function::NativeImpl impl =
            [](const std::vector<Value>& args) -> Value {
                if (args.empty()) return Value(std::make_shared<String>(""));
                return Value(std::make_shared<String>("MOD:" + args[0].to_string()));
            };
        env->define(func_name,
            Value(std::make_shared<Function>(
                std::move(params), Value{},
                std::move(impl), func_name)),
            false);

        auto result = interp.evaluate_program(results);
        ASSERT_TRUE(result.is<String>());
        EXPECT_EQ(result.as<String>()->value(), "MOD:" + std::to_string(n))
            << "Modifier: " << modifier << ", n: " << n;
    }
}

INSTANTIATE_TEST_SUITE_P(Modifiers, ModifierDesugaringTest, ::testing::ValuesIn(kModifierNames));

// ===========================================================================
// Property: Naming convention — __interpolate-<sym>__
// ===========================================================================

class NamingConventionTest : public ::testing::TestWithParam<std::string> {};

TEST_P(NamingConventionTest, FunctionNameFollowsConvention) {
    const auto& modifier = GetParam();
    std::string source = tmpl("${:" + modifier + " 42}");

    auto env = std::make_shared<Environment>();
    AstInterpreter interp(env);

    std::string expected_name = "__interpolate-" + modifier + "__";
    auto params = std::vector<std::shared_ptr<Symbol>>{
        std::make_shared<Symbol>("value")};
    Function::NativeImpl impl =
        [](const std::vector<Value>& args) -> Value {
            return Value(std::make_shared<String>("OK"));
        };
    env->define(expected_name,
        Value(std::make_shared<Function>(
            std::move(params), Value{},
            std::move(impl), expected_name)),
        false);

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto result = interp.evaluate_program(results);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "OK");
}

INSTANTIATE_TEST_SUITE_P(Modifiers, NamingConventionTest, ::testing::ValuesIn(kModifierNames));

// ===========================================================================
// Property: No compiler knowledge — identical AST structure for any modifier
// ===========================================================================

TEST(InterpolationModifierPropertyTest, IdenticalASTStructureForAnyModifier) {
    for (size_t i = 0; i < kModifierNames.size(); ++i) {
        for (size_t j = i + 1; j < kModifierNames.size(); ++j) {
            std::string source1 = tmpl("${:" + kModifierNames[i] + " 42}");
            std::string source2 = tmpl("${:" + kModifierNames[j] + " 42}");

            std::vector<expression> results1, results2;
            ASSERT_TRUE(parse_source(source1, results1));
            ASSERT_TRUE(parse_source(source2, results2));

            auto* lit1 = extract_string_literal(results1[0]);
            auto* lit2 = extract_string_literal(results2[0]);
            ASSERT_NE(lit1, nullptr);
            ASSERT_NE(lit2, nullptr);

            EXPECT_EQ(lit1->has_interpolation, lit2->has_interpolation);
            EXPECT_EQ(lit1->has_modifier_interpolation, lit2->has_modifier_interpolation);
            EXPECT_EQ(lit1->is_template, lit2->is_template);
            EXPECT_EQ(lit1->is_multiline, lit2->is_multiline);
        }
    }
}

// ===========================================================================
// Property: Backward compatibility — template strings without modifiers
// ===========================================================================

class BackwardCompatTest : public ::testing::TestWithParam<int> {};

TEST_P(BackwardCompatTest, NoModifierFlagWithoutModifier) {
    int n = GetParam();
    std::string source = tmpl("hello ${" + std::to_string(n) + "} world");

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* lit = extract_string_literal(results[0]);
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->has_interpolation);
    EXPECT_FALSE(lit->has_modifier_interpolation);
    EXPECT_TRUE(lit->is_template);
}

INSTANTIATE_TEST_SUITE_P(Integers, BackwardCompatTest, ::testing::ValuesIn(kTestIntegers));

// ===========================================================================
// Property: Plain template strings have no interpolation flags
// ===========================================================================

TEST(InterpolationModifierPropertyTest, PlainTemplateStringsHaveNoInterpolationFlags) {
    std::vector<std::string> texts = {
        "hello", "world", "", "a b c", "no dollars here",
        "just text 123", "special chars @#"
    };
    for (const auto& text : texts) {
        std::string source = tmpl(text);

        std::vector<expression> results;
        ASSERT_TRUE(parse_source(source, results));

        auto* lit = extract_string_literal(results[0]);
        ASSERT_NE(lit, nullptr);
        EXPECT_FALSE(lit->has_interpolation) << "Text: " << text;
        EXPECT_TRUE(lit->is_template) << "Text: " << text;
    }
}

// ===========================================================================
// Property: Static strings never have interpolation flags
// ===========================================================================

TEST(InterpolationModifierPropertyTest, StaticStringsNeverHaveInterpolationFlags) {
    std::vector<std::string> texts = {
        "hello", "world", "${name}", "${:debug x}", "a ${b} c"
    };
    for (const auto& text : texts) {
        std::string source = "val s = \"" + text + "\"";

        std::vector<expression> results;
        // May produce warnings for ${} in static strings, but should still parse
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        TokenParser parser(tokens, ParserContext{});
        ASSERT_TRUE(parser.parse_file(results));

        auto* lit = extract_string_literal(results[0]);
        ASSERT_NE(lit, nullptr);
        EXPECT_FALSE(lit->has_interpolation) << "Text: " << text;
        EXPECT_FALSE(lit->is_template) << "Text: " << text;
    }
}

// ===========================================================================
// Property: Undefined modifier produces runtime error
// ===========================================================================

class UndefinedModifierTest : public ::testing::TestWithParam<std::string> {};

TEST_P(UndefinedModifierTest, ProducesErrorWithModifierName) {
    const auto& modifier = GetParam();
    if (modifier == "debug" || modifier == "pretty" || modifier == "default") return;

    std::string source = tmpl("${:" + modifier + " 42}");

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto env = std::make_shared<Environment>();
    AstInterpreter interp(env);

    try {
        interp.evaluate_program(results);
        FAIL() << "Expected InterpreterError for undefined modifier: " << modifier;
    } catch (const InterpreterError& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find(modifier), std::string::npos)
            << "Error should mention modifier '" << modifier << "', got: " << msg;
    }
}

INSTANTIATE_TEST_SUITE_P(Modifiers, UndefinedModifierTest, ::testing::ValuesIn(kModifierNames));
