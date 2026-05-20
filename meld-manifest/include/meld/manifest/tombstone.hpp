#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::manifest {

/// Tombstone data embedded in .note.meld ELF section or Mach-O section.
struct Tombstone {
    std::string manifest_hash;   // SHA-256 of manifest file
    std::string signature_blob;  // Cryptographic signature
    std::string effect_id;       // Unique SRT policy lookup ID
    std::string debug_id;        // Links binary to its .mdebug sidecar

    bool operator==(const Tombstone& other) const {
        return manifest_hash == other.manifest_hash &&
               signature_blob == other.signature_blob &&
               effect_id == other.effect_id &&
               debug_id == other.debug_id;
    }
};

/// Embed a Tombstone into an ELF binary's .note.meld section
bool embed_tombstone_elf(const std::filesystem::path& binary, const Tombstone& ts);

/// Embed a Tombstone into a Mach-O binary's __DATA,__meld section
bool embed_tombstone_macho(const std::filesystem::path& binary, const Tombstone& ts);

/// Read a Tombstone from an ELF binary's .note.meld section
std::optional<Tombstone> read_tombstone_elf(const std::filesystem::path& binary);

/// Read a Tombstone from a Mach-O binary
std::optional<Tombstone> read_tombstone_macho(const std::filesystem::path& binary);

/// Platform-agnostic: embed tombstone using the appropriate format
bool embed_tombstone(const std::filesystem::path& binary, const Tombstone& ts);

/// Platform-agnostic: read tombstone
std::optional<Tombstone> read_tombstone(const std::filesystem::path& binary);

/// Serialize Tombstone to bytes (for file-based fallback)
std::vector<uint8_t> serialize_tombstone(const Tombstone& ts);

/// Deserialize Tombstone from bytes
std::optional<Tombstone> deserialize_tombstone(const std::vector<uint8_t>& data);

}  // namespace meld::manifest
