#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// Tests for Task 44: Mutating Methods (var fnc) and Mutable Parameters (var)
class MutatingMethodTest : public ::testing::Test {
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
        return true;
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

    bool parse_fails(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) {
            last_error_ = lexer.errors().front();
            return true;
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
};

// ============================================================
// 44.1: var fnc â€” Mutating Method Syntax
//    Validates: Requirements 57.1, 57.2
// ============================================================

TEST_F(MutatingMethodTest, VarFncParsesAsMutatingMethod) {
    std::string source = R"(
        var fnc increment() {
            count = count + 1
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));
    ASSERT_EQ(results.size(), 1u);

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "increment");
    EXPECT_TRUE(def.is_mutating);
    EXPECT_EQ(def.parameters.size(), 0u);
}

TEST_F(MutatingMethodTest, VarFncWithReturnType) {
    std::string source = R"(
        var fnc pop() -> int {
            rtn items.removeLast()
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "pop");
    EXPECT_TRUE(def.is_mutating);
    EXPECT_TRUE(def.has_return_type);
    EXPECT_EQ(def.return_type.type_name.name, "int");
}

TEST_F(MutatingMethodTest, VarFncWithParameters) {
    std::string source = R"(
        var fnc add(val item: string) {
            items.append(item)
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "add");
    EXPECT_TRUE(def.is_mutating);
    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_EQ(def.parameters[0].name.name, "item");
}

TEST_F(MutatingMethodTest, RegularFncIsNotMutating) {
    std::string source = R"(
        fnc current() -> int {
            rtn count
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "current");
    EXPECT_FALSE(def.is_mutating);
}

TEST_F(MutatingMethodTest, VarFncWithEffectsClause) {
    std::string source = R"(
        var fnc save() effects { FileSystem } {
            FileSystem.write(path, data)
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "save");
    EXPECT_TRUE(def.is_mutating);
    EXPECT_TRUE(def.has_effects);
    ASSERT_EQ(def.effects_clause.size(), 1u);
    EXPECT_EQ(def.effects_clause[0].name, "FileSystem");
}

TEST_F(MutatingMethodTest, VarFncWithContracts) {
    std::string source = R"(
        var fnc withdraw(val amount: int)
        require { amount > 0 }
        ensure { balance >= 0 }
        {
            balance = balance - amount
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "withdraw");
    EXPECT_TRUE(def.is_mutating);
    EXPECT_TRUE(def.has_contracts);
}

TEST_F(MutatingMethodTest, VarWithoutFncIsVarDeclaration) {
    // 'var x = 5' should still parse as a var declaration, not a mutating method
    std::string source = R"(
        var x = 5
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* vd = boost::get<boost::spirit::x3::forward_ast<var_declaration>>(&results[0]);
    ASSERT_NE(vd, nullptr);
    EXPECT_EQ(vd->get().name.name, "x");
}

// ============================================================
// 44.2: var Parameter Modifier
//    Validates: Requirements 57.4, 57.5
// ============================================================

TEST_F(MutatingMethodTest, VarParameterModifier) {
    std::string source = R"(
        fnc normalize(user: var User) {
            user.name = user.name.trim()
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_EQ(def.name.name, "normalize");
    EXPECT_FALSE(def.is_mutating);
    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_EQ(def.parameters[0].name.name, "user");
    EXPECT_TRUE(def.parameters[0].is_mutable);
    EXPECT_EQ(def.parameters[0].type.type_name.name, "User");
}

TEST_F(MutatingMethodTest, DefaultParameterIsImmutable) {
    std::string source = R"(
        fnc greet(val user: User) -> string {
            rtn user.name
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_FALSE(def.parameters[0].is_mutable);
    EXPECT_FALSE(def.parameters[0].has_explicit_val);
}

TEST_F(MutatingMethodTest, MixedMutableAndImmutableParameters) {
    std::string source = R"(
        fnc transfer(from: var Account, to: var Account, val amount: int) {
            from.balance = from.balance - amount
            to.balance = to.balance + amount
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 3u);
    EXPECT_TRUE(def.parameters[0].is_mutable);   // from: var Account
    EXPECT_TRUE(def.parameters[1].is_mutable);   // to: var Account
    EXPECT_FALSE(def.parameters[2].is_mutable);  // amount: int (default immutable)
}

TEST_F(MutatingMethodTest, VarParameterWithNullableType) {
    std::string source = R"(
        fnc clear(user: var User?) {
            user = nil
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_TRUE(def.parameters[0].is_mutable);
    EXPECT_TRUE(def.parameters[0].type.is_nullable);
    EXPECT_EQ(def.parameters[0].type.type_name.name, "User");
}

TEST_F(MutatingMethodTest, VarParameterWithGenericType) {
    std::string source = R"(
        fnc sort(items: var List[int]) {
            items = items.sorted()
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_TRUE(def.parameters[0].is_mutable);
    EXPECT_EQ(def.parameters[0].type.type_name.name, "List");
    EXPECT_TRUE(def.parameters[0].type.has_type_arguments);
}

TEST_F(MutatingMethodTest, VarFncWithVarParameter) {
    // Both var fnc and var parameter together
    std::string source = R"(
        var fnc merge(other: var Collection) {
            items.addAll(other.items)
            other.clear()
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));

    auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
    ASSERT_NE(fd, nullptr);
    const auto& def = fd->get();

    EXPECT_TRUE(def.is_mutating);
    ASSERT_EQ(def.parameters.size(), 1u);
    EXPECT_TRUE(def.parameters[0].is_mutable);
}

// ============================================================
// Conflict detection tests
// ============================================================

TEST_F(MutatingMethodTest, ErrorValVarOnParameter) {
    // val + var on same parameter should fail
    std::string source = R"(
        fnc update(val user: var User) {
            user.name = "x"
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("Cannot use both 'val' and 'var'"), std::string::npos);
}

TEST_F(MutatingMethodTest, ErrorVarConstOnParameter) {
    // var + @const on same parameter should fail
    std::string source = R"(
        fnc update(@const user: var User) {
            user.name = "x"
        }
    )";

    ASSERT_TRUE(parse_fails(source));
    EXPECT_NE(last_error_.find("'var' and '@const'"), std::string::npos);
}

TEST_F(MutatingMethodTest, WarningVarMutOnParameter) {
    // var + @mut on same parameter should produce a warning (redundant)
    std::string source = R"(
        fnc update(@mut user: var User) {
            user.name = "x"
        }
    )";

    std::vector<expression> results;
    ASSERT_TRUE(parse_source(source, results));
    ASSERT_FALSE(warnings_.empty());
    EXPECT_NE(warnings_[0].find("Redundant"), std::string::npos);
}
