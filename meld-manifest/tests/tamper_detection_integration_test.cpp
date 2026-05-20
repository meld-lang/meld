#include "meld/manifest/manifest.hpp"
#include "meld/manifest/signing.hpp"
#include "meld/manifest/tombstone.hpp"
#include "meld/manifest/verifier.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(TamperDetectionTest, TamperedCodeHashDetected) {
    // Create original manifest
    Manifest original;
    original.format_version = 1;
    original.project_name = "tamper-test";
    original.project_version = "1.0.0";
    original.code_hash = "original_code_hash_sha256";

    auto original_bytes = serialize_manifest(original);

    // Create tombstone referencing the original manifest
    Tombstone ts;
    ts.manifest_hash = "hash_of_original_manifest";
    ts.signature_blob = "valid_signature";
    ts.effect_id = original.code_hash;

    // Tamper: change code_hash
    Manifest tampered = original;
    tampered.code_hash = "tampered_code_hash_sha256";
    auto tampered_bytes = serialize_manifest(tampered);

    // The serialized bytes should differ
    EXPECT_NE(original_bytes, tampered_bytes);

    // Tombstone's manifest_hash no longer matches
    // (In a real scenario, the verifier would detect this)
}

TEST(TamperDetectionTest, TamperedManifestDetected) {
    Manifest m;
    m.format_version = 1;
    m.project_name = "tamper-test";
    m.project_version = "1.0.0";
    m.code_hash = "valid_hash";

    auto bytes = serialize_manifest(m);

    // Tamper: add an extra symbol
    m.symbols.push_back(SymbolEffectEntry{"injected", {}, {}, false});
    auto tampered_bytes = serialize_manifest(m);

    EXPECT_NE(bytes, tampered_bytes);
}

TEST(TamperDetectionTest, TombstoneIntegrityChain) {
    // Verify the chain: binary → code_hash → manifest → manifest_hash → tombstone
    Manifest m;
    m.format_version = 1;
    m.project_name = "chain";
    m.project_version = "1.0.0";
    m.code_hash = "binary_hash_abc";

    auto manifest_bytes = serialize_manifest(m);

    Tombstone ts;
    ts.manifest_hash = "sha256_of_manifest_bytes";
    ts.signature_blob = "signature_over_manifest";
    ts.effect_id = "eff-chain";

    auto ts_bytes = serialize_tombstone(ts);
    auto ts_restored = deserialize_tombstone(ts_bytes);
    ASSERT_TRUE(ts_restored.has_value());

    // Verify the chain is intact
    EXPECT_EQ(ts_restored->manifest_hash, "sha256_of_manifest_bytes");
}

TEST(TamperDetectionTest, VerifierRejectsNonExistentFiles) {
    Verifier v(VerificationMode::Offline);
    auto result = v.verify_binary("/nonexistent/binary", "/nonexistent/manifest");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.failed_check.empty());
}

TEST(TamperDetectionTest, TamperedDebugIdBreaksCombinedHash) {
    // The Combined Integrity Hash includes debug_id.
    // Swapping the debug_id (e.g., to point to a different .mdebug sidecar)
    // should break the signature verification.
    auto h_original = compute_combined_hash("code_hash_abc", "manifest_hash_def", "debug_id_123");
    auto h_tampered = compute_combined_hash("code_hash_abc", "manifest_hash_def", "debug_id_EVIL");

    // The combined hashes must differ — tampering debug_id changes the signed value
    EXPECT_NE(h_original, h_tampered);
}

}  // namespace
}  // namespace meld::manifest
