#include "meld/manifest/manifest.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

Manifest make_test_manifest() {
    Manifest m;
    m.format_version = 1;
    m.project_name = "test-project";
    m.project_version = "0.1.0";
    m.code_hash = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

    SymbolEffectEntry entry;
    entry.symbol_name = "main";
    entry.effects.set(Effect::FileSystemRead);
    entry.bounds.read_paths = {"/tmp"};
    m.symbols.push_back(entry);

    return m;
}

TEST(ManifestTest, SerializeDeserializeRoundTrip) {
    auto original = make_test_manifest();
    auto bytes = serialize_manifest(original);
    ASSERT_FALSE(bytes.empty());

    auto restored = deserialize_manifest(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
}

TEST(ManifestTest, FormatVersionPresent) {
    auto m = make_test_manifest();
    auto bytes = serialize_manifest(m);
    auto restored = deserialize_manifest(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->format_version, 1u);
}

TEST(ManifestTest, EmptyManifestRoundTrip) {
    Manifest m;
    m.format_version = 1;
    m.project_name = "empty";
    m.project_version = "0.0.0";
    m.code_hash = "";

    auto bytes = serialize_manifest(m);
    auto restored = deserialize_manifest(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(m, *restored);
}

TEST(ManifestTest, MultipleSymbols) {
    Manifest m;
    m.format_version = 1;
    m.project_name = "multi";
    m.project_version = "1.0.0";
    m.code_hash = "deadbeef";

    SymbolEffectEntry e1;
    e1.symbol_name = "read_file";
    e1.effects.set(Effect::FileSystemRead);

    SymbolEffectEntry e2;
    e2.symbol_name = "send_data";
    e2.effects.set(Effect::Network);
    e2.bounds.allowed_domains = {"api.example.com"};

    SymbolEffectEntry e3;
    e3.symbol_name = "c_bridge";
    e3.effects.set_all();
    e3.is_ffi = true;

    m.symbols = {e1, e2, e3};

    auto bytes = serialize_manifest(m);
    auto restored = deserialize_manifest(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(m, *restored);
}

TEST(ManifestTest, DeserializeInvalidData) {
    std::vector<uint8_t> garbage{0xFF, 0xFE, 0x00, 0x01};
    auto result = deserialize_manifest(garbage);
    EXPECT_FALSE(result.has_value());
}

TEST(ManifestTest, DeserializeEmptyData) {
    std::vector<uint8_t> empty;
    auto result = deserialize_manifest(empty);
    EXPECT_FALSE(result.has_value());
}

TEST(ManifestEmitterTest, SetProjectAndEmit) {
    ManifestEmitter emitter;
    emitter.set_project("my-app", "2.0.0");

    SymbolEffectEntry entry;
    entry.symbol_name = "do_io";
    entry.effects.set(Effect::Network);
    emitter.add_symbol(entry);

    // emit() requires a real binary path for code_hash; test the emitter state
    // by verifying it doesn't crash with a non-existent path
    // (compute_code_hash returns empty string for missing files)
    auto m = emitter.emit("/nonexistent/binary");
    EXPECT_EQ(m.project_name, "my-app");
    EXPECT_EQ(m.project_version, "2.0.0");
    EXPECT_EQ(m.symbols.size(), 1u);
    EXPECT_EQ(m.symbols[0].symbol_name, "do_io");
}

TEST(CodeHashTest, NonExistentFile) {
    auto hash = compute_code_hash("/nonexistent/file");
    EXPECT_TRUE(hash.empty());
}

TEST(FileHashTest, NonExistentFile) {
    auto hash = compute_file_hash("/nonexistent/file");
    EXPECT_TRUE(hash.empty());
}

}  // namespace
}  // namespace meld::manifest
