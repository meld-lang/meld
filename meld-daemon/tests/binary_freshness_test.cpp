#include "meld/daemon/binary_freshness.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

class BinaryFreshnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "meld_freshness_test";
        fs::create_directories(tmp_dir_);

        // Create a source file
        src_path_ = tmp_dir_ / "main.meld";
        write_file(src_path_, "fn main() -> Int { 42 }");

        // Index it in the model
        FileSemantics sem;
        sem.path = src_path_;
        sem.ast = std::make_shared<ASTNode>();
        sem.ast->kind = "function_definition";
        sem.ast->name = "main";
        model_.update_file(src_path_, std::move(sem));

        // Create a binary (older than source initially)
        bin_path_ = tmp_dir_ / "main";
        write_file(bin_path_, "BINARY");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    void write_file(const fs::path& p, const std::string& content) {
        std::ofstream f(p);
        f << content;
    }

    void touch_file(const fs::path& p) {
        // Make the file newer by rewriting it
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        write_file(p, "UPDATED");
    }

    fs::path tmp_dir_;
    fs::path src_path_;
    fs::path bin_path_;
    SemanticModel model_;
};

TEST_F(BinaryFreshnessTest, CurrentBinaryReturnsIsCurrent) {
    // Touch binary to make it newer than source
    touch_file(bin_path_);

    BinaryFreshnessChecker checker(model_);
    auto result = checker.check(bin_path_);

    EXPECT_TRUE(result.is_current);
    EXPECT_TRUE(result.stale_sources.empty());
    EXPECT_FALSE(result.rebuild_triggered);
}

TEST_F(BinaryFreshnessTest, StaleBinaryDetected) {
    // Touch source to make it newer than binary
    touch_file(src_path_);

    BinaryFreshnessChecker checker(model_);
    // No worker → no rebuild triggered
    auto result = checker.check(bin_path_);

    // Without a worker, rebuild can't happen, so it stays stale
    EXPECT_FALSE(result.stale_sources.empty());
    EXPECT_TRUE(result.rebuild_triggered);  // Attempted but no worker
}

TEST_F(BinaryFreshnessTest, NonexistentBinaryIsStale) {
    auto missing = tmp_dir_ / "nonexistent";
    BinaryFreshnessChecker checker(model_);
    auto result = checker.check(missing);

    EXPECT_FALSE(result.is_current);
}

TEST_F(BinaryFreshnessTest, CacheHitReturnsImmediately) {
    touch_file(bin_path_);  // Make binary current

    BinaryFreshnessChecker checker(model_);
    checker.set_debounce_window(std::chrono::milliseconds(500));

    auto r1 = checker.check(bin_path_);
    EXPECT_TRUE(r1.is_current);

    // Touch source to make it stale — but cache should still return current
    touch_file(src_path_);
    auto r2 = checker.check(bin_path_);
    EXPECT_TRUE(r2.is_current);  // Cached result
}

TEST_F(BinaryFreshnessTest, CacheInvalidationOnFileChange) {
    touch_file(bin_path_);  // Make binary current

    BinaryFreshnessChecker checker(model_);
    checker.set_debounce_window(std::chrono::milliseconds(5000));

    auto r1 = checker.check(bin_path_);
    EXPECT_TRUE(r1.is_current);

    // Invalidate cache for this source
    checker.invalidate(src_path_);

    // Now touch source and re-check — should not use cache
    touch_file(src_path_);
    auto r2 = checker.check(bin_path_);
    EXPECT_FALSE(r2.stale_sources.empty());
}

TEST_F(BinaryFreshnessTest, InvalidateAllClearsCache) {
    touch_file(bin_path_);

    BinaryFreshnessChecker checker(model_);
    checker.set_debounce_window(std::chrono::milliseconds(5000));

    checker.check(bin_path_);
    checker.invalidate_all();

    touch_file(src_path_);
    auto result = checker.check(bin_path_);
    EXPECT_FALSE(result.stale_sources.empty());
}

}  // namespace
}  // namespace meld::daemon
