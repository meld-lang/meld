/**
 * **Feature: meld-lsp-server, Property 27: Incremental index updates**
 *
 * For any file system change (add, modify, delete), the workspace index
 * should be updated incrementally to reflect the change.
 *
 * **Validates: Requirements 6.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/workspace_manager.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace {

using namespace meld::lsp::workspace;

/// Generate a plausible file URI (file:///workspace/some_name.meld)
rc::Gen<std::string> genFileUri() {
    return rc::gen::map(
        rc::gen::inRange(1, 9),
        [](int len) {
            std::string name;
            name.reserve(len);
            for (int i = 0; i < len; ++i) {
                name += static_cast<char>('a' + (i % 26));
            }
            return "file:///workspace/" + name + ".meld";
        }
    );
}

/// Generate a unique set of file URIs
rc::Gen<std::vector<std::string>> genUniqueUris(int min_count, int max_count) {
    return rc::gen::mapcat(
        rc::gen::inRange(min_count, max_count),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<std::string>>(count, genFileUri()),
                [](std::vector<std::string> uris) {
                    // Deduplicate by appending index
                    std::vector<std::string> unique;
                    for (size_t i = 0; i < uris.size(); ++i) {
                        unique.push_back(uris[i] + "_" + std::to_string(i));
                    }
                    return unique;
                }
            );
        }
    );
}

/// Represents an operation on the workspace index
enum class OpKind { Add, Modify, Delete };

struct Op {
    OpKind kind;
    std::string uri;
    std::string content;  // only used for Modify
    int version;          // only used for Modify
};

} // anonymous namespace

/**
 * Property 27.1: Add operation tracking
 *
 * For any file added via notify_file_added, it must appear in
 * index_state().indexed_files and get_changed_files().
 */
TEST(IncrementalIndexPropertyTest, AddOperationTracking) {
    rc::check("Added files appear in indexed_files and changed_files",
        []() {
            auto uris = *genUniqueUris(1, 11);

            WorkspaceManager mgr;

            for (const auto& uri : uris) {
                mgr.notify_file_added(uri);
            }

            const auto& state = mgr.index_state();
            auto changed = mgr.get_changed_files();

            for (const auto& uri : uris) {
                RC_ASSERT(state.indexed_files.count(uri) == 1);
                RC_ASSERT(changed.count(uri) == 1);
            }
        }
    );
}

/**
 * Property 27.2: Modify operation tracking
 *
 * For any document updated via update_document, it must appear in
 * get_changed_files().
 */
TEST(IncrementalIndexPropertyTest, ModifyOperationTracking) {
    rc::check("Modified documents appear in changed_files",
        []() {
            auto uris = *genUniqueUris(1, 11);

            WorkspaceManager mgr;

            int version = 1;
            for (const auto& uri : uris) {
                auto content = *rc::gen::string<std::string>();
                mgr.update_document(uri, content, version++);
            }

            auto changed = mgr.get_changed_files();

            for (const auto& uri : uris) {
                RC_ASSERT(changed.count(uri) == 1);
            }
        }
    );
}

/**
 * Property 27.3: Delete operation tracking
 *
 * For any file deleted via notify_file_deleted, it must be removed from
 * index_state().indexed_files and appear in get_changed_files().
 */
TEST(IncrementalIndexPropertyTest, DeleteOperationTracking) {
    rc::check("Deleted files are removed from indexed_files and appear in changed_files",
        []() {
            auto uris = *genUniqueUris(1, 11);

            WorkspaceManager mgr;

            // First add all files
            for (const auto& uri : uris) {
                mgr.notify_file_added(uri);
            }
            mgr.clear_changed_files();

            // Pick a random subset to delete
            auto delete_count = *rc::gen::inRange(1, static_cast<int>(uris.size()) + 1);
            std::vector<std::string> to_delete(uris.begin(), uris.begin() + delete_count);

            for (const auto& uri : to_delete) {
                mgr.notify_file_deleted(uri);
            }

            const auto& state = mgr.index_state();
            auto changed = mgr.get_changed_files();

            for (const auto& uri : to_delete) {
                // Must be removed from indexed_files
                RC_ASSERT(state.indexed_files.count(uri) == 0);
                // Must appear in changed_files
                RC_ASSERT(changed.count(uri) == 1);
            }

            // Files not deleted must still be indexed
            for (size_t i = static_cast<size_t>(delete_count); i < uris.size(); ++i) {
                RC_ASSERT(state.indexed_files.count(uris[i]) == 1);
            }
        }
    );
}

