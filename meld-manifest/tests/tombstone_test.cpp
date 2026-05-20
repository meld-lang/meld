#include "meld/manifest/tombstone.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

Tombstone make_test_tombstone() {
    return Tombstone{
        .manifest_hash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2",
        .signature_blob = "SIGBLOB_BASE64_ENCODED_DATA",
        .effect_id = "eff-001",
    };
}

TEST(TombstoneTest, SerializeDeserializeRoundTrip) {
    auto original = make_test_tombstone();
    auto bytes = serialize_tombstone(original);
    ASSERT_FALSE(bytes.empty());

    auto restored = deserialize_tombstone(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
}

TEST(TombstoneTest, DeserializeInvalidData) {
    std::vector<uint8_t> garbage{0xDE, 0xAD};
    auto result = deserialize_tombstone(garbage);
    EXPECT_FALSE(result.has_value());
}

TEST(TombstoneTest, DeserializeEmptyData) {
    std::vector<uint8_t> empty;
    auto result = deserialize_tombstone(empty);
    EXPECT_FALSE(result.has_value());
}

TEST(TombstoneTest, Equality) {
    auto a = make_test_tombstone();
    auto b = make_test_tombstone();
    EXPECT_EQ(a, b);

    b.manifest_hash = "different";
    EXPECT_NE(a, b);
}

TEST(TombstoneTest, ElfEmbedNonExistentBinary) {
    Tombstone ts = make_test_tombstone();
    EXPECT_FALSE(embed_tombstone_elf("/nonexistent/binary", ts));
}

TEST(TombstoneTest, MachoEmbedNonExistentBinary) {
    Tombstone ts = make_test_tombstone();
    EXPECT_FALSE(embed_tombstone_macho("/nonexistent/binary", ts));
}

TEST(TombstoneTest, ReadElfNonExistentBinary) {
    auto result = read_tombstone_elf("/nonexistent/binary");
    EXPECT_FALSE(result.has_value());
}

TEST(TombstoneTest, ReadMachoNonExistentBinary) {
    auto result = read_tombstone_macho("/nonexistent/binary");
    EXPECT_FALSE(result.has_value());
}

TEST(TombstoneTest, PlatformAgnosticNonExistent) {
    Tombstone ts = make_test_tombstone();
    EXPECT_FALSE(embed_tombstone("/nonexistent/binary", ts));
    EXPECT_FALSE(read_tombstone("/nonexistent/binary").has_value());
}

}  // namespace
}  // namespace meld::manifest
