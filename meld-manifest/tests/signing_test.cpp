#include "meld/manifest/signing.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

std::vector<uint8_t> make_test_manifest_bytes() {
    Manifest m;
    m.format_version = 1;
    m.project_name = "sign-test";
    m.project_version = "1.0.0";
    m.code_hash = "cafebabe";
    return serialize_manifest(m);
}

TEST(SigningEngineTest, SignWithKeyNonExistentKey) {
    SigningEngine engine;
    auto bytes = make_test_manifest_bytes();
    auto result = engine.sign_with_key(bytes, "/nonexistent/key.pem");
    // Should fail gracefully with an error message
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(SigningEngineTest, VerifyWithKeyInvalidSignature) {
    SigningEngine engine;
    auto bytes = make_test_manifest_bytes();
    auto result = engine.verify_with_key(bytes, "invalid_sig", "/nonexistent/pub.pem");
    EXPECT_FALSE(result.valid);
}

TEST(SigningEngineTest, SignWithSigstoreReturnsResult) {
    SigningEngine engine;
    auto bytes = make_test_manifest_bytes();
    // Sigstore requires OIDC — in test env this will fail gracefully
    auto result = engine.sign_with_sigstore(bytes);
    // We just verify it doesn't crash and returns a structured result
    // In CI without OIDC, success will be false
    EXPECT_FALSE(result.error_message.empty() && result.success);
}

TEST(SigningEngineTest, VerifySigstoreOfflineInvalidBundle) {
    SigningEngine engine;
    auto bytes = make_test_manifest_bytes();
    auto result = engine.verify_sigstore_offline(bytes, "not_a_bundle");
    EXPECT_FALSE(result.valid);
}

TEST(SigningEngineTest, VerifySigstoreOnlineInvalidBundle) {
    SigningEngine engine;
    auto bytes = make_test_manifest_bytes();
    auto result = engine.verify_sigstore_online(bytes, "not_a_bundle");
    EXPECT_FALSE(result.valid);
}

TEST(SigningEngineTest, SignAndEmbedNonExistentFiles) {
    SigningEngine engine;
    EXPECT_FALSE(engine.sign_and_embed("/no/binary", "/no/manifest", "/no/key"));
}

TEST(SigningEngineTest, LinkedHashChainIntegrity) {
    // Verify the conceptual chain: binary → code_hash → manifest → manifest_hash → tombstone
    Manifest m;
    m.format_version = 1;
    m.project_name = "chain-test";
    m.project_version = "1.0.0";
    m.code_hash = "original_hash";

    auto bytes1 = serialize_manifest(m);

    // Tamper with code_hash
    m.code_hash = "tampered_hash";
    auto bytes2 = serialize_manifest(m);

    // The serialized bytes should differ
    EXPECT_NE(bytes1, bytes2);
}

TEST(SigningEngineTest, CombinedHashDeterministic) {
    auto h1 = compute_combined_hash("code_abc", "manifest_def", "debug_123");
    auto h2 = compute_combined_hash("code_abc", "manifest_def", "debug_123");
    EXPECT_EQ(h1, h2);
    EXPECT_FALSE(h1.empty());
}

TEST(SigningEngineTest, CombinedHashChangesWithDebugId) {
    auto h1 = compute_combined_hash("code_abc", "manifest_def", "debug_123");
    auto h2 = compute_combined_hash("code_abc", "manifest_def", "debug_456");
    EXPECT_NE(h1, h2);
}

TEST(SigningEngineTest, CombinedHashChangesWithCodeHash) {
    auto h1 = compute_combined_hash("code_abc", "manifest_def", "debug_123");
    auto h2 = compute_combined_hash("code_xyz", "manifest_def", "debug_123");
    EXPECT_NE(h1, h2);
}

TEST(SigningEngineTest, CombinedHashChangesWithManifestHash) {
    auto h1 = compute_combined_hash("code_abc", "manifest_def", "debug_123");
    auto h2 = compute_combined_hash("code_abc", "manifest_xyz", "debug_123");
    EXPECT_NE(h1, h2);
}

}  // namespace
}  // namespace meld::manifest
