/**
 * **Feature: meld-lsp-server, Property 26: Workspace file discovery completeness**
 *
 * For any workspace, opening should discover and index all Meld source
 * files recursively.
 *
 * **Validates: Requirements 6.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/workspace_manager.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

namespace {

namespace fs = std::filesystem;
using namespace meld::lsp::workspace;

/// RAII helper to create and clean up a temporary directory tree
struct TempWorkspace {
    fs::path root;

    TempWorkspace() {
        root = fs::temp_directory_path() / ("meld_test_ws_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }

    ~TempWorkspace() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    /// Create a file at a relative path under the workspace root
    void create_file(const fs::path& relative_path) {
        auto full = root / relative_path;
        fs::create_directories(full.parent_path());
        std::ofstream ofs(full);
        ofs << "// auto-generated test file\n";
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

/// Generate a relative directory path with 0-3 levels of nesting
rc::Gen<std::vector<std::string>> genDirComponents() {
    return rc::gen::mapcat(
        rc::gen::inRange(0, 4),
        [](int depth) {
            return rc::gen::container<std::vector<std::string>>(depth, genFileNamePart());
        }
    );
}

/// Generate a non-.meld file extension
rc::Gen<std::string> genNonMeldExtension() {
    static const std::vector<std::string> extensions = {
        ".txt", ".cpp", ".hpp", ".py", ".rs", ".json", ".toml", ".md", ".log"
    };
    return rc::gen::elementOf(extensions);
}

} // anonymous namespace

/**
 * Property 26: All .meld files in a workspace directory tree are discovered
 *
 * For any set of .meld files placed at arbitrary depths in a workspace
 * directory tree, find_meld_files() must return every one of them.
 */
TEST(WorkspaceFileDiscoveryPropertyTest, AllMeldFilesAreDiscovered) {
    rc::check("All .meld files in workspace must be discovered",
        []() {
            // Generate 1-10 .meld files at random directory depths
            auto file_count = *rc::gen::inRange(1, 11);

            TempWorkspace ws;
            std::set<std::string> expected_files;

            for (int i = 0; i < file_count; ++i) {
                auto dir_parts = *genDirComponents();
                auto name = *genFileNamePart();

                fs::path rel_path;
                for (const auto& part : dir_parts) {
                    rel_path /= part;
                }
                rel_path /= (name + "_" + std::to_string(i) + ".meld");

                ws.create_file(rel_path);
                auto full_path = (ws.root / rel_path).string();
                // Normalize path separators for comparison
                std::replace(full_path.begin(), full_path.end(), '\\', '/');
                expected_files.insert(full_path);
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());
            auto discovered = mgr.find_meld_files();

            // Normalize discovered paths
            std::set<std::string> discovered_set;
            for (auto& f : discovered) {
                std::replace(f.begin(), f.end(), '\\', '/');
                discovered_set.insert(f);
            }

            // Property: every expected .meld file must be in the discovered set
            for (const auto& expected : expected_files) {
                RC_ASSERT(discovered_set.count(expected) == 1);
            }

            // Property: discovered count must match expected count
            RC_ASSERT(discovered_set.size() == expected_files.size());
        }
    );
}

/**
 * Property 26 (continued): Non-.meld files are excluded from discovery
 *
 * For any workspace containing a mix of .meld and non-.meld files,
 * find_meld_files() must only return .meld files.
 */
TEST(WorkspaceFileDiscoveryPropertyTest, NonMeldFilesAreExcluded) {
    rc::check("Non-.meld files must not appear in discovery results",
        []() {
            auto meld_count = *rc::gen::inRange(1, 6);
            auto other_count = *rc::gen::inRange(1, 6);

            TempWorkspace ws;

            // Create .meld files
            for (int i = 0; i < meld_count; ++i) {
                auto name = *genFileNamePart();
                fs::path rel_path = name + "_" + std::to_string(i) + ".meld";
                ws.create_file(rel_path);
            }

            // Create non-.meld files
            for (int i = 0; i < other_count; ++i) {
                auto name = *genFileNamePart();
                auto ext = *genNonMeldExtension();
                fs::path rel_path = name + "_other_" + std::to_string(i) + ext;
                ws.create_file(rel_path);
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());
            auto discovered = mgr.find_meld_files();

            // Property: every discovered file must have .meld extension
            for (const auto& file : discovered) {
                fs::path p(file);
                RC_ASSERT(p.extension().string() == ".meld");
            }

            // Property: discovered count must equal the number of .meld files created
            RC_ASSERT(static_cast<int>(discovered.size()) == meld_count);
        }
    );
}

/**
 * Property 26 (continued): Recursive discovery through subdirectories
 *
 * For any workspace with nested subdirectories containing .meld files,
 * all files at every depth level must be discovered.
 */
TEST(WorkspaceFileDiscoveryPropertyTest, RecursiveDiscoveryThroughSubdirectories) {
    rc::check("Files at all nesting depths must be discovered",
        []() {
            // Generate a depth between 1 and 5
            auto max_depth = *rc::gen::inRange(1, 6);

            TempWorkspace ws;
            int expected_count = 0;

            // Place one .meld file at each depth level
            fs::path current;
            for (int depth = 0; depth <= max_depth; ++depth) {
                if (depth > 0) {
                    auto dir_name = *genFileNamePart();
                    current /= (dir_name + "_d" + std::to_string(depth));
                }
                auto file_name = "level_" + std::to_string(depth) + ".meld";
                ws.create_file(current / file_name);
                expected_count++;
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());
            auto discovered = mgr.find_meld_files();

            // Property: number of discovered files must equal files placed at each depth
            RC_ASSERT(static_cast<int>(discovered.size()) == expected_count);

            // Property: all discovered files are .meld files
            for (const auto& file : discovered) {
                RC_ASSERT(fs::path(file).extension().string() == ".meld");
            }
        }
    );
}

/**
 * Property 26 (continued): Empty workspace discovers no files
 *
 * For any workspace with no .meld files (only directories or non-.meld files),
 * find_meld_files() must return an empty result.
 */
TEST(WorkspaceFileDiscoveryPropertyTest, EmptyWorkspaceDiscoversNoFiles) {
    rc::check("Workspace with no .meld files must discover nothing",
        []() {
            auto dir_count = *rc::gen::inRange(0, 5);
            auto other_file_count = *rc::gen::inRange(0, 5);

            TempWorkspace ws;

            // Create some empty subdirectories
            for (int i = 0; i < dir_count; ++i) {
                auto name = *genFileNamePart();
                fs::create_directories(ws.root / (name + "_dir_" + std::to_string(i)));
            }

            // Create non-.meld files
            for (int i = 0; i < other_file_count; ++i) {
                auto name = *genFileNamePart();
                auto ext = *genNonMeldExtension();
                ws.create_file(fs::path(name + "_" + std::to_string(i) + ext));
            }

            WorkspaceManager mgr;
            mgr.add_workspace_folder(ws.root.string());
            auto discovered = mgr.find_meld_files();

            // Property: no files should be discovered
            RC_ASSERT(discovered.empty());
        }
    );
}
