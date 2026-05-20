/**
 * **Feature: meld-lsp-server, Property 34: Tree initialization validation**
 *
 * For any tree initialization syntax, constructor block syntax and nested
 * property assignments should be validated correctly.
 *
 * **Validates: Requirements 7.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/analysis_engine.hpp"

#include <string>
#include <vector>

namespace {

using namespace meld::lsp::analysis;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genStructName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s = "S";
            for (int i = 1; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 13 + 3) % 26));
            }
            return s;
        }
    );
}

rc::Gen<std::string> genFieldName() {
    return rc::gen::element(
        std::string("name"), std::string("age"),
        std::string("value"), std::string("count"),
        std::string("label"));
}

/// Struct definition + valid initialization using known fields
rc::Gen<std::string> genValidTreeInit() {
    return rc::gen::map(
        genStructName(),
        [](const std::string& name) {
            return "struct " + name + " {\n"
                   "    name: String\n"
                   "    age: Int\n"
                   "}\n"
                   "let obj = " + name + " { name: \"Alice\", age: 30 }\n";
        }
    );
}

/// Struct definition + initialization with unknown field
rc::Gen<std::string> genInvalidFieldInit() {
    return rc::gen::map(
        genStructName(),
        [](const std::string& name) {
            return "struct " + name + " {\n"
                   "    name: String\n"
                   "    age: Int\n"
                   "}\n"
                   "let obj = " + name + " { name: \"Alice\", unknown_field: 42 }\n";
        }
    );
}

/// Struct definition + initialization with only valid fields (subset)
rc::Gen<std::string> genPartialInit() {
    return rc::gen::map(
        genStructName(),
        [](const std::string& name) {
            return "struct " + name + " {\n"
                   "    name: String\n"
                   "    age: Int\n"
                   "    value: Float\n"
                   "}\n"
                   "let obj = " + name + " { name: \"Bob\" }\n";
        }
    );
}

/// Code with no struct definitions or initializations
rc::Gen<std::string> genNoStructCode() {
    return rc::gen::just(
        std::string("fnc foo(x: Int) -> Int { return x }\nlet val = 42\n"));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(TreeInitializationPropertyTest, ValidInitProducesNoErrors) {
    rc::check("Tree initialization with valid fields must produce no errors",
        []() {
            auto source = *genValidTreeInit();
            AnalysisEngine engine;
            auto result = engine.validate_tree_initialization("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(TreeInitializationPropertyTest, UnknownFieldProducesError) {
    rc::check("Tree initialization with unknown field must produce an error",
        []() {
            auto source = *genInvalidFieldInit();
            AnalysisEngine engine;
            auto result = engine.validate_tree_initialization("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("Unknown field") != std::string::npos);
            RC_ASSERT(result.errors[0].field_name == "unknown_field");
        }
    );
}

TEST(TreeInitializationPropertyTest, PartialInitIsValid) {
    rc::check("Tree initialization with subset of fields must not produce errors",
        []() {
            auto source = *genPartialInit();
            AnalysisEngine engine;
            auto result = engine.validate_tree_initialization("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(TreeInitializationPropertyTest, NoStructCodeProducesNoErrors) {
    rc::check("Code without struct initializations must produce no tree init errors",
        []() {
            auto source = *genNoStructCode();
            AnalysisEngine engine;
            auto result = engine.validate_tree_initialization("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.init_count == 0);
        }
    );
}

TEST(TreeInitializationPropertyTest, ErrorPositionsAreValid) {
    rc::check("Tree init error positions must be non-negative",
        []() {
            auto source = *genInvalidFieldInit();
            AnalysisEngine engine;
            auto result = engine.validate_tree_initialization("file:///test.meld", source);
            for (const auto& err : result.errors) {
                RC_ASSERT(err.line >= 0);
                RC_ASSERT(err.character >= 0);
            }
        }
    );
}
