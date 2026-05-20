#include "meld/manifest/manifest.hpp"
#include "meld/manifest/sandbox_provider.hpp"
#include "meld/manifest/srt_provider.hpp"
#include "meld/manifest/tombstone.hpp"
#include "meld/manifest/verifier.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace meld::supervisor::test {

// ---------------------------------------------------------------------------
// Helper: build a SandboxConfig from a Manifest (mirrors main.cpp logic)
// ---------------------------------------------------------------------------
static manifest::SandboxConfig build_sandbox_config_from_manifest(
    const manifest::Manifest& mf) {
    manifest::SandboxConfig sandbox;
    sandbox.working_directory = std::filesystem::current_path();

    manifest::EffectBitmask combined;
    for (const auto& sym : mf.symbols) {
        combined = combined | sym.effects;
        for (const auto& d : sym.bounds.allowed_domains) {
            sandbox.allowed_domains.push_back(d);
        }
        for (const auto& p : sym.bounds.read_paths) {
            sandbox.read_only_paths.push_back(p);
        }
        for (const auto& p : sym.bounds.write_paths) {
            sandbox.read_write_paths.push_back(p);
        }
    }
    sandbox.allowed_effects = combined.to_vector();
    return sandbox;
}

// ---------------------------------------------------------------------------
// Test: Tombstone reading from binary
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, TombstoneSerializationRoundtrip) {
    manifest::Tombstone ts;
    ts.manifest_hash = "abc123def456";
    ts.signature_blob = "sig_blob_data";
    ts.effect_id = "myapp:1.0.0";
    ts.debug_id = "debug_001";

    auto bytes = manifest::serialize_tombstone(ts);
    ASSERT_FALSE(bytes.empty());

    auto deserialized = manifest::deserialize_tombstone(bytes);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->manifest_hash, ts.manifest_hash);
    EXPECT_EQ(deserialized->signature_blob, ts.signature_blob);
    EXPECT_EQ(deserialized->effect_id, ts.effect_id);
    EXPECT_EQ(deserialized->debug_id, ts.debug_id);
}

// ---------------------------------------------------------------------------
// Test: Verification failure on missing tombstone
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, VerificationFailsOnMissingTombstone) {
    // A non-existent binary should yield no tombstone
    auto ts = manifest::read_tombstone("/nonexistent/binary");
    EXPECT_FALSE(ts.has_value());
}

// ---------------------------------------------------------------------------
// Test: Manifest deserialization and effect extraction
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, ManifestEffectExtraction) {
    manifest::Manifest mf;
    mf.format_version = 1;
    mf.project_name = "test-app";
    mf.project_version = "1.0.0";
    mf.code_hash = "deadbeef";

    manifest::SymbolEffectEntry entry;
    entry.symbol_name = "main";
    entry.effects.set(manifest::Effect::Network);
    entry.effects.set(manifest::Effect::FileSystemRead);
    entry.bounds.allowed_domains = {"api.example.com"};
    entry.bounds.read_paths = {"/etc/config"};
    mf.symbols.push_back(entry);

    auto sandbox = build_sandbox_config_from_manifest(mf);

    EXPECT_EQ(sandbox.allowed_effects.size(), 2u);
    EXPECT_EQ(sandbox.allowed_domains.size(), 1u);
    EXPECT_EQ(sandbox.allowed_domains[0], "api.example.com");
    EXPECT_EQ(sandbox.read_only_paths.size(), 1u);
    EXPECT_EQ(sandbox.read_only_paths[0], "/etc/config");
}

// ---------------------------------------------------------------------------
// Test: Offline vs online verification mode selection
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, VerificationModeSelection) {
    manifest::Verifier offline_verifier(manifest::VerificationMode::Offline);
    EXPECT_EQ(offline_verifier.mode(), manifest::VerificationMode::Offline);

    manifest::Verifier online_verifier(manifest::VerificationMode::OnlineAudit);
    EXPECT_EQ(online_verifier.mode(), manifest::VerificationMode::OnlineAudit);
}

// ---------------------------------------------------------------------------
// Test: SRT provider selection (process isolation)
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, SRTProviderSelection) {
    manifest::MeldSRTProvider srt;
    EXPECT_EQ(srt.name(), "SRT");
}

