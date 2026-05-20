#pragma once

#include "meld/manifest/core_types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::manifest {

/// The .meld manifest sidecar — binary-serialized effect map + code hash.
struct Manifest {
    uint32_t format_version{1};
    std::string project_name;
    std::string project_version;
    std::string code_hash;  // SHA-256 of binary executable segments
    std::vector<SymbolEffectEntry> symbols;

    bool operator==(const Manifest& other) const {
        return format_version == other.format_version &&
               project_name == other.project_name &&
               project_version == other.project_version &&
               code_hash == other.code_hash &&
               symbols == other.symbols;
    }
};

/// Serialize a Manifest to binary format
std::vector<uint8_t> serialize_manifest(const Manifest& m);

/// Deserialize a Manifest from binary format
std::optional<Manifest> deserialize_manifest(const std::vector<uint8_t>& data);

/// Compute SHA-256 hash of a binary's executable segments
std::string compute_code_hash(const std::filesystem::path& binary_path);

/// Compute SHA-256 hash of arbitrary file content
std::string compute_file_hash(const std::filesystem::path& path);

/// Emits a .meld manifest sidecar from compiled symbol table + effect analysis.
class ManifestEmitter {
public:
    ManifestEmitter() = default;

    /// Set project metadata
    void set_project(const std::string& name, const std::string& version);

    /// Add a symbol with its effects
    void add_symbol(SymbolEffectEntry entry);

    /// Emit the manifest for a compiled binary
    Manifest emit(const std::filesystem::path& binary_path) const;

    /// Write manifest to a sidecar file (binary_path with .meld extension)
    void write_sidecar(const std::filesystem::path& binary_path) const;

private:
    std::string project_name_;
    std::string project_version_;
    std::vector<SymbolEffectEntry> symbols_;
};

}  // namespace meld::manifest
