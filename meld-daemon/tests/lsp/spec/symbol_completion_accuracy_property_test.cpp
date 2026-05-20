/**
 * **Feature: meld-lsp-server, Property 7: Symbol completion accuracy**
 *
 * For any symbol completion request, all available variables, functions,
 * types, and imported symbols in scope should be suggested.
 *
 * **Validates: Requirements 2.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <set>

namespace {

using namespace meld::lsp::services;

/// Check if a label exists in the completion list
bool has_completion(const CompletionList& list, const std::string& label) {
    return std::any_of(list.items.begin(), list.items.end(),
        [&](const CompletionItem& item) { return item.label == label; });
}

/// Check if a label exists with a specific kind
bool has_completion_with_kind(const CompletionList& list, const std::string& label,
                              CompletionItemKind kind) {
    return std::any_of(list.items.begin(), list.items.end(),
        [&](const CompletionItem& item) {
            return item.label == label && item.kind == kind;
        });
}

/// Generate a unique variable name with a given index
std::string var_name(int i) { return "myvar_" + std::to_string(i); }

/// Generate a unique function name with a given index
std::string func_name(int i) { return "myfunc_" + std::to_string(i); }

} // anonymous namespace

/**
 * Property 7.1: Local variables declared before cursor are suggested
 *
 * For code with let/var declarations before the cursor position,
 * all declared variables must appear in completions as Variable kind.
 */
TEST(SymbolCompletionAccuracyPropertyTest, LocalVariablesInScopeAreSuggested) {
    rc::check("All local variables declared before cursor must appear in completions",
        []() {
            int num_vars = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";
            for (int i = 0; i < num_vars; ++i) {
                std::string decl = (i % 2 == 0) ? "let" : "var";
                oss << "    " << decl << " " << var_name(i) << " = " << i << "\n";
            }
            // Cursor line: after all declarations
            oss << "    \n";
            int cursor_line = 1 + num_vars; // 0-based: line after all let/var lines
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, 4);

            // Property: every declared variable must appear in completions
            for (int i = 0; i < num_vars; ++i) {
                RC_ASSERT(has_completion_with_kind(completions, var_name(i),
                                                   CompletionItemKind::Variable));
            }
        }
    );
}

/**
 * Property 7.2: Function names are suggested inside function bodies
 *
 * For code with function declarations, all function names must appear
 * in completions when cursor is inside a function body.
 */
TEST(SymbolCompletionAccuracyPropertyTest, FunctionNamesAreSuggested) {
    rc::check("All function names must appear in completions inside a function body",
        []() {
            int num_funcs = *rc::gen::inRange(2, 6);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc " << func_name(i) << "() {\n";
                oss << "    let x = " << i << "\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            // Place cursor inside the last function body (on the let line)
            // Each function takes 4 lines (fnc, let, }, blank)
            // Last function starts at line (num_funcs-1)*4
            // Cursor inside last function body: (num_funcs-1)*4 + 1
            int cursor_line = (num_funcs - 1) * 4 + 1;

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, 4);

            // Property: every function name must appear in completions
            for (int i = 0; i < num_funcs; ++i) {
                RC_ASSERT(has_completion_with_kind(completions, func_name(i),
                                                   CompletionItemKind::Function));
            }
        }
    );
}

/**
 * Property 7.3: Variables declared after cursor are not suggested
 *
 * Variables declared after the cursor position should not appear
 * in the completion list as Variable items.
 */
TEST(SymbolCompletionAccuracyPropertyTest, OutOfScopeVariablesNotSuggested) {
    rc::check("Variables declared after cursor must not appear in completions",
        []() {
            int num_before = *rc::gen::inRange(1, 4);
            int num_after = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";

            // Variables before cursor
            for (int i = 0; i < num_before; ++i) {
                oss << "    let before_" << i << " = " << i << "\n";
            }

            // Cursor line
            oss << "    \n";
            int cursor_line = 1 + num_before;

            // Variables after cursor
            for (int i = 0; i < num_after; ++i) {
                oss << "    let after_" << i << " = " << i << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, 4);

            // Property: variables declared after cursor must NOT appear
            for (int i = 0; i < num_after; ++i) {
                std::string name = "after_" + std::to_string(i);
                RC_ASSERT(!has_completion_with_kind(completions, name,
                                                    CompletionItemKind::Variable));
            }

            // Property: variables declared before cursor MUST appear
            for (int i = 0; i < num_before; ++i) {
                std::string name = "before_" + std::to_string(i);
                RC_ASSERT(has_completion_with_kind(completions, name,
                                                    CompletionItemKind::Variable));
            }
        }
    );
}

/**
 * Property 7.4: Both local variables and function names coexist in completions
 *
 * When inside a function body with local variables, completions should
 * include both local variables and all function names from the file.
 */
TEST(SymbolCompletionAccuracyPropertyTest, VariablesAndFunctionsCoexistInCompletions) {
    rc::check("Completions inside a body must include both local variables and function names",
        []() {
            int num_other_funcs = *rc::gen::inRange(1, 4);
            int num_vars = *rc::gen::inRange(1, 4);

            std::ostringstream oss;

            // Other functions defined before the target function
            for (int i = 0; i < num_other_funcs; ++i) {
                oss << "fnc " << func_name(i) << "() {\n";
                oss << "    return " << i << "\n";
                oss << "}\n\n";
            }

            // Target function with local variables
            oss << "fnc target_func() {\n";
            for (int i = 0; i < num_vars; ++i) {
                oss << "    let " << var_name(i) << " = " << i << "\n";
            }
            oss << "    \n"; // cursor line
            int cursor_line = num_other_funcs * 4 + 1 + num_vars;
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, 4);

            // Property: all local variables must be present
            for (int i = 0; i < num_vars; ++i) {
                RC_ASSERT(has_completion_with_kind(completions, var_name(i),
                                                   CompletionItemKind::Variable));
            }

            // Property: all function names must be present
            for (int i = 0; i < num_other_funcs; ++i) {
                RC_ASSERT(has_completion_with_kind(completions, func_name(i),
                                                   CompletionItemKind::Function));
            }

            // Property: target_func itself should also appear
            RC_ASSERT(has_completion_with_kind(completions, "target_func",
                                               CompletionItemKind::Function));
        }
    );
}
