#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::manifest {

/// Lifecycle state of a reference at a given instruction boundary.
enum class LifecycleState : uint8_t {
    Valid              = 0,
    Moved              = 1,
    PotentiallyDangling = 2,
};

/// A single entry in the AST-to-PC index.
/// Maps a machine code address range to an AST node selector.
struct AstToPcEntry {
    uint64_t pc_start{0};
    uint64_t pc_end{0};
    std::string ast_selector;  // JSONPath-like path to AST node

    bool operator==(const AstToPcEntry& other) const {
        return pc_start == other.pc_start &&
               pc_end == other.pc_end &&
               ast_selector == other.ast_selector;
    }
};

/// A single entry in the Ownership State Trace table.
/// Records compile-time ARC state for a reference at an instruction boundary.
struct OwnershipTraceEntry {
    uint64_t instruction_offset{0};
    uint32_t reference_id{0};
    uint32_t strong_count_status{0};  // 0 = unknown, 1+ = known count
    uint32_t weak_count_status{0};
    LifecycleState lifecycle{LifecycleState::Valid};

    bool operator==(const OwnershipTraceEntry& other) const {
        return instruction_offset == other.instruction_offset &&
               reference_id == other.reference_id &&
               strong_count_status == other.strong_count_status &&
               weak_count_status == other.weak_count_status &&
               lifecycle == other.lifecycle;
    }
};

/// The .mdebug debug sidecar — a compressed archive containing stripped
/// DWARF data, AST-to-PC index, and Ownership State Traces.
/// Co-located with the binary or stored in a symbol cache.
struct MdebugSidecar {
    static constexpr uint32_t kCurrentVersion = 1;

    uint32_t format_version{kCurrentVersion};
    std::string debug_id;  // Content-addressable hash matching Tombstone debug_id

    /// Stripped DWARF debug data (source locations, function names, types, locals)
    std::vector<uint8_t> dwarf_data;

    /// AST-to-PC index: maps PC ranges to ast_selector paths
    std::vector<AstToPcEntry> ast_to_pc_index;

    /// Ownership State Trace: compile-time ARC state per reference per instruction
    std::vector<OwnershipTraceEntry> ownership_traces;

    bool operator==(const MdebugSidecar& other) const {
        return format_version == other.format_version &&
               debug_id == other.debug_id &&
               dwarf_data == other.dwarf_data &&
               ast_to_pc_index == other.ast_to_pc_index &&
               ownership_traces == other.ownership_traces;
    }
};

/// Serialize an MdebugSidecar to bytes with zstd compression.
std::vector<uint8_t> serialize_mdebug(const MdebugSidecar& sidecar);

/// Deserialize an MdebugSidecar from zstd-compressed bytes.
/// Returns nullopt on invalid magic, version mismatch, or corruption.
std::optional<MdebugSidecar> deserialize_mdebug(const std::vector<uint8_t>& data);

/// Compute a debug_id (content-addressable hash) from a binary's code segments.
/// Identical builds produce the same debug_id.
std::string compute_debug_id(const std::filesystem::path& binary_path);

/// Write an MdebugSidecar to a file (binary_path with .mdebug extension).
bool write_mdebug_file(const std::filesystem::path& binary_path,
                       const MdebugSidecar& sidecar);

/// Read an MdebugSidecar from a .mdebug file.
std::optional<MdebugSidecar> read_mdebug_file(const std::filesystem::path& mdebug_path);

}  // namespace meld::manifest
