/**
 * **Feature: meld-daemon, Property 46: Workspace File Discovery Completeness**
 *
 * For any workspace, opening SHALL discover and index all Meld source files
 * recursively.
 *
 * **Validates: Requirements 20.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/workspace_provider.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Create a temporary workspace with .meld files for testing.
struct TempWorkspace {
    std::filesystem::path root;
    std::vector<std::filesystem::path> meld_files;
    std::vector<std::filesystem::path> non_meld_files;

    TempWorkspace() {
        root = std::filesystem::temp_directory_path() /
               ("meld_test_ws_" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
    }

    void add_meld_file(const std::string& relative_path) {
        auto full = root / relative_path;
        std::filesystem::create_directories(full.parent_path());
        // Create an empty file
        std::ofstream(full.string()).close();
        meld_files.push_back(full);
    }

    void add_non_meld_file(const std::string& relative_path) {
        auto full = root / relative_path;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream(full.string()).close();
        non_meld_files.push_back(full);
    }

    ~TempWorkspace() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genFileName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

rc::Gen<std::string> genSubdir() {
    return rc::gen::elementOf(std::vector<std::string>{
        "", "src", "lib", "src/core", "lib/utils"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 46a: All .meld files in a workspace are discovered and indexed.
 */
RC_GTEST_PROP(WorkspaceFileDiscovery,
              AllMeldFilesDiscovered,
              ()) {
    auto file_count = *rc::gen::inRange(1, 6);
    TempWorkspace ws;

    std::set<std::filesystem::path> expected;
    for (int i = 0; i < file_count; ++i) {
        auto subdir = *genSubdir();
        auto name = *genFileName();
        std::string rel = subdir.empty() ? (name + ".meld")
                                         : (subdir + "/" + name + ".meld");
        auto full = ws.root / rel;
        if (expected.count(full) == 0) {
            ws.add_meld_file(rel);
            expected.insert(full);
        }
    }

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    auto discovered = provider.discover_and_index(ws.root);

    // Every expected file must be discovered
    std::set<std::filesystem::path> discovered_set(discovered.begin(),
                                                    discovered.end());
    for (const auto& f : expected) {
        RC_ASSERT(discovered_set.count(f) == 1);
    }
    // No extra files
    RC_ASSERT(discovered_set.size() == expected.size());
}

/**
 * Property 46b: Non-.meld files are NOT indexed.
 */
RC_GTEST_PROP(WorkspaceFileDiscovery,
              NonMeldFilesExcluded,
              ()) {
    TempWorkspace ws;
    ws.add_meld_file("main.meld");
    ws.add_non_meld_file("readme.txt");
    ws.add_non_meld_file("build.toml");
    ws.add_non_meld_file("src/helper.cpp");

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    auto discovered = provider.discover_and_index(ws.root);

    // Only .meld files should be discovered
    for (const auto& f : discovered) {
        RC_ASSERT(f.extension() == ".meld");
    }
    RC_ASSERT(discovered.size() == 1);
}

/**
 * Property 46c: After discovery, all files are present in the SemanticModel.
 */
RC_GTEST_PROP(WorkspaceFileDiscovery,
              AllDiscoveredFilesInModel,
              ()) {
    auto file_count = *rc::gen::inRange(1, 4);
    TempWorkspace ws;

    for (int i = 0; i < file_count; ++i) {
        ws.add_meld_file("file" + std::to_string(i) + ".meld");
    }

    SemanticModel model;
    DependencyGraph dep_graph;
    WorkspaceProvider provider(model, dep_graph);

    auto discovered = provider.discover_and_index(ws.root);

    for (const auto& f : discovered) {
        RC_ASSERT(model.has_file(f));
    }
    RC_ASSERT(model.file_count() == discovered.size());
}

}  // namespace
}  // namespace meld::daemon
