/**
 * Tests for ast.abort structured error reporting.
 *
 * Validates that ast.abort halts macro expansion and produces structured
 * compiler errors through the Compiler-Agent Protocol (CAP).
 *
 * Requirements: 2.10
 */

#include <gtest/gtest.h>
#include "meld/macro/ast_abort.hpp"
#include "meld/compiler/cap.hpp"
#include <nlohmann/json.hpp>
#include <string>

using namespace meld::macro;
using namespace meld::compiler::cap;

// =============================================================================
// ast_abort throws AstAbortError
// =============================================================================

TEST(AstAbortTest, ThrowsAstAbortError) {
    EXPECT_THROW(ast_abort("test error"), AstAbortError);
}

TEST(AstAbortTest, AbortMessageIsPreserved) {
    try {
        ast_abort("@Getter must be applied to a field inside a class");
        FAIL() << "ast_abort should have thrown";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.abort_message(),
                  "@Getter must be applied to a field inside a class");
    }
}

TEST(AstAbortTest, SourceLocationIsPreserved) {
    try {
        ast_abort("bad context",
                  AbortSourceLocation{"example.meld", 10, 5, 10, 20});
        FAIL() << "ast_abort should have thrown";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.source_location().file, "example.meld");
        EXPECT_EQ(e.source_location().line, 10u);
        EXPECT_EQ(e.source_location().column, 5u);
        EXPECT_EQ(e.source_location().end_line, 10u);
        EXPECT_EQ(e.source_location().end_column, 20u);
    }
}

// =============================================================================
// Convenience overload with individual location params
// =============================================================================

TEST(AstAbortTest, ConvenienceOverloadWithFileLineColumn) {
    try {
        ast_abort("invalid node kind", "macros.meld", 42, 8);
        FAIL() << "ast_abort should have thrown";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.abort_message(), "invalid node kind");
        EXPECT_EQ(e.source_location().file, "macros.meld");
        EXPECT_EQ(e.source_location().line, 42u);
        EXPECT_EQ(e.source_location().column, 8u);
    }
}

// =============================================================================
// Default location (no source info available)
// =============================================================================

TEST(AstAbortTest, DefaultLocationWhenNotProvided) {
    try {
        ast_abort("something went wrong");
        FAIL() << "ast_abort should have thrown";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.source_location().file, "");
        EXPECT_EQ(e.source_location().line, 0u);
        EXPECT_EQ(e.source_location().column, 0u);
    }
}

// =============================================================================
// what() includes "ast.abort:" prefix
// =============================================================================

TEST(AstAbortTest, WhatMessageIncludesPrefix) {
    try {
        ast_abort("parent is nil");
        FAIL() << "ast_abort should have thrown";
    } catch (const AstAbortError& e) {
        std::string what_str = e.what();
        EXPECT_NE(what_str.find("ast.abort:"), std::string::npos);
        EXPECT_NE(what_str.find("parent is nil"), std::string::npos);
    }
}

// =============================================================================
// CAP integration — to_cap_message produces correct CompilationMessage
// =============================================================================

TEST(AstAbortTest, ToCapMessageSeverityIsError) {
    try {
        ast_abort("macro failed", "test.meld", 5, 3);
        FAIL();
    } catch (const AstAbortError& e) {
        auto msg = e.to_cap_message();
        EXPECT_EQ(msg.severity, MessageSeverity::ERROR);
    }
}

TEST(AstAbortTest, ToCapMessageCodeIsMacroAbort) {
    try {
        ast_abort("macro failed", "test.meld", 5, 3);
        FAIL();
    } catch (const AstAbortError& e) {
        auto msg = e.to_cap_message();
        EXPECT_EQ(msg.code, "E_MACRO_ABORT");
    }
}

TEST(AstAbortTest, ToCapMessageContainsUserMessage) {
    try {
        ast_abort("@Property must be applied to a field inside a class",
                  "decorators.meld", 15, 1);
        FAIL();
    } catch (const AstAbortError& e) {
        auto msg = e.to_cap_message();
        EXPECT_EQ(msg.message,
                  "@Property must be applied to a field inside a class");
    }
}

TEST(AstAbortTest, ToCapMessageContainsLocation) {
    try {
        ast_abort("error here", "src/main.meld", 100, 12);
        FAIL();
    } catch (const AstAbortError& e) {
        auto msg = e.to_cap_message();
        EXPECT_EQ(msg.location.file, "src/main.meld");
        EXPECT_EQ(msg.location.line, 100u);
        EXPECT_EQ(msg.location.column, 12u);
    }
}

TEST(AstAbortTest, ToCapMessageHasContext) {
    try {
        ast_abort("bad", "f.meld", 1, 1);
        FAIL();
    } catch (const AstAbortError& e) {
        auto msg = e.to_cap_message();
        EXPECT_FALSE(msg.context.empty());
        EXPECT_NE(msg.context.find("ast.abort"), std::string::npos);
    }
}

// =============================================================================
// CAP integration — structured JSON output
// =============================================================================

TEST(AstAbortTest, ToCapJsonProducesValidJson) {
    try {
        ast_abort("field has no parent", "getter.meld", 22, 5);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string json_str = e.to_cap_json();
        auto j = nlohmann::json::parse(json_str);

        EXPECT_EQ(j["severity"], "error");
        EXPECT_EQ(j["code"], "E_MACRO_ABORT");
        EXPECT_EQ(j["message"], "field has no parent");
        EXPECT_EQ(j["location"]["file"], "getter.meld");
        EXPECT_EQ(j["location"]["line"], 22);
        EXPECT_EQ(j["location"]["column"], 5);
    }
}

TEST(AstAbortTest, ToCapJsonPrettyPrint) {
    try {
        ast_abort("test", "f.meld", 1, 1);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string pretty = e.to_cap_json(true);
        // Pretty-printed JSON contains newlines
        EXPECT_NE(pretty.find('\n'), std::string::npos);
        // Still valid JSON
        auto j = nlohmann::json::parse(pretty);
        EXPECT_EQ(j["code"], "E_MACRO_ABORT");
    }
}

// =============================================================================
// AbortSourceLocation — to_cap_location conversion
// =============================================================================

TEST(AstAbortTest, AbortSourceLocationConvertsToCapLocation) {
    AbortSourceLocation loc{"my_file.meld", 10, 5, 10, 25};
    auto cap_loc = loc.to_cap_location();

    EXPECT_EQ(cap_loc.file, "my_file.meld");
    EXPECT_EQ(cap_loc.line, 10u);
    EXPECT_EQ(cap_loc.column, 5u);
    EXPECT_EQ(cap_loc.end_line, 10u);
    EXPECT_EQ(cap_loc.end_column, 25u);
}

// =============================================================================
// AstAbortError is catchable as std::runtime_error
// =============================================================================

TEST(AstAbortTest, CatchableAsRuntimeError) {
    EXPECT_THROW(ast_abort("test"), std::runtime_error);
}

// =============================================================================
// Empty message is allowed
// =============================================================================

TEST(AstAbortTest, EmptyMessageIsAllowed) {
    try {
        ast_abort("");
        FAIL();
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.abort_message(), "");
        auto msg = e.to_cap_message();
        EXPECT_EQ(msg.message, "");
    }
}
// main() provided by gtest_main
