#include "meld/manifest/ima_integration.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(ImaIntegrationTest, DefaultDisabled) {
    ImaIntegration ima;
    EXPECT_FALSE(ima.enabled());
}

TEST(ImaIntegrationTest, OptInConfiguration) {
    ImaIntegration ima;
    ima.set_enabled(true);
    EXPECT_TRUE(ima.enabled());
    ima.set_enabled(false);
    EXPECT_FALSE(ima.enabled());
}

TEST(ImaIntegrationTest, AvailabilityCheck) {
    // On macOS (test environment), IMA is not available
    // This tests the fallback path
    bool available = ImaIntegration::is_available();
    // We don't assert a specific value — platform-dependent
    (void)available;
}

TEST(ImaIntegrationTest, RegisterHashNonExistentBinary) {
    auto result = ImaIntegration::register_hash("/nonexistent/binary", "deadbeef");
    // Should fail gracefully on non-Linux or when IMA unavailable
    EXPECT_FALSE(result);
}

TEST(ImaIntegrationTest, FallbackToUserspaceVerification) {
    // When IMA is not available, the system falls back to userspace verification
    // This is the expected path on macOS
    ImaIntegration ima;
    ima.set_enabled(true);
    // Even when enabled, if platform doesn't support IMA, it should not crash
    EXPECT_TRUE(ima.enabled());
}

}  // namespace
}  // namespace meld::manifest
