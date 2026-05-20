/**
 * **Feature: meld-daemon, Property 43: Symbol Renaming Completeness**
 *
 * For any symbol rename operation, all references across the workspace
 * SHALL be updated correctly.
 *
 * **Validates: Requirements 19.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/formatting_provider.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_model_with_refs(SemanticModel& model,
                              const std::string& symbol_name,
                              const std::vector<std::filesystem::path>& files,
                              int refs_per_file) {
    for (const auto& file : files) {
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = "";

        // Add a definition in the first file
        if (file == files.front()) {
            auto def = std::make_shared<ASTNode>();
            def->kind = "function_definition";
            def->name = symbol_name;
            def->type_info = "() -> Int";
            def->location = SourceLocation{file, 1, 0};
            root->children.push_back(def);
        }

        // Add references
        for (int i = 0; i < refs_per_file; ++i) {
            auto ref = std::make_shared<ASTNode>();
            ref->kind = "identifier";
            ref->name = symbol_name;
            ref->location = SourceLocation{file,
                static_cast<uint32_t>(10 + i), 4};
            root->children.push_back(ref);
        }

        FileSemantics sem;
        sem.path = file;
        sem.ast = root;
        model.update_file(file, std::move(sem));
    }
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            s += 'a';  // Ensure starts with letter
            for (int i = 1; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 43a: Renaming a symbol produces edits for every reference
 * across all files in the workspace.
 */
RC_GTEST_PROP(SymbolRenamingCompleteness,
              AllReferencesAreUpdated,
              ()) {
    auto old_name = *genIdentifier();
    auto new_name = *rc::gen::distinctFrom(genIdentifier(), old_name);
    auto num_files = *rc::gen::inRange(1, 4);
    auto refs_per_file = *rc::gen::inRange(1, 4);

    std::vector<std::filesystem::path> files;
    for (int i = 0; i < num_files; ++i) {
        files.push_back("file" + std::to_string(i) + ".meld");
    }

    SemanticModel model;
    populate_model_with_refs(model, old_name, files, refs_per_file);

    FormattingProvider provider(model);
    auto result = provider.rename_symbol(old_name, new_name);

    RC_ASSERT(result.success);

    // Count total edits
    size_t total_edits = 0;
    for (const auto& [path, edits] : result.file_edits) {
        total_edits += edits.size();
    }

    // Should have at least 1 (definition) + refs_per_file * num_files references
    size_t expected_min = 1 + static_cast<size_t>(refs_per_file * num_files);
    RC_ASSERT(total_edits >= expected_min);

    // Every edit should replace old_name with new_name
    for (const auto& [path, edits] : result.file_edits) {
        for (const auto& edit : edits) {
            RC_ASSERT(edit.new_text == new_name);
        }
    }
}

/**
 * Property 43b: Renaming a nonexistent symbol fails gracefully.
 */
RC_GTEST_PROP(SymbolRenamingCompleteness,
              NonexistentSymbolFails,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";

    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    auto child = std::make_shared<ASTNode>();
    child->kind = "function_definition";
    child->name = "existing_func";
    child->location = SourceLocation{file, 1, 0};
    root->children.push_back(child);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));

    FormattingProvider provider(model);
    auto result = provider.rename_symbol("nonexistent_xyz", "new_name");

    RC_ASSERT(!result.success);
    RC_ASSERT(!result.error_message.empty());
}

/**
 * Property 43c: Rename edits within each file are sorted by position.
 */
RC_GTEST_PROP(SymbolRenamingCompleteness,
              RenameEditsAreSorted,
              ()) {
    auto old_name = *genIdentifier();
    auto new_name = *rc::gen::distinctFrom(genIdentifier(), old_name);

    SemanticModel model;
    std::vector<std::filesystem::path> files = {"a.meld", "b.meld"};
    populate_model_with_refs(model, old_name, files, 3);

    FormattingProvider provider(model);
    auto result = provider.rename_symbol(old_name, new_name);
    RC_ASSERT(result.success);

    for (const auto& [path, edits] : result.file_edits) {
        for (size_t i = 1; i < edits.size(); ++i) {
            RC_ASSERT(edits[i].start_line > edits[i - 1].start_line ||
                      (edits[i].start_line == edits[i - 1].start_line &&
                       edits[i].start_column >= edits[i - 1].start_column));
        }
    }
}

}  // namespace
}  // namespace meld::daemon
