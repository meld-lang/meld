#include <gtest/gtest.h>
#include <algorithm>
#include "meld/daemon/workspace_manager.hpp"

namespace meld::lsp::workspace {

TEST(WorkspaceManagerTest, AddAndRemoveFolder) {
    WorkspaceManager mgr;
    mgr.add_workspace_folder("/project");
    EXPECT_EQ(mgr.workspace_folders().size(), 1u);
    mgr.remove_workspace_folder("/project");
    EXPECT_TRUE(mgr.workspace_folders().empty());
}

TEST(WorkspaceManagerTest, DuplicateFolderNotAdded) {
    WorkspaceManager mgr;
    mgr.add_workspace_folder("/project");
    mgr.add_workspace_folder("/project");
    EXPECT_EQ(mgr.workspace_folders().size(), 1u);
}

TEST(WorkspaceManagerTest, UpdateAndGetDocument) {
    WorkspaceManager mgr;
    mgr.update_document("file:///test.meld", "fnc main() {}", 1);
    auto doc = mgr.get_document("file:///test.meld");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->content, "fnc main() {}");
    EXPECT_EQ(doc->version, 1);
}

TEST(WorkspaceManagerTest, UpdateExistingDocument) {
    WorkspaceManager mgr;
    mgr.update_document("file:///test.meld", "v1", 1);
    mgr.update_document("file:///test.meld", "v2", 2);
    auto doc = mgr.get_document("file:///test.meld");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->content, "v2");
    EXPECT_EQ(doc->version, 2);
}

TEST(WorkspaceManagerTest, CloseDocument) {
    WorkspaceManager mgr;
    mgr.update_document("file:///test.meld", "content", 1);
    mgr.close_document("file:///test.meld");
    EXPECT_EQ(mgr.get_document("file:///test.meld"), nullptr);
}

TEST(WorkspaceManagerTest, GetNonexistentDocument) {
    WorkspaceManager mgr;
    EXPECT_EQ(mgr.get_document("file:///nonexistent.meld"), nullptr);
}

TEST(WorkspaceManagerTest, IsInWorkspace) {
    WorkspaceManager mgr;
    mgr.add_workspace_folder("/project");
    EXPECT_TRUE(mgr.is_in_workspace("file:///project/src/main.meld"));
    EXPECT_FALSE(mgr.is_in_workspace("file:///other/main.meld"));
}

// --- Batch analysis tests ---

TEST(WorkspaceManagerTest, AllDocumentUris) {
    WorkspaceManager mgr;
    mgr.update_document("file:///a.meld", "fnc a() {}", 1);
    mgr.update_document("file:///b.meld", "fnc b() {}", 1);

    auto uris = mgr.all_document_uris();
    EXPECT_EQ(uris.size(), 2u);

    // Both URIs should be present (order not guaranteed)
    std::sort(uris.begin(), uris.end());
    EXPECT_EQ(uris[0], "file:///a.meld");
    EXPECT_EQ(uris[1], "file:///b.meld");
}

TEST(WorkspaceManagerTest, AllDocumentUrisEmpty) {
    WorkspaceManager mgr;
    EXPECT_TRUE(mgr.all_document_uris().empty());
}

TEST(WorkspaceManagerTest, AnalyzeAllCallsAnalyzer) {
    WorkspaceManager mgr;
    mgr.update_document("file:///a.meld", "fnc a() {}", 1);
    mgr.update_document("file:///b.meld", "fnc b() {}", 1);

    std::vector<std::string> analyzed_uris;
    auto result = mgr.analyze_all([&](const std::string& uri, const std::string& /*content*/) {
        analyzed_uris.push_back(uri);
    });

    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(analyzed_uris.size(), 2u);
}

TEST(WorkspaceManagerTest, AnalyzeAllSkipsEmptyDocuments) {
    WorkspaceManager mgr;
    mgr.update_document("file:///a.meld", "fnc a() {}", 1);
    mgr.update_document("file:///empty.meld", "", 1);

    std::vector<std::string> analyzed_uris;
    auto result = mgr.analyze_all([&](const std::string& uri, const std::string& /*content*/) {
        analyzed_uris.push_back(uri);
    });

    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(analyzed_uris.size(), 1u);
    EXPECT_EQ(analyzed_uris[0], "file:///a.meld");
}

TEST(WorkspaceManagerTest, AnalyzeAllEmpty) {
    WorkspaceManager mgr;
    auto result = mgr.analyze_all([](const std::string&, const std::string&) {});
    EXPECT_TRUE(result.empty());
}

} // namespace meld::lsp::workspace