/**
 * Property 27.4: Clear resets changed set
 *
 * After clear_changed_files(), get_changed_files() must be empty.
 */
TEST(IncrementalIndexPropertyTest, ClearResetsChangedSet) {
    rc::check("clear_changed_files empties the changed set",
        []() {
            auto uris = *genUniqueUris(1, 11);

            WorkspaceManager mgr;

            // Perform a mix of operations to populate changed_files
            for (const auto& uri : uris) {
                mgr.notify_file_added(uri);
            }
            // Optionally modify some
            auto modify_count = *rc::gen::inRange(0, static_cast<int>(uris.size()) + 1);
            for (int i = 0; i < modify_count; ++i) {
                mgr.update_document(uris[i], "modified content", i + 1);
            }

            // Precondition: changed_files is non-empty
            RC_PRE(!mgr.get_changed_files().empty());

            mgr.clear_changed_files();

            RC_ASSERT(mgr.get_changed_files().empty());
        }
    );
}

/**
 * Property 27.5: Sequence of operations maintains consistent index state
 *
 * For any sequence of add/modify/delete operations, the index state must
 * be consistent: indexed_files contains exactly the files that have been
 * added and not subsequently deleted.
 */
TEST(IncrementalIndexPropertyTest, SequenceOfOperationsMaintainsConsistency) {
    rc::check("Arbitrary operation sequences produce consistent index state",
        []() {
            // Generate a pool of URIs to operate on
            auto pool = *genUniqueUris(2, 8);
            auto op_count = *rc::gen::inRange(5, 31);

            WorkspaceManager mgr;

            // Track expected state ourselves
            std::unordered_set<std::string> expected_indexed;
            std::unordered_set<std::string> expected_changed;

            for (int i = 0; i < op_count; ++i) {
                auto op_kind = *rc::gen::inRange(0, 4); // 0=add, 1=modify, 2=delete, 3=clear
                auto uri_idx = *rc::gen::inRange(0, static_cast<int>(pool.size()));
                const auto& uri = pool[uri_idx];

                switch (op_kind) {
                    case 0: { // Add
                        mgr.notify_file_added(uri);
                        expected_indexed.insert(uri);
                        expected_changed.insert(uri);
                        break;
                    }
                    case 1: { // Modify
                        auto content = "content_v" + std::to_string(i);
                        mgr.update_document(uri, content, i);
                        expected_changed.insert(uri);
                        break;
                    }
                    case 2: { // Delete
                        mgr.notify_file_deleted(uri);
                        expected_indexed.erase(uri);
                        expected_changed.insert(uri);
                        break;
                    }
                    case 3: { // Clear changed
                        mgr.clear_changed_files();
                        expected_changed.clear();
                        break;
                    }
                }
            }

            // Verify indexed_files matches our expected set
            const auto& state = mgr.index_state();
            RC_ASSERT(state.indexed_files.size() == expected_indexed.size());
            for (const auto& uri : expected_indexed) {
                RC_ASSERT(state.indexed_files.count(uri) == 1);
            }

            // Verify changed_files matches our expected set
            auto changed = mgr.get_changed_files();
            RC_ASSERT(changed.size() == expected_changed.size());
            for (const auto& uri : expected_changed) {
                RC_ASSERT(changed.count(uri) == 1);
            }
        }
    );
}
