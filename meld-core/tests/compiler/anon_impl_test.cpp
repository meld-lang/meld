/**
 * Tests for anonymous implementation blocks (Task 57.5, 57.6).
 *
 * Validates Requirement 177:
 * - Name { fnc ... } parses as standalone expression
 * - Works in val binding
 * - (Trait1 & Trait2) { fnc ... } intersection syntax
 * - handle() handlers use same syntax
 */

#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

class AnonImplBlockTest : public ::testing::Test {
protected:
    Parser parser;

    bool parses(const std::string& input) {
        std::vector<ast::expression> result;
        return parser.parse_file(input, result) && !result.empty();
    }
};

TEST_F(AnonImplBlockTest, SimpleAnonImplParses) {
    EXPECT_TRUE(parses(R"(
        val x = Comparable {
            fnc compare(a: int, b: int) -> int {
                rtn 0
            }
        }
    )"));
}

TEST_F(AnonImplBlockTest, AnonImplWithMultipleMethods) {
    EXPECT_TRUE(parses(R"(
        val logger = Logger {
            fnc log(msg: string) -> string {
                rtn msg
            }
            fnc error(msg: string) -> string {
                rtn msg
            }
        }
    )"));
}

TEST_F(AnonImplBlockTest, AnonImplInValBinding) {
    EXPECT_TRUE(parses(R"(
        val handler = EventHandler {
            fnc on_click(x: int, y: int) -> int {
                rtn x
            }
        }
    )"));
}

TEST_F(AnonImplBlockTest, HandleUsesInlineTraitImpl) {
    EXPECT_TRUE(parses(R"(
        val result = handle({ 42 },
            FileSystem {
                fnc read(path: string) -> string {
                    rtn "content"
                }
            }
        )
    )"));
}

TEST_F(AnonImplBlockTest, IntersectionTypeAnonImpl) {
    EXPECT_TRUE(parses(R"(
        val widget = (Drawable & Clickable) {
            fnc draw() -> int {
                rtn 0
            }
            fnc on_click() -> int {
                rtn 0
            }
        }
    )"));
}

TEST_F(AnonImplBlockTest, MixedWithRegularCode) {
    EXPECT_TRUE(parses(R"(
        val x = 42
        val comp = Comparable {
            fnc compare(a: int, b: int) -> int {
                rtn a
            }
        }
        val y = x + 1
    )"));
}

TEST_F(AnonImplBlockTest, TypeDefStillWorks) {
    EXPECT_TRUE(parses(R"(
        struct Point {
            val x: float
            val y: float
        }
        class Person {
            val name: string
        }
        enum Color {
            Red, Green, Blue
        }
    )"));
}
