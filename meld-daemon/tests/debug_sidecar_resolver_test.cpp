#include "meld/daemon/debug_sidecar_resolver.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

class DebugSidecarResolverTest : public ::testing::Test {
protected:
    DebugSidecarResolver resolver;
    fs::path tmp_dir;

    void SetUp() override {
        tmp_dir = fs::temp_directory_path() / "meld-dsr-test";
        fs::create_directories(tmp_dir);
        resolver.set_workspace_root(tmp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir, ec);
    }

    void write_file(const fs::path& path, const std::string& content) {
        fs::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
    }
};

// --- Co-located sidecar ---

TEST_F(DebugSidecarResolverTest, FindsColocatedSidecar) {
    auto binary = tmp_dir / "app";
    auto mdebug = tmp_dir / "app.mdebug";
    write_file(binary, "binary");
    write_file(mdebug, "debug data");

    auto result = resolver.resolve("some-debug-id", binary);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, mdebug);
}

TEST_F(DebugSidecarResolverTest, ColocatedTakesPriorityOverCache) {
    auto binary = tmp_dir / "app";
    auto colocated = tmp_dir / "app.mdebug";
    auto cached = tmp_dir / ".meld" / "debug" / "some-id.mdebug";
    write_file(binary, "binary");
    write_file(colocated, "colocated data");
    write_file(cached, "cached data");

    auto result = resolver.resolve("some-id", binary);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, colocated);
}

// --- Project debug cache fallback ---

TEST_F(DebugSidecarResolverTest, FallsBackToProjectCache) {
    auto debug_id = std::string("deadbeef1234");
    auto cached = tmp_dir / ".meld" / "debug" / (debug_id + ".mdebug");
    write_file(cached, "cached debug data");

    // No binary path provided — can't check co-located
    auto result = resolver.resolve(debug_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, cached);
}

TEST_F(DebugSidecarResolverTest, FallsBackToProjectCacheWhenNoColocated) {
    auto binary = tmp_dir / "app";
    write_file(binary, "binary");
    // No app.mdebug co-located

    auto debug_id = std::string("cafe0001");
    auto cached = tmp_dir / ".meld" / "debug" / (debug_id + ".mdebug");
    write_file(cached, "cached");

    auto result = resolver.resolve(debug_id, binary);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, cached);
}

// --- Bazel-bin fallback ---

TEST_F(DebugSidecarResolverTest, FallsBackToBazelBin) {
    auto debug_id = std::string("bazel-id-42");
    auto bazel_path = tmp_dir / "bazel-bin" / "pkg" / (debug_id + ".mdebug");
    write_file(bazel_path, "bazel debug data");

    auto result = resolver.resolve(debug_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, bazel_path);
}

TEST_F(DebugSidecarResolverTest, BazelBinIsLastResort) {
    auto debug_id = std::string("fallback-id");
    auto binary = tmp_dir / "app";
    write_file(binary, "binary");
    // No co-located, no project cache
    auto bazel_path = tmp_dir / "bazel-bin" / "sub" / (debug_id + ".mdebug");
    write_file(bazel_path, "bazel data");

    auto result = resolver.resolve(debug_id, binary);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, bazel_path);
}

// --- Not found ---

TEST_F(DebugSidecarResolverTest, ReturnsNulloptWhenNotFound) {
    auto result = resolver.resolve("nonexistent-id");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DebugSidecarResolverTest, ReturnsNulloptWithBinaryButNoSidecar) {
    auto binary = tmp_dir / "app";
    write_file(binary, "binary");
    auto result = resolver.resolve("missing-id", binary);
    EXPECT_FALSE(result.has_value());
}

// --- Caching ---

TEST_F(DebugSidecarResolverTest, CacheHitAvoidsDuplicateSearch) {
    auto debug_id = std::string("cached-id");
    auto cached = tmp_dir / ".meld" / "debug" / (debug_id + ".mdebug");
    write_file(cached, "data");

    // First resolve populates cache
    auto r1 = resolver.resolve(debug_id);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(resolver.cache_size(), 1u);

    // Second resolve hits cache
    auto r2 = resolver.resolve(debug_id);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r1, *r2);
    EXPECT_EQ(resolver.cache_size(), 1u);
}

TEST_F(DebugSidecarResolverTest, StaleCacheEntryReSearches) {
    auto debug_id = std::string("stale-id");
    auto cached = tmp_dir / ".meld" / "debug" / (debug_id + ".mdebug");
    write_file(cached, "data");

    // Populate cache
    auto r1 = resolver.resolve(debug_id);
    ASSERT_TRUE(r1.has_value());

    // Delete the file — cache entry becomes stale
    fs::remove(cached);

    // Resolve should detect stale entry and re-search (finding nothing)
    auto r2 = resolver.resolve(debug_id);
    EXPECT_FALSE(r2.has_value());
}

// --- Invalidation ---

TEST_F(DebugSidecarResolverTest, InvalidateAllClearsCache) {
    auto id1 = std::string("id-1");
    auto id2 = std::string("id-2");
    write_file(tmp_dir / ".meld" / "debug" / (id1 + ".mdebug"), "d1");
    write_file(tmp_dir / ".meld" / "debug" / (id2 + ".mdebug"), "d2");

    resolver.resolve(id1);
    resolver.resolve(id2);
    EXPECT_EQ(resolver.cache_size(), 2u);

    resolver.invalidate();
    EXPECT_EQ(resolver.cache_size(), 0u);
}

TEST_F(DebugSidecarResolverTest, InvalidateSingleEntry) {
    auto id1 = std::string("keep-me");
    auto id2 = std::string("remove-me");
    write_file(tmp_dir / ".meld" / "debug" / (id1 + ".mdebug"), "d1");
    write_file(tmp_dir / ".meld" / "debug" / (id2 + ".mdebug"), "d2");

    resolver.resolve(id1);
    resolver.resolve(id2);
    EXPECT_EQ(resolver.cache_size(), 2u);

    resolver.invalidate(id2);
    EXPECT_EQ(resolver.cache_size(), 1u);

    // id1 still cached
    auto r = resolver.resolve(id1);
    ASSERT_TRUE(r.has_value());
}

TEST_F(DebugSidecarResolverTest, InvalidateAfterRebuildFindsNewPath) {
    auto debug_id = std::string("rebuild-id");
    auto old_path = tmp_dir / ".meld" / "debug" / (debug_id + ".mdebug");
    write_file(old_path, "old data");

    auto r1 = resolver.resolve(debug_id);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, old_path);

    // Simulate rebuild: delete old, create in bazel-bin
    fs::remove(old_path);
    auto new_path = tmp_dir / "bazel-bin" / (debug_id + ".mdebug");
    write_file(new_path, "new data");

    // Invalidate and re-resolve
    resolver.invalidate(debug_id);
    auto r2 = resolver.resolve(debug_id);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, new_path);
}

// --- Edge cases ---

TEST_F(DebugSidecarResolverTest, EmptyWorkspaceRootSkipsCacheAndBazel) {
    DebugSidecarResolver bare_resolver;
    // No workspace root set — project cache and bazel-bin searches should
    // return nullopt without crashing.
    auto result = bare_resolver.resolve("any-id");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DebugSidecarResolverTest, EmptyDebugIdReturnsNullopt) {
    auto result = resolver.resolve("");
    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace meld::daemon
