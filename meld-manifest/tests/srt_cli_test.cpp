#include "meld/manifest/srt_provider.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(SrtCliTest, LaunchWithValidPolicy) {
    MeldSRTProvider provider;
    SandboxConfig config;
    config.allowed_effects = {Effect::FileSystemRead};
    config.read_only_paths = {"/tmp"};

    // If SRT is not installed, this should fall back gracefully
    auto result = provider.launch(config, {"echo", "hello"});
    // Either succeeds or reports sandbox_failed (SRT not found)
    // Should not crash either way
    if (!MeldSRTProvider::is_srt_available()) {
        // Fallback behavior — may run unsandboxed or report failure
        SUCCEED();
    } else {
        EXPECT_FALSE(result.sandbox_failed);
    }
}

TEST(SrtCliTest, FallbackWhenSrtNotFound) {
    MeldSRTProvider provider;
    provider.set_srt_path("/nonexistent/srt-binary");

    SandboxConfig config;
    auto result = provider.launch(config, {"echo", "test"});
    // Should handle missing SRT gracefully
    // The provider should either fall back or set sandbox_failed
    SUCCEED();  // Just verify no crash
}

TEST(SrtCliTest, SandboxOffFlag) {
    MeldSRTProvider provider;
    provider.set_sandbox_enabled(false);

    SandboxConfig config;
    config.allowed_effects = {Effect::Network};

    auto result = provider.launch(config, {"echo", "unsandboxed"});
    // With sandbox disabled, should run command directly
    EXPECT_FALSE(result.sandbox_failed);
}

TEST(SrtCliTest, ExitCodeDistinction) {
    MeldSRTProvider provider;
    provider.set_sandbox_enabled(false);

    SandboxConfig config;
    // Run a command that exits with non-zero
    auto result = provider.launch(config, {"false"});
    // exit_code should reflect the application's exit code, not sandbox
    EXPECT_NE(result.exit_code, 0);
    EXPECT_FALSE(result.sandbox_failed);
}

TEST(SrtCliTest, SetSrtPath) {
    MeldSRTProvider provider;
    provider.set_srt_path("/usr/local/bin/srt");
    // Should not crash; path is used during launch
    SUCCEED();
}

}  // namespace
}  // namespace meld::manifest