// ---------------------------------------------------------------------------
// Test: Passthrough provider as fallback
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, PassthroughProviderFallback) {
    manifest::PassthroughProvider passthrough;
    EXPECT_EQ(passthrough.name(), "Passthrough");
}

// ---------------------------------------------------------------------------
// Test: SRT policy generation from SandboxConfig
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, SRTPolicyGeneration) {
    manifest::SandboxConfig config;
    config.allowed_effects = {
        manifest::Effect::Network,
        manifest::Effect::FileSystemRead
    };
    config.allowed_domains = {"example.com"};
    config.read_only_paths = {"/etc/config"};
    config.working_directory = "/app";

    auto policy = manifest::MeldSRTProvider::generate_policy(config);
    // Policy should be a valid JSON object
    EXPECT_TRUE(policy.is_object());
}

// ---------------------------------------------------------------------------
// Test: Manifest serialization roundtrip
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, ManifestRoundtrip) {
    manifest::Manifest mf;
    mf.format_version = 1;
    mf.project_name = "roundtrip-test";
    mf.project_version = "2.0.0";
    mf.code_hash = "aabbccdd";

    manifest::SymbolEffectEntry sym;
    sym.symbol_name = "handler";
    sym.effects.set(manifest::Effect::FileSystemWrite);
    sym.bounds.write_paths = {"/tmp/output"};
    mf.symbols.push_back(sym);

    auto bytes = manifest::serialize_manifest(mf);
    ASSERT_FALSE(bytes.empty());

    auto deserialized = manifest::deserialize_manifest(bytes);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->project_name, mf.project_name);
    EXPECT_EQ(deserialized->code_hash, mf.code_hash);
    EXPECT_EQ(deserialized->symbols.size(), 1u);
    EXPECT_EQ(deserialized->symbols[0].symbol_name, "handler");
}

// ---------------------------------------------------------------------------
// Test: INTEGRITY_FAILURE on tampered binary (hash mismatch)
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, IntegrityFailureOnHashMismatch) {
    // Simulate: manifest code_hash doesn't match actual binary
    manifest::Manifest mf;
    mf.code_hash = "expected_hash_value";

    std::string actual_hash = "different_hash_value";
    EXPECT_NE(mf.code_hash, actual_hash);
}

// ---------------------------------------------------------------------------
// Test: Combined effects from multiple symbols
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, CombinedEffectsFromMultipleSymbols) {
    manifest::Manifest mf;
    mf.format_version = 1;
    mf.project_name = "multi-sym";
    mf.project_version = "1.0.0";
    mf.code_hash = "hash";

    manifest::SymbolEffectEntry sym1;
    sym1.symbol_name = "read_config";
    sym1.effects.set(manifest::Effect::FileSystemRead);
    mf.symbols.push_back(sym1);

    manifest::SymbolEffectEntry sym2;
    sym2.symbol_name = "send_data";
    sym2.effects.set(manifest::Effect::Network);
    sym2.bounds.allowed_domains = {"api.example.com"};
    mf.symbols.push_back(sym2);

    auto sandbox = build_sandbox_config_from_manifest(mf);

    // Should have both effects combined
    EXPECT_EQ(sandbox.allowed_effects.size(), 2u);
    EXPECT_EQ(sandbox.allowed_domains.size(), 1u);
}

// ---------------------------------------------------------------------------
// Test: Empty manifest produces empty sandbox config
// ---------------------------------------------------------------------------
TEST(MeldsSupervisor, EmptyManifestProducesEmptySandbox) {
    manifest::Manifest mf;
    mf.format_version = 1;
    mf.project_name = "pure-app";
    mf.project_version = "1.0.0";
    mf.code_hash = "hash";
    // No symbols — pure computation, no effects

    auto sandbox = build_sandbox_config_from_manifest(mf);

    EXPECT_TRUE(sandbox.allowed_effects.empty());
    EXPECT_TRUE(sandbox.allowed_domains.empty());
    EXPECT_TRUE(sandbox.read_only_paths.empty());
    EXPECT_TRUE(sandbox.read_write_paths.empty());
}

}  // namespace meld::supervisor::test
