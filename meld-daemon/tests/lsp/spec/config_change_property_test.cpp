/**
 * **Feature: meld-lsp-server, Property 29: Configuration change handling**
 *
 * For any workspace configuration change, affected files should be
 * reloaded and reindexed appropriately.
 *
 * **Validates: Requirements 6.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/workspace_manager.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <chrono>
#include <algorithm>

namespace {

namespace fs = std::filesystem;
using namespace meld::lsp::workspace;

/// RAII helper to create and clean up a temporary directory tree
struct TempWorkspace {
    fs::path root;

    TempWorkspace() {
        root = fs::temp_directory_path() / ("meld_cfg_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }

    ~TempWorkspace() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void create_file(const fs::path& relative_path) {
        auto full = root / relative_path;
        fs::create_directories(full.parent_path());
        std::ofstream ofs(full);
        ofs << "// test file\n";
    }

    void remove_file(const fs::path& relative_path) {
        std::error_code ec;
        fs::remove(root / relative_path, ec);
    }
};

/// Generate a valid filename component (lowercase letters, 1-8 chars)
rc::Gen<std::string> genFileNamePart() {
    return rc::gen::map(
        rc::gen::inRange(1, 9),
        [](int len) {
            std::string result;
            result.reserve(len);
            for (int i = 0; i < len; ++i) {
                result += static_cast<char>('a' + (i % 26));
            }
            return result;
        }
    );
}

/// Normalize a path string for cross-platform comparison
std::string normalize(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

/// Collect normalized indexed files from a WorkspaceManager
std::set<std::string> normalized_indexed(const WorkspaceManager& mgr) {
    std::set<std::string> result;
    for (const auto& f : mgr.index_state().indexed_files) {
        result.insert(normalize(std::string(f)));
    }
    return result;
}

/// Collect normalized changed files from a WorkspaceManager
std::set<std::string> normalized_changed(const WorkspaceManager& mgr) {
    std::set<std::string> result;
    for (const auto& f : mgr.get_changed_files()) {
        result.insert(normalize(std::string(f)));
    }
    return result;
}

} // anonymous namespace

/**
 * Property 29.1: Reload rebuilds index from current workspace folders
 *
 * After reload_configuration(), indexed_files must contain exactly the
 * files discoverable from the current workspace folders.
 */
TEST(ConfigChangePropertyTest, ReloadRebuildsIndex) {
    rc::check("reload_configuration rebuilds index to match discoverable files",
        []() {
            auto file_count = *rc::gen::inRange(1, 8);

            TempWorkspace ws;
            std::set<std::string> expected;

            for (int i = 0; i < file_count; ++i) {
                auto name = *genFileNamePart();
                fs::path rel = name + "_" + std::to_string(i) + ".meld";
                ws.create_file(rel);
                expected.insert(normalize((ws.root / rel).string()));
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());

            // Mutate state: manually add a bogus entry
            mgr.notify_file_added("file:///bogus/fake.meld");

            mgr.reload_configuration();

            auto indexed = normalized_indexed(mgr);

            // Property: indexed files must match exactly what's on disk
            RC_ASSERT(indexed.size() == expected.size());
            for (const auto& f : expected) {
                RC_ASSERT(indexed.count(f) == 1);
            }
            // Bogus entry must be gone
            RC_ASSERT(indexed.count("file:///bogus/fake.meld") == 0);
        }
    );
}

/**
 * Property 29.2: Reload updates the last_full_index timestamp
 *
 * After reload_configuration(), last_full_index must be more recent
 * than the timestamp captured before the reload.
 */
TEST(ConfigChangePropertyTest, ReloadUpdatesTimestamp) {
    rc::check("reload_configuration updates last_full_index timestamp",
        []() {
            TempWorkspace ws;
            ws.create_file("dummy.meld");

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());

            auto before = mgr.index_state().last_full_index;

            // Small delay to ensure clock advances
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            mgr.reload_configuration();

            auto after = mgr.index_state().last_full_index;

            RC_ASSERT(after > before);
        }
    );
}

/**
 * Property 29.3: Reload marks all files as changed
 *
 * After reload_configuration(), every indexed file must also appear
 * in changed_files so downstream consumers know to re-process them.
 */
