#include "meld/manifest/mdebug_sidecar.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace meld::manifest {
namespace {

class MdebugSidecarTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir;

    void SetUp() override {
        tmp_dir = std::filesystem::temp_directory_path() / "meld-mdebug-test";
        std::filesystem::create_directories(tmp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmp_dir, ec);
    }

    void write_file(const std::filesystem::path& path,
                    const std::string& content) {
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
    }

    MdebugSidecar make_populated_sidecar() {
        MdebugSidecar s;
        s.format_version = MdebugSidecar::kCurrentVersion;
        s.debug_id = "abc123def456";
        s.dwarf_data = {0x01, 0x02, 0x03, 0x04, 0xFF};

        s.ast_to_pc_index.push_back(
            AstToPcEntry{0x1000, 0x1040, "$.module.functions[0]"});
        s.ast_to_pc_index.push_back(
            AstToPcEntry{0x2000, 0x2100, "$.module.functions[1].body"});

        s.ownership_traces.push_back(
            OwnershipTraceEntry{0x10, 1, 1, 0, LifecycleState::Valid});
        s.ownership_traces.push_back(
            OwnershipTraceEntry{0x20, 2, 0, 1, LifecycleState::Moved});
        s.ownership_traces.push_back(
            OwnershipTraceEntry{0x30, 3, 0, 0,
                                LifecycleState::PotentiallyDangling});
        return s;
    }
};

// --- Serialization / deserialization round-trip ---

TEST_F(MdebugSidecarTest, RoundTripAllSectionsPopulated) {
    auto original = make_populated_sidecar();
    auto bytes = serialize_mdebug(original);
    ASSERT_FALSE(bytes.empty());

    auto restored = deserialize_mdebug(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
}

TEST_F(MdebugSidecarTest, RoundTripEmptySections) {
    MdebugSidecar original;
    original.debug_id = "empty-test";
    // dwarf_data, ast_to_pc_index, ownership_traces all empty

    auto bytes = serialize_mdebug(original);
    auto restored = deserialize_mdebug(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
}

TEST_F(MdebugSidecarTest, FormatVersionPresent) {
    auto sidecar = make_populated_sidecar();
    auto bytes = serialize_mdebug(sidecar);
    auto restored = deserialize_mdebug(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(MdebugSidecar::kCurrentVersion, restored->format_version);
}

TEST_F(MdebugSidecarTest, FutureVersionRejected) {
    auto sidecar = make_populated_sidecar();
    sidecar.format_version = MdebugSidecar::kCurrentVersion + 99;
    auto bytes = serialize_mdebug(sidecar);
    auto restored = deserialize_mdebug(bytes);
    EXPECT_FALSE(restored.has_value());
}

// --- Compression / decompression ---

TEST_F(MdebugSidecarTest, SerializedDataIsCompressed) {
    // The serialized output should start with the zstd stub magic "ZSTD"
    auto sidecar = make_populated_sidecar();
    auto bytes = serialize_mdebug(sidecar);
    ASSERT_GE(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 'Z');
    EXPECT_EQ(bytes[1], 'S');
    EXPECT_EQ(bytes[2], 'T');
    EXPECT_EQ(bytes[3], 'D');
}

TEST_F(MdebugSidecarTest, DeserializeGarbageReturnsNullopt) {
    std::vector<uint8_t> garbage{0xDE, 0xAD, 0xBE, 0xEF};
    auto result = deserialize_mdebug(garbage);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MdebugSidecarTest, DeserializeEmptyReturnsNullopt) {
    std::vector<uint8_t> empty;
    auto result = deserialize_mdebug(empty);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MdebugSidecarTest, DeserializeTruncatedReturnsNullopt) {
    auto sidecar = make_populated_sidecar();
    auto bytes = serialize_mdebug(sidecar);
    // Truncate to just the compression header
    bytes.resize(8);
    auto result = deserialize_mdebug(bytes);
    EXPECT_FALSE(result.has_value());
}

// --- debug_id computation ---

TEST_F(MdebugSidecarTest, DebugIdDeterministicForIdenticalBinaries) {
    auto binary = tmp_dir / "test_binary";
    write_file(binary, "identical binary content here");

    auto id1 = compute_debug_id(binary);
    auto id2 = compute_debug_id(binary);
    EXPECT_FALSE(id1.empty());
    EXPECT_EQ(id1, id2);
}

TEST_F(MdebugSidecarTest, DebugIdDiffersForDifferentBinaries) {
    auto binary_a = tmp_dir / "binary_a";
    auto binary_b = tmp_dir / "binary_b";
    write_file(binary_a, "content A");
    write_file(binary_b, "content B");

    auto id_a = compute_debug_id(binary_a);
    auto id_b = compute_debug_id(binary_b);
    EXPECT_FALSE(id_a.empty());
    EXPECT_FALSE(id_b.empty());
    EXPECT_NE(id_a, id_b);
}

TEST_F(MdebugSidecarTest, DebugIdEmptyForNonexistentBinary) {
    auto result = compute_debug_id(tmp_dir / "nonexistent");
    EXPECT_TRUE(result.empty());
}

// --- File I/O ---

TEST_F(MdebugSidecarTest, WriteAndReadMdebugFile) {
    auto binary = tmp_dir / "app.bin";
    write_file(binary, "binary content");

    auto original = make_populated_sidecar();
    ASSERT_TRUE(write_mdebug_file(binary, original));

    auto mdebug_path = tmp_dir / "app.mdebug";
    ASSERT_TRUE(std::filesystem::exists(mdebug_path));

    auto restored = read_mdebug_file(mdebug_path);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
}

TEST_F(MdebugSidecarTest, ReadNonexistentMdebugFile) {
    auto result = read_mdebug_file(tmp_dir / "nonexistent.mdebug");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MdebugSidecarTest, WriteToInvalidPathFails) {
    auto bad_path = std::filesystem::path("/nonexistent/dir/app.bin");
    auto sidecar = make_populated_sidecar();
    EXPECT_FALSE(write_mdebug_file(bad_path, sidecar));
}

// --- Independence from .meld manifest ---

TEST_F(MdebugSidecarTest, SidecarIndependentOfManifest) {
    // MdebugSidecar has no fields referencing Manifest — verify the struct
    // can be constructed and round-tripped without any manifest dependency.
    MdebugSidecar sidecar;
    sidecar.debug_id = "standalone-debug-id";
    sidecar.dwarf_data = {0xCA, 0xFE};
    sidecar.ast_to_pc_index.push_back(
        AstToPcEntry{0x100, 0x200, "$.standalone"});
    sidecar.ownership_traces.push_back(
        OwnershipTraceEntry{0x50, 7, 2, 0, LifecycleState::Valid});

    auto bytes = serialize_mdebug(sidecar);
    auto restored = deserialize_mdebug(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(sidecar, *restored);
}

}  // namespace
}  // namespace meld::manifest
