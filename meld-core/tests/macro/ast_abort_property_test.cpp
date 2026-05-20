/**
 * Property-based test for ast.abort structured error reporting.
 *
 * Feature: meld-lang, Property 69: ast.abort Structured Error
 *
 * For any message passed to ast.abort, macro expansion should halt and
 * produce a structured compiler error containing the message and source
 * location.
 *
 * Properties tested:
 *   (1) AstAbortError preserves the input message exactly
 *   (2) AstAbortError preserves all AbortSourceLocation fields exactly
 *   (3) to_cap_message() produces correct severity, code, message, and location
 *   (4) to_cap_json() produces valid JSON that round-trips correctly
 *   (5) what() always contains "ast.abort:" prefix and the original message
 *
 * Uses rapidcheck for property-based testing.
 *
 * **Validates: Requirements 2.10**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/macro/ast_abort.hpp"
#include "meld/compiler/cap.hpp"
#include <nlohmann/json.hpp>
#include <string>

using namespace meld::macro;
using namespace meld::compiler::cap;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate an arbitrary non-null error message string.
rc::Gen<std::string> genMessage() {
    return rc::gen::arbitrary<std::string>();
}

/// Generate an arbitrary file path string.
rc::Gen<std::string> genFilePath() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "file_" + std::to_string(n) + ".meld"; }
    );
}

/// Generate an arbitrary AbortSourceLocation with valid field values.
rc::Gen<AbortSourceLocation> genSourceLocation() {
    return rc::gen::apply(
        [](const std::string& file, size_t line, size_t col,
           size_t end_line, size_t end_col) {
            return AbortSourceLocation{file, line, col, end_line, end_col};
        },
        genFilePath(),
        rc::gen::inRange<size_t>(0, 100000),
        rc::gen::inRange<size_t>(0, 1000),
        rc::gen::inRange<size_t>(0, 100000),
        rc::gen::inRange<size_t>(0, 1000)
    );
}

} // anonymous namespace

// ===========================================================================
// Property 69.1: Message Preservation
//
// For any arbitrary message string, calling ast_abort(message, location)
// throws AstAbortError whose abort_message() equals the input message.
//
// **Validates: Requirements 2.10**
// ===========================================================================

TEST(AstAbortPropertyTest, MessagePreservation) {
    rc::check(
        "abort_message() equals the input message for any string",
        []() {
            auto message = *genMessage();
            auto location = *genSourceLocation();

            try {
                ast_abort(message, location);
                RC_FAIL("ast_abort should have thrown");
            } catch (const AstAbortError& e) {
                RC_ASSERT(e.abort_message() == message);
            }
        }
    );
}

// ===========================================================================
// Property 69.2: Location Preservation
//
// For any arbitrary AbortSourceLocation (file, line, column, end_line,
// end_column), the thrown error preserves all location fields exactly.
//
// **Validates: Requirements 2.10**
// ===========================================================================

TEST(AstAbortPropertyTest, LocationPreservation) {
    rc::check(
        "source_location() preserves all fields for any AbortSourceLocation",
        []() {
            auto message = *genMessage();
            auto location = *genSourceLocation();

            try {
                ast_abort(message, location);
                RC_FAIL("ast_abort should have thrown");
            } catch (const AstAbortError& e) {
                const auto& loc = e.source_location();
                RC_ASSERT(loc.file == location.file);
                RC_ASSERT(loc.line == location.line);
                RC_ASSERT(loc.column == location.column);
                RC_ASSERT(loc.end_line == location.end_line);
                RC_ASSERT(loc.end_column == location.end_column);
            }
        }
    );
}

// ===========================================================================
// Property 69.3: CAP Message Correctness
//
// For any arbitrary message and location, to_cap_message() produces a
// CompilationMessage with severity ERROR, code "E_MACRO_ABORT", the exact
// message, and matching location fields.
//
// **Validates: Requirements 2.10**
// ===========================================================================

TEST(AstAbortPropertyTest, CapMessageCorrectness) {
    rc::check(
        "to_cap_message() has ERROR severity, E_MACRO_ABORT code, correct message and location",
        []() {
            auto message = *genMessage();
            auto location = *genSourceLocation();

            try {
                ast_abort(message, location);
                RC_FAIL("ast_abort should have thrown");
            } catch (const AstAbortError& e) {
                auto cap_msg = e.to_cap_message();

                RC_ASSERT(cap_msg.severity == MessageSeverity::ERROR);
                RC_ASSERT(cap_msg.code == "E_MACRO_ABORT");
                RC_ASSERT(cap_msg.message == message);
                RC_ASSERT(cap_msg.location.file == location.file);
                RC_ASSERT(cap_msg.location.line == location.line);
                RC_ASSERT(cap_msg.location.column == location.column);
                RC_ASSERT(cap_msg.location.end_line == location.end_line);
                RC_ASSERT(cap_msg.location.end_column == location.end_column);
            }
        }
    );
}

// ===========================================================================
// Property 69.4: JSON Round-Trip
//
// For any arbitrary message and location, to_cap_json() produces valid JSON
// that round-trips through nlohmann::json and contains the correct severity,
// code, message, and location.
//
// **Validates: Requirements 2.10**
// ===========================================================================

TEST(AstAbortPropertyTest, JsonRoundTrip) {
    rc::check(
        "to_cap_json() produces valid JSON with correct severity, code, message, and location",
        []() {
            auto message = *genMessage();
            auto location = *genSourceLocation();

            try {
                ast_abort(message, location);
                RC_FAIL("ast_abort should have thrown");
            } catch (const AstAbortError& e) {
                std::string json_str = e.to_cap_json();

                // Must parse without throwing
                auto j = nlohmann::json::parse(json_str);

                RC_ASSERT(j["severity"] == "error");
                RC_ASSERT(j["code"] == "E_MACRO_ABORT");
                RC_ASSERT(j["message"] == message);
                RC_ASSERT(j["location"]["file"] == location.file);
                RC_ASSERT(j["location"]["line"] == location.line);
                RC_ASSERT(j["location"]["column"] == location.column);
                RC_ASSERT(j["location"]["end_line"] == location.end_line);
                RC_ASSERT(j["location"]["end_column"] == location.end_column);
            }
        }
    );
}

// ===========================================================================
// Property 69.5: what() String Format
//
// The what() string always contains "ast.abort:" prefix and the original
// message.
//
// **Validates: Requirements 2.10**
// ===========================================================================

TEST(AstAbortPropertyTest, WhatStringFormat) {
    rc::check(
        "what() contains 'ast.abort:' prefix and the original message",
        []() {
            auto message = *genMessage();
            auto location = *genSourceLocation();

            try {
                ast_abort(message, location);
                RC_FAIL("ast_abort should have thrown");
            } catch (const AstAbortError& e) {
                std::string what_str = e.what();
                RC_ASSERT(what_str.find("ast.abort:") != std::string::npos);
                RC_ASSERT(what_str.find(message) != std::string::npos);
            }
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
