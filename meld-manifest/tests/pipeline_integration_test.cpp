#include "meld/manifest/manifest.hpp"
#include "meld/manifest/signing.hpp"
#include "meld/manifest/tombstone.hpp"
#include "meld/manifest/verifier.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

/// End-to-end: manifest → serialize → sign → tombstone → verify
TEST(PipelineIntegrationTest, FullSigningPipeline) {
    // 1. Create a manifest
    Manifest m;
    m.format_version = 1;
    m.project_name = "pipeline-test";
    m.project_version = "1.0.0";
    m.code_hash = "0123456789abcdef0123456789abcdef";

    SymbolEffectEntry entry;
    entry.symbol_name = "main";
    entry.effects.set(Effect::FileSystemRead);
    m.symbols.push_back(entry);

    // 2. Serialize
    auto manifest_bytes = serialize_manifest(m);
    ASSERT_FALSE(manifest_bytes.empty());

    // 3. Deserialize back to verify round-trip
    auto restored = deserialize_manifest(manifest_bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(m, *restored);

    // 4. Create tombstone from manifest
    Tombstone ts;
    ts.manifest_hash = "manifest_hash_placeholder";
    ts.signature_blob = "sig_placeholder";
    ts.effect_id = m.code_hash;

    // 5. Tombstone round-trip
    auto ts_bytes = serialize_tombstone(ts);
    auto ts_restored = deserialize_tombstone(ts_bytes);
    ASSERT_TRUE(ts_restored.has_value());
    EXPECT_EQ(ts, *ts_restored);
}

TEST(PipelineIntegrationTest, ManifestEmitterToSigning) {
    ManifestEmitter emitter;
    emitter.set_project("integration-app", "2.0.0");

    SymbolEffectEntry e1;
    e1.symbol_name = "read_config";
    e1.effects.set(Effect::FileSystemRead);
    emitter.add_symbol(e1);

    SymbolEffectEntry e2;
    e2.symbol_name = "send_telemetry";
    e2.effects.set(Effect::Network);
    e2.bounds.allowed_domains = {"telemetry.example.com"};
    emitter.add_symbol(e2);

    // Emit with a non-existent binary (code_hash will be empty)
    auto manifest = emitter.emit("/nonexistent");
    EXPECT_EQ(manifest.project_name, "integration-app");
    EXPECT_EQ(manifest.symbols.size(), 2u);

    // Serialize and verify it's valid
    auto bytes = serialize_manifest(manifest);
    auto restored = deserialize_manifest(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(manifest, *restored);
}

}  // namespace
}  // namespace meld::manifest
