/**
 * **Feature: meld-lsp-server, Property 33: Multiple dispatch resolution**
 *
 * For any multiple dispatch function call, dispatch should be resolved
 * based on all argument types correctly.
 *
 * **Validates: Requirements 7.4**
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
                s += static_cast<char>('a' + ((i * 9 + 2) % 26));
            }
            return s;
        }
    );
}

/// Overloaded functions with matching call (resolvable)
rc::Gen<std::string> genResolvableDispatch() {
    return rc::gen::map(
        genFuncName(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int) -> Int { return x }\n"
                   "fnc " + name + "(x: String) -> String { return x }\n"
                   "let result = " + name + "(42)\n";
        }
    );
}

/// Overloaded functions with call that has wrong arg count
rc::Gen<std::string> genUnresolvableDispatch() {
    return rc::gen::map(
        genFuncName(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int) -> Int { return x }\n"
                   "fnc " + name + "(x: Int, y: Int) -> Int { return x }\n"
                   "let result = " + name + "(1, 2, 3)\n";
        }
    );
}

/// Single function with matching call
rc::Gen<std::string> genSingleFuncCall() {
    return rc::gen::map(
        genFuncName(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int) -> Int { return x }\n"
                   "let result = " + name + "(42)\n";
        }
    );
}

/// Code with no function calls
rc::Gen<std::string> genNoFuncCalls() {
    return rc::gen::map(
        genFuncName(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int) -> Int { return x }\n"
                   "let val = 42\n";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(MultipleDispatchResolutionPropertyTest, ResolvableDispatchProducesNoErrors) {
    rc::check("Calls matching an overload must produce no dispatch errors",
        []() {
            auto source = *genResolvableDispatch();
            AnalysisEngine engine;
            auto result = engine.resolve_multiple_dispatch("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(MultipleDispatchResolutionPropertyTest, UnresolvableDispatchProducesError) {
    rc::check("Calls with wrong arg count for all overloads must produce errors",
        []() {
            auto source = *genUnresolvableDispatch();
            AnalysisEngine engine;
            auto result = engine.resolve_multiple_dispatch("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("No matching") != std::string::npos);
        }
    );
}

TEST(MultipleDispatchResolutionPropertyTest, SingleFuncCallNoErrors) {
    rc::check("Single function with matching call must produce no errors",
        []() {
            auto source = *genSingleFuncCall();
            AnalysisEngine engine;
            auto result = engine.resolve_multiple_dispatch("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(MultipleDispatchResolutionPropertyTest, NoCallsNoErrors) {
    rc::check("Code without function calls must produce no dispatch errors",
        []() {
            auto source = *genNoFuncCalls();
            AnalysisEngine engine;
            auto result = engine.resolve_multiple_dispatch("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(MultipleDispatchResolutionPropertyTest, ErrorIncludesCandidateSignatures) {
    rc::check("Dispatch errors must include candidate signatures",
        []() {
            auto source = *genUnresolvableDispatch();
            AnalysisEngine engine;
            auto result = engine.resolve_multiple_dispatch("file:///test.meld", source);
            if (result.has_errors()) {
                RC_ASSERT(!result.errors[0].candidate_signatures.empty());
            }
        }
    );
}
