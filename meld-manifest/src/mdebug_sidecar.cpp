#include "meld/manifest/mdebug_sidecar.hpp"

#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>

namespace meld::manifest {

namespace {

// --- Wire helpers (little-endian) ---

void write_u32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void write_u64(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 0; i < 8; ++i)
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
}

uint32_t read_u32(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) return 0;
    uint32_t v = static_cast<uint32_t>(ptr[0]) |
                 (static_cast<uint32_t>(ptr[1]) << 8) |
                 (static_cast<uint32_t>(ptr[2]) << 16) |
                 (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return v;
}

uint64_t read_u64(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 8 > end) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    ptr += 8;
    return v;
}

void write_string(std::vector<uint8_t>& buf, const std::string& s) {
    write_u32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

std::string read_string(const uint8_t*& ptr, const uint8_t* end) {
    uint32_t len = read_u32(ptr, end);
    if (ptr + len > end) return "";
    std::string s(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return s;
}

void write_bytes(std::vector<uint8_t>& buf, const std::vector<uint8_t>& data) {
    write_u32(buf, static_cast<uint32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
}

std::vector<uint8_t> read_bytes(const uint8_t*& ptr, const uint8_t* end) {
    uint32_t len = read_u32(ptr, end);
    if (ptr + len > end) return {};
    std::vector<uint8_t> v(ptr, ptr + len);
    ptr += len;
    return v;
}

// --- Simple zstd-like compression stub ---
// In production, link against libzstd. For now, use a trivial
// framing: 4-byte magic "ZSTD" + 4-byte uncompressed size + raw data.
// This allows the round-trip contract to hold while the real zstd
// dependency is wired into Bazel.

constexpr uint8_t kZstdMagic[4] = {'Z', 'S', 'T', 'D'};

std::vector<uint8_t> zstd_compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> out;
    out.insert(out.end(), kZstdMagic, kZstdMagic + 4);
    uint32_t sz = static_cast<uint32_t>(input.size());
    out.push_back(static_cast<uint8_t>(sz & 0xFF));
    out.push_back(static_cast<uint8_t>((sz >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((sz >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((sz >> 24) & 0xFF));
    out.insert(out.end(), input.begin(), input.end());
    return out;
}

std::optional<std::vector<uint8_t>> zstd_decompress(const std::vector<uint8_t>& input) {
    if (input.size() < 8) return std::nullopt;
    if (std::memcmp(input.data(), kZstdMagic, 4) != 0) return std::nullopt;
    uint32_t sz = static_cast<uint32_t>(input[4]) |
                  (static_cast<uint32_t>(input[5]) << 8) |
                  (static_cast<uint32_t>(input[6]) << 16) |
                  (static_cast<uint32_t>(input[7]) << 24);
    if (input.size() < 8 + sz) return std::nullopt;
    return std::vector<uint8_t>(input.begin() + 8, input.begin() + 8 + sz);
}

// Simple SHA-256 stub — in production, use OpenSSL or similar.
// Returns a hex string derived from std::hash for determinism.
std::string sha256_of_bytes(const uint8_t* data, size_t len) {
    std::hash<std::string_view> hasher;
    auto h1 = hasher(std::string_view(reinterpret_cast<const char*>(data), len));
    // Mix to get a second hash for more bits
    auto h2 = hasher(std::string_view(reinterpret_cast<const char*>(&h1), sizeof(h1)));
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << h1
        << std::setw(16) << h2;
    return oss.str();
}

// --- Magic bytes for the uncompressed payload ---
constexpr uint8_t kMdebugMagic[4] = {'M', 'D', 'E', 'B'};

}  // anonymous namespace

std::vector<uint8_t> serialize_mdebug(const MdebugSidecar& sidecar) {
    std::vector<uint8_t> payload;

    // Header: magic + version + debug_id
    payload.insert(payload.end(), kMdebugMagic, kMdebugMagic + 4);
    write_u32(payload, sidecar.format_version);
    write_string(payload, sidecar.debug_id);

    // Section 1: DWARF data blob
    write_bytes(payload, sidecar.dwarf_data);

    // Section 2: AST-to-PC index
    write_u32(payload, static_cast<uint32_t>(sidecar.ast_to_pc_index.size()));
    for (const auto& entry : sidecar.ast_to_pc_index) {
        write_u64(payload, entry.pc_start);
        write_u64(payload, entry.pc_end);
        write_string(payload, entry.ast_selector);
    }

    // Section 3: Ownership State Traces
    write_u32(payload, static_cast<uint32_t>(sidecar.ownership_traces.size()));
    for (const auto& entry : sidecar.ownership_traces) {
        write_u64(payload, entry.instruction_offset);
        write_u32(payload, entry.reference_id);
        write_u32(payload, entry.strong_count_status);
        write_u32(payload, entry.weak_count_status);
        payload.push_back(static_cast<uint8_t>(entry.lifecycle));
    }

    // Compress the entire payload
    return zstd_compress(payload);
}

std::optional<MdebugSidecar> deserialize_mdebug(const std::vector<uint8_t>& data) {
    // Decompress
    auto decompressed = zstd_decompress(data);
    if (!decompressed) return std::nullopt;

    const uint8_t* ptr = decompressed->data();
    const uint8_t* end = ptr + decompressed->size();

    // Validate magic
    if (decompressed->size() < 8) return std::nullopt;
    if (std::memcmp(ptr, kMdebugMagic, 4) != 0) return std::nullopt;
    ptr += 4;

    MdebugSidecar sidecar;
    sidecar.format_version = read_u32(ptr, end);
    if (sidecar.format_version > MdebugSidecar::kCurrentVersion)
        return std::nullopt;

    sidecar.debug_id = read_string(ptr, end);

    // Section 1: DWARF data
    sidecar.dwarf_data = read_bytes(ptr, end);

    // Section 2: AST-to-PC index
    uint32_t ast_count = read_u32(ptr, end);
    sidecar.ast_to_pc_index.reserve(ast_count);
    for (uint32_t i = 0; i < ast_count; ++i) {
        AstToPcEntry entry;
        entry.pc_start = read_u64(ptr, end);
        entry.pc_end = read_u64(ptr, end);
        entry.ast_selector = read_string(ptr, end);
        sidecar.ast_to_pc_index.push_back(std::move(entry));
    }

    // Section 3: Ownership traces
    uint32_t trace_count = read_u32(ptr, end);
    sidecar.ownership_traces.reserve(trace_count);
    for (uint32_t i = 0; i < trace_count; ++i) {
        OwnershipTraceEntry entry;
        entry.instruction_offset = read_u64(ptr, end);
        entry.reference_id = read_u32(ptr, end);
        entry.strong_count_status = read_u32(ptr, end);
        entry.weak_count_status = read_u32(ptr, end);
        if (ptr < end) {
            entry.lifecycle = static_cast<LifecycleState>(*ptr++);
        }
        sidecar.ownership_traces.push_back(std::move(entry));
    }

    return sidecar;
}

std::string compute_debug_id(const std::filesystem::path& binary_path) {
    std::ifstream ifs(binary_path, std::ios::binary);
    if (!ifs.is_open()) return "";
    std::vector<uint8_t> contents((std::istreambuf_iterator<char>(ifs)),
                                   std::istreambuf_iterator<char>());
    return sha256_of_bytes(contents.data(), contents.size());
}

bool write_mdebug_file(const std::filesystem::path& binary_path,
                       const MdebugSidecar& sidecar) {
    auto mdebug_path = binary_path;
    mdebug_path.replace_extension(".mdebug");
    auto data = serialize_mdebug(sidecar);
    std::ofstream ofs(mdebug_path, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return ofs.good();
}

std::optional<MdebugSidecar> read_mdebug_file(const std::filesystem::path& mdebug_path) {
    std::ifstream ifs(mdebug_path, std::ios::binary);
    if (!ifs.is_open()) return std::nullopt;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
    return deserialize_mdebug(data);
}

}  // namespace meld::manifest
