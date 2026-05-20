#include "meld/daemon/file_watcher.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace meld::daemon {
namespace {

class FileWatcherTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_root = std::filesystem::temp_directory_path() / "meld-fw-test";

    void SetUp() override {
        std::filesystem::create_directories(tmp_root);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmp_root, ec);
    }
};

TEST_F(FileWatcherTest, FiltersMeldFiles) {
    FileWatcher watcher(tmp_root, std::chrono::milliseconds(10));
    std::vector<FileChangeEvent> received;

    watcher.start([&](const std::vector<FileChangeEvent>& events) {
        received.insert(received.end(), events.begin(), events.end());
    });

    // Inject .meld file event — should be accepted
    watcher.inject_event({tmp_root / "test.meld", FileChangeType::Modified});
    // Inject .cpp file event — should be filtered out
    watcher.inject_event({tmp_root / "test.cpp", FileChangeType::Modified});
    // Inject meld.toml event — should be accepted
    watcher.inject_event({tmp_root / "meld.toml", FileChangeType::Modified});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    watcher.stop();

    EXPECT_EQ(received.size(), 2u);
}

TEST_F(FileWatcherTest, DebounceCoalescesRapidEvents) {
    FileWatcher watcher(tmp_root, std::chrono::milliseconds(30));
    int callback_count = 0;

    watcher.start([&](const std::vector<FileChangeEvent>&) {
        ++callback_count;
    });

    // Inject rapid events within debounce window
    for (int i = 0; i < 5; ++i) {
        watcher.inject_event({tmp_root / "test.meld", FileChangeType::Modified});
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    watcher.stop();

    // Should have been coalesced into 1-2 callbacks, not 5
    EXPECT_LE(callback_count, 2);
    EXPECT_GE(callback_count, 1);
}

TEST_F(FileWatcherTest, DetectsCreateModifyDelete) {
    FileWatcher watcher(tmp_root, std::chrono::milliseconds(10));
    std::vector<FileChangeEvent> received;

    watcher.start([&](const std::vector<FileChangeEvent>& events) {
        received.insert(received.end(), events.begin(), events.end());
    });

    watcher.inject_event({tmp_root / "new.meld", FileChangeType::Created});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    watcher.inject_event({tmp_root / "new.meld", FileChangeType::Modified});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    watcher.inject_event({tmp_root / "new.meld", FileChangeType::Deleted});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    watcher.stop();

    EXPECT_GE(received.size(), 1u);
}

TEST_F(FileWatcherTest, StopIsIdempotent) {
    FileWatcher watcher(tmp_root, std::chrono::milliseconds(10));
    watcher.start([](const std::vector<FileChangeEvent>&) {});
    watcher.stop();
    watcher.stop();  // Should not crash
}

}  // namespace
}  // namespace meld::daemon
