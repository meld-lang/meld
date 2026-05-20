/**
 * **Feature: meld-lsp-server, Property 9: Type completion accuracy**
 *
 * For any type annotation context, completions should include all valid
 * type names (built-in, user-defined, and aliases).
 *
 * **Validates: Requirements 2.4**
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

/// Built-in types that must always appear in type position completions
static const std::vector<std::string> BUILTIN_TYPES = {
    "Int", "Float", "String", "Bool", "Void",
    "List", "Map", "Option", "Result"
};

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

/// Generate a valid Meld identifier starting with uppercase (for type names)
rc::Gen<std::string> genTypeName(const std::string& prefix) {
    return rc::gen::map(rc::gen::inRange(0, 10000), [prefix](int i) {
        return prefix + std::to_string(i);
    });
}

} // anonymous namespace

/**
 * Property 9.1: Built-in types are always suggested in type positions
 *
 * In any type position (after ':' or '->'), all built-in types
 * (Int, Float, String, Bool, Void) must appear in completions.
 */
TEST(TypeCompletionAccuracyPropertyTest, BuiltInTypesAlwaysSuggestedAfterColon) {
    rc::check("All built-in types must appear in completions after ':'",
        []() {
            // Generate a random number of let bindings before the cursor
            int num_vars = *rc::gen::inRange(0, 5);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";
            for (int i = 0; i < num_vars; ++i) {
                oss << "    let x" << i << ": Int = " << i << "\n";
            }
            // Type annotation position: after ':'
            oss << "    let result: \n";
            int cursor_line = 1 + num_vars;
            int cursor_char = 16; // after ": "
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every built-in type must appear
            for (const auto& type_name : BUILTIN_TYPES) {
                RC_ASSERT(has_completion(completions, type_name));
            }
        }
    );
}

/**
 * Property 9.2: Built-in types are always suggested after return type arrow
 *
 * In return type position (after '->'), all built-in types must appear.
 */
TEST(TypeCompletionAccuracyPropertyTest, BuiltInTypesAlwaysSuggestedAfterArrow) {
    rc::check("All built-in types must appear in completions after '->'",
        []() {
            // Generate random function name
            int fn_idx = *rc::gen::inRange(0, 1000);
            std::string fn_name = "my_func_" + std::to_string(fn_idx);

            std::ostringstream oss;
            oss << "fnc " << fn_name << "() -> \n";
            int cursor_line = 0;
            int cursor_char = static_cast<int>(std::string("fnc " + fn_name + "() -> ").size());

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every built-in type must appear
            for (const auto& type_name : BUILTIN_TYPES) {
                RC_ASSERT(has_completion(completions, type_name));
            }
        }
    );
}

/**
 * Property 9.3: User-defined struct types are suggested in type positions
 *
 * For code with struct declarations, struct names must appear
 * in type completions.
 */
TEST(TypeCompletionAccuracyPropertyTest, UserDefinedStructTypesAreSuggested) {
    rc::check("Struct type names must appear in type position completions",
        []() {
            int num_structs = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            std::vector<std::string> struct_names;

            for (int i = 0; i < num_structs; ++i) {
                std::string name = "MyStruct" + std::to_string(i);
                struct_names.push_back(name);
                oss << "struct " << name << " {\n";
                oss << "    value: Int\n";
                oss << "}\n\n";
            }

            // Function with type annotation position
            oss << "fnc test_func() {\n";
            oss << "    let x: \n";
            // cursor line: each struct takes 4 lines, then fnc line, then let line
            int cursor_line = num_structs * 4 + 1;
            int cursor_char = 11; // after "    let x: "
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every struct name must appear as Type kind
            for (const auto& name : struct_names) {
                RC_ASSERT(has_completion_with_kind(completions, name,
                                                   CompletionItemKind::Type));
            }
        }
    );
}

/**
 * Property 9.4: User-defined enum types are suggested in type positions
 *
 * For code with enum declarations, enum names must appear
 * in type completions.
 */
TEST(TypeCompletionAccuracyPropertyTest, UserDefinedEnumTypesAreSuggested) {
    rc::check("Enum type names must appear in type position completions",
        []() {
            int num_enums = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            std::vector<std::string> enum_names;

            for (int i = 0; i < num_enums; ++i) {
                std::string name = "MyEnum" + std::to_string(i);
                enum_names.push_back(name);
                oss << "enum " << name << " {\n";
                oss << "    VariantA\n";
                oss << "}\n\n";
            }

            // Function with type annotation position
            oss << "fnc test_func() {\n";
            oss << "    let x: \n";
            int cursor_line = num_enums * 4 + 1;
            int cursor_char = 11;
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every enum name must appear as Type kind
            for (const auto& name : enum_names) {
                RC_ASSERT(has_completion_with_kind(completions, name,
                                                   CompletionItemKind::Type));
            }
        }
    );
}

/**
 * Property 9.5: User-defined trait types are suggested in type positions
 *
 * For code with trait declarations, trait names must appear
 * in type completions.
 */
TEST(TypeCompletionAccuracyPropertyTest, UserDefinedTraitTypesAreSuggested) {
    rc::check("Trait type names must appear in type position completions",
        []() {
            int num_traits = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            std::vector<std::string> trait_names;

            for (int i = 0; i < num_traits; ++i) {
                std::string name = "MyTrait" + std::to_string(i);
                trait_names.push_back(name);
                oss << "trait " << name << " {\n";
                oss << "    fnc do_something()\n";
                oss << "}\n\n";
            }

            // Function with type annotation position
            oss << "fnc test_func() {\n";
            oss << "    let x: \n";
            int cursor_line = num_traits * 4 + 1;
            int cursor_char = 11;
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every trait name must appear as Type kind
            for (const auto& name : trait_names) {
                RC_ASSERT(has_completion_with_kind(completions, name,
                                                   CompletionItemKind::Type));
            }
        }
    );
}

/**
 * Property 9.6: All type completions have Type kind
 *
 * Every item in type position completions must have CompletionItemKind::Type.
 */
TEST(TypeCompletionAccuracyPropertyTest, AllTypeCompletionsHaveTypeKind) {
    rc::check("All completions in type position must have Type kind",
        []() {
            int num_structs = *rc::gen::inRange(0, 3);
            int num_enums = *rc::gen::inRange(0, 3);
            int num_traits = *rc::gen::inRange(0, 3);

            std::ostringstream oss;

            for (int i = 0; i < num_structs; ++i) {
                oss << "struct TypeStruct" << i << " {\n";
                oss << "    value: Int\n";
                oss << "}\n\n";
            }
            for (int i = 0; i < num_enums; ++i) {
                oss << "enum TypeEnum" << i << " {\n";
                oss << "    Variant\n";
                oss << "}\n\n";
            }
            for (int i = 0; i < num_traits; ++i) {
                oss << "trait TypeTrait" << i << " {\n";
                oss << "    fnc method()\n";
                oss << "}\n\n";
            }

            int total_decls = num_structs + num_enums + num_traits;
            oss << "fnc test_func() {\n";
            oss << "    let x: \n";
            int cursor_line = total_decls * 4 + 1;
            int cursor_char = 11;
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source,
                                                        cursor_line, cursor_char);

            // Property: every completion item must have Type kind
            RC_ASSERT(!completions.items.empty());
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Type);
            }
        }
    );
}