TEST(ConfigChangePropertyTest, ReloadMarksAllFilesAsChanged) {
    rc::check("reload_configuration marks all indexed files as changed",
        []() {
            auto file_count = *rc::gen::inRange(1, 8);

            TempWorkspace ws;
            for (int i = 0; i < file_count; ++i) {
                auto name = *genFileNamePart();
                ws.create_file(fs::path(name + "_" + std::to_string(i) + ".meld"));
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());

            // Clear changed files to simulate a "clean" state
            mgr.clear_changed_files();
            RC_PRE(mgr.get_changed_files().empty());

            mgr.reload_configuration();

            auto indexed = normalized_indexed(mgr);
            auto changed = normalized_changed(mgr);

            // Property: every indexed file must be in changed_files
            for (const auto& f : indexed) {
                RC_ASSERT(changed.count(f) == 1);
            }
            // Property: changed set must equal indexed set
            RC_ASSERT(changed.size() == indexed.size());
        }
    );
}

/**
 * Property 29.4: Add/remove folders then reload reflects only active folders
 *
 * For any sequence of add/remove workspace folder operations followed
 * by reload, the index must reflect only the currently-active folders.
 */
TEST(ConfigChangePropertyTest, AddRemoveFoldersThenReload) {
    rc::check("reload after add/remove reflects only active folders",
        []() {
            // Create 2-4 separate workspace folders
            auto folder_count = *rc::gen::inRange(2, 5);

            std::vector<TempWorkspace> workspaces(folder_count);
            std::set<std::string> all_expected;

            // Put files in each folder
            for (int i = 0; i < folder_count; ++i) {
                auto fc = *rc::gen::inRange(1, 4);
                for (int j = 0; j < fc; ++j) {
                    auto name = *genFileNamePart();
                    fs::path rel = name + "_" + std::to_string(j) + ".meld";
                    workspaces[i].create_file(rel);
                }
            }

            WorkspaceManager mgr;

            // Add all folders
            for (int i = 0; i < folder_count; ++i) {
                mgr.add_workspace_folder(workspaces[i].root.string());
            }

            // Remove a random subset of folders (at least 1, keep at least 1)
            auto remove_count = *rc::gen::inRange(1, folder_count);
            std::set<int> removed_indices;
            for (int i = 0; i < remove_count; ++i) {
                removed_indices.insert(i);
                mgr.remove_workspace_folder(workspaces[i].root.string());
            }

            // Build expected set from remaining folders
            for (int i = 0; i < folder_count; ++i) {
                if (removed_indices.count(i) == 0) {
                    for (const auto& entry : fs::recursive_directory_iterator(workspaces[i].root)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                            all_expected.insert(normalize(entry.path().string()));
                        }
                    }
                }
            }

            mgr.reload_configuration();

            auto indexed = normalized_indexed(mgr);

            // Property: indexed files must match only the active folders
            RC_ASSERT(indexed.size() == all_expected.size());
            for (const auto& f : all_expected) {
                RC_ASSERT(indexed.count(f) == 1);
            }
        }
    );
}

/**
 * Property 29.5: Reload with filesystem changes reflects current disk state
 *
 * If files are added or removed from disk between reloads, the new
 * reload must reflect the current filesystem state.
 */
TEST(ConfigChangePropertyTest, ReloadReflectsFilesystemChanges) {
    rc::check("reload after filesystem changes reflects current disk state",
        []() {
            auto initial_count = *rc::gen::inRange(2, 7);

            TempWorkspace ws;
            std::vector<fs::path> initial_files;

            for (int i = 0; i < initial_count; ++i) {
                auto name = *genFileNamePart();
                fs::path rel = name + "_" + std::to_string(i) + ".meld";
                ws.create_file(rel);
                initial_files.push_back(rel);
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());

            // Verify initial state
            auto initial_indexed = normalized_indexed(mgr);
            RC_ASSERT(static_cast<int>(initial_indexed.size()) == initial_count);

            // Remove some files from disk
            auto remove_count = *rc::gen::inRange(1, initial_count);
            for (int i = 0; i < remove_count; ++i) {
                ws.remove_file(initial_files[i]);
            }

            // Add some new files to disk
            auto add_count = *rc::gen::inRange(0, 4);
            for (int i = 0; i < add_count; ++i) {
                auto name = *genFileNamePart();
                fs::path rel = "new_" + name + "_" + std::to_string(i) + ".meld";
                ws.create_file(rel);
            }

            // Build expected set from what's actually on disk now
            std::set<std::string> expected;
            for (const auto& entry : fs::recursive_directory_iterator(ws.root)) {
                if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                    expected.insert(normalize(entry.path().string()));
                }
            }

            mgr.reload_configuration();

            auto indexed = normalized_indexed(mgr);

            // Property: indexed files must match current disk state
            RC_ASSERT(indexed.size() == expected.size());
            for (const auto& f : expected) {
                RC_ASSERT(indexed.count(f) == 1);
            }
        }
    );
}
