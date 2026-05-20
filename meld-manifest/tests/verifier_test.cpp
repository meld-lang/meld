#include "meld/manifest/verifier.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(VerifierTest, DefaultModeIsOffline) {
    Verifier v;
    EXPECT_EQ(v.mode(), VerificationMode::Offline);
}

TEST(VerifierTest, OnlineAuditMode) {
    Verifier v(VerificationMode::OnlineAudit);
    EXPECT_EQ(v.mode(), VerificationMode::OnlineAudit);
}

TEST(VerifierTest, VerifyNonExistentBinary) {
    Verifier v;
    auto result = v.verify_binary("/nonexistent/binary", "/nonexistent/manifest");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.failed_check.empty());
}

TEST(VerifierTest, AuditLogPopulated) {
    Verifier v;
    v.verify_binary("/nonexistent/binary", "/nonexistent/manifest");
    EXPECT_FALSE(v.audit_log().empty());
}

TEST(VerifierTest, SetTrustRoot) {
    Verifier v;
    v.set_trust_root("/path/to/trust/root");
    // Should not crash; trust root is used during verification
}

TEST(VerifierTest, VerifyWithTamperedManifestPath) {
    Verifier v;
    // Both paths invalid — should fail with structured result
    auto result = v.verify_binary("/tmp/some_binary", "/tmp/wrong_manifest");
    EXPECT_FALSE(result.valid);
}

}  // namespace
}  // namespace meld::manifest
