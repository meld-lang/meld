/**
 * **Feature: meld-lsp-server, Property 15: Dispatch ambiguity detection**
 *
 * For any ambiguous multiple dispatch function call, the LSP server should
 * detect and report dispatch resolution errors.
 *
 * **Validates: Requirements 3.5**
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

rc::Gen<std::string> genFuncName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 3 + 7) % 26));
            }
            return s;
        }
    );
}

rc::Gen<std::string> genParamType() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"),
        std::string("String"), std::string("Bool"));
}

/// Two functions with same name and same param types (ambiguous)
rc::Gen<std::string> genAmbiguousDispatch() {
    return rc::gen::map(
        rc::gen::tuple(genFuncName(), genParamType()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, ptype] = t;
            return "fnc " + name + "(x: " + ptype + ") -> Int { return 1 }\n"
                   "fnc " + name + "(y: " + ptype + ") -> Int { return 2 }\n";
        }
    );
}

/// Two functions with same name but different param types (not ambiguous)
rc::Gen<std::string> genNonAmbiguousDispatch() {
    return rc::gen::map(
        genFuncName(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int) -> Int { return 1 }\n"
                   "fnc " + name + "(x: String) -> String { return \"a\" }\n";
        }
    );
}

/// Single function (no dispatch at all)
rc::Gen<std::string> genSingleFunction() {
    return rc::gen::map(
        rc::gen::tuple(genFuncName(), genParamType()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, ptype] = t;
            return "fnc " + name + "(x: " + ptype + ") -> " + ptype + " { return x }";
        }
    );
}

/// Multiple functions with different names (no ambiguity)
rc::Gen<std::string> genDifferentNameFunctions() {
    return rc::gen::map(
        rc::gen::tuple(genFuncName(), genFuncName()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [n1, n2] = t;
            std::string name2 = n2 + "_other";
            return "fnc " + n1 + "(x: Int) -> Int { return x }\n"
                   "fnc " + name2 + "(x: Int) -> Int { return x }\n";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(DispatchAmbiguityPropertyTest, AmbiguousDispatchDetected) {
    rc::check("Functions with same name and same param types must be flagged as ambiguous",
        []() {
            auto source = *genAmbiguousDispatch();
            AnalysisEngine engine;
            auto result = engine.detect_dispatch_ambiguity("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(!result.errors.empty());
            RC_ASSERT(result.errors[0].message.find("Ambiguous") != std::string::npos);
        }
    );
}

TEST(DispatchAmbiguityPropertyTest, DifferentParamTypesNotAmbiguous) {
    rc::check("Functions with same name but different param types must not be ambiguous",
        []() {
            auto source = *genNonAmbiguousDispatch();
            AnalysisEngine engine;
            auto result = engine.detect_dispatch_ambiguity("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(DispatchAmbiguityPropertyTest, SingleFunctionNotAmbiguous) {
    rc::check("A single function declaration must not produce ambiguity errors",
        []() {
            auto source = *genSingleFunction();
            AnalysisEngine engine;
            auto result = engine.detect_dispatch_ambiguity("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(DispatchAmbiguityPropertyTest, DifferentNameFunctionsNotAmbiguous) {
    rc::check("Functions with different names must not produce ambiguity errors",
        []() {
            auto source = *genDifferentNameFunctions();
            AnalysisEngine engine;
            auto result = engine.detect_dispatch_ambiguity("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(DispatchAmbiguityPropertyTest, AmbiguityErrorHasConflictingSignatures) {
    rc::check("Ambiguity errors must include conflicting signatures",
        []() {
            auto source = *genAmbiguousDispatch();
            AnalysisEngine engine;
            auto result = engine.detect_dispatch_ambiguity("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].conflicting_signatures.size() >= 2);
        }
    );
}
