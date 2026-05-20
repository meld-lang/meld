/**
 * **Feature: meld-lsp-server, Property 24: Rename conflict detection**
 *
 * For any rename operation that would cause naming conflicts, the LSP
 * server should detect and prevent the unsafe rename.
 *
 * **Validates: Requirements 5.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>

namespace {

using namespace meld::lsp::services;

} // anonymous namespace

/**
 * Property 24.1: Renaming a symbol to a name that already exists in
 * the document is detected and rejected.
 */
TEST(RenameConflictPropertyTest, DetectsExistingSymbolConflict) {
    rc::check("Rename to an existing symbol name is rejected",
        []() {
            int num_funcs = *rc::gen::inRange(3, 7);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc action_" << i << "(x: Int) {\n";
                oss << "    return x\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            // Pick two different functions
            int src_idx = *rc::gen::inRange(0, num_funcs);
            int dst_idx = *rc::gen::inRange(0, num_funcs - 1);
            if (dst_idx >= src_idx) dst_idx++;

            std::string target_name = "action_" + std::to_string(dst_idx);

            LanguageService service;

            // Try to rename src function to dst function's name
            int decl_line = src_idx * 4;
            auto result = service.rename_symbol("file:///test.meld", source,
                                                 decl_line, 4, target_name);

            // Should fail due to conflict
            RC_ASSERT(!result.success);
            RC_ASSERT(!result.error_message.empty());
            RC_ASSERT(result.edits.empty());
        }
    );
}

/**
 * Property 24.2: Renaming a symbol to a name that does NOT exist
 * succeeds without conflict.
 */
TEST(RenameConflictPropertyTest, NoConflictWhenNameIsUnique) {
    rc::check("Rename to a unique name succeeds",
        []() {
            int num_funcs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc worker_" << i << "(x: Int) {\n";
                oss << "    return x\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            int target = *rc::gen::inRange(0, num_funcs);

            LanguageService service;

            // Rename to a name that doesn't exist
            int decl_line = target * 4;
            auto result = service.rename_symbol("file:///test.meld", source,
                                                 decl_line, 4, "unique_new_name");

            RC_ASSERT(result.success);
            RC_ASSERT(result.error_message.empty());
            RC_ASSERT(!result.edits.empty());
        }
    );
}

/**
 * Property 24.3: Renaming to an invalid identifier (empty, starts with
 * digit, contains special chars) is rejected.
 */
TEST(RenameConflictPropertyTest, RejectsInvalidIdentifiers) {
    rc::check("Rename to invalid identifier is rejected",
        []() {
            std::string source =
                "fnc hello(x: Int) {\n"
                "    return x\n"
                "}\n";

            LanguageService service;

            // Empty name
            auto r1 = service.rename_symbol("file:///test.meld", source, 0, 4, "");
            RC_ASSERT(!r1.success);

            // Starts with digit
            auto r2 = service.rename_symbol("file:///test.meld", source, 0, 4, "3bad");
            RC_ASSERT(!r2.success);

            // Contains special character
            auto r3 = service.rename_symbol("file:///test.meld", source, 0, 4, "no-hyphens");
            RC_ASSERT(!r3.success);
        }
    );
}

/**
 * Property 24.4: Renaming a symbol to its own current name is rejected
 * as a no-op conflict.
 */
TEST(RenameConflictPropertyTest, RejectsSameNameRename) {
    rc::check("Rename to the same name is rejected",
        []() {
            int idx = *rc::gen::inRange(0, 5);
            std::string name = "func_" + std::to_string(idx);

            std::ostringstream oss;
            oss << "fnc " << name << "(x: Int) {\n";
            oss << "    return x\n";
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;

            auto result = service.rename_symbol("file:///test.meld", source,
                                                 0, 4, name);

            RC_ASSERT(!result.success);
            RC_ASSERT(result.edits.empty());
        }
    );
}
