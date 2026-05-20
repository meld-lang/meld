#include "meld/manifest/tombstone.hpp"

#include <cstring>
#include <fstream>

namespace meld::manifest {

namespace {

void write_len_str(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    buf.push_back(static_cast<uint8_t>(len & 0xFF));
    buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    buf.insert(buf.end(), s.begin(), s.end());
}

std::string read_len_str(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) return "";
    uint32_t len = static_cast<uint32_t>(ptr[0]) |
                   (static_cast<uint32_t>(ptr[1]) << 8) |
                   (static_cast<uint32_t>(ptr[2]) << 16) |
                   (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    if (ptr + len > end) return "";
    std::string s(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return s;
}

}  // namespace

std::vector<uint8_t> serialize_tombstone(const Tombstone& ts) {
    std::vector<uint8_t> buf;
    buf.push_back('T'); buf.push_back('M'); buf.push_back('B'); buf.push_back('S');
    write_len_str(buf, ts.manifest_hash);
    write_len_str(buf, ts.signature_blob);
    write_len_str(buf, ts.effect_id);
    write_len_str(buf, ts.debug_id);
    return buf;
}

std::optional<Tombstone> deserialize_tombstone(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return std::nullopt;
    if (data[0] != 'T' || data[1] != 'M' || data[2] != 'B' || data[3] != 'S')
        return std::nullopt;

    const uint8_t* ptr = data.data() + 4;
    const uint8_t* end = data.data() + data.size();

    Tombstone ts;
    ts.manifest_hash = read_len_str(ptr, end);
    ts.signature_blob = read_len_str(ptr, end);
    ts.effect_id = read_len_str(ptr, end);
    // debug_id is the fourth field — gracefully handle older tombstones without it
    if (ptr < end) {
        ts.debug_id = read_len_str(ptr, end);
    }
    return ts;
}

bool embed_tombstone_elf(const std::filesystem::path& binary, const Tombstone& ts) {
    // In production: use libelf to add .note.meld section
    // Fallback: write to co-located .tombstone file
    auto ts_path = binary;
    ts_path.replace_extension(".tombstone");
    auto data = serialize_tombstone(ts);
    std::ofstream ofs(ts_path, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return true;
}

bool embed_tombstone_macho(const std::filesystem::path& binary, const Tombstone& ts) {
    // Same fallback approach for now
    return embed_tombstone_elf(binary, ts);
}

std::optional<Tombstone> read_tombstone_elf(const std::filesystem::path& binary) {
    auto ts_path = binary;
    ts_path.replace_extension(".tombstone");
    std::ifstream ifs(ts_path, std::ios::binary);
    if (!ifs.is_open()) return std::nullopt;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
    return deserialize_tombstone(data);
}

std::optional<Tombstone> read_tombstone_macho(const std::filesystem::path& binary) {
    return read_tombstone_elf(binary);
}

bool embed_tombstone(const std::filesystem::path& binary, const Tombstone& ts) {
#ifdef __APPLE__
    return embed_tombstone_macho(binary, ts);
#else
    return embed_tombstone_elf(binary, ts);
#endif
}

std::optional<Tombstone> read_tombstone(const std::filesystem::path& binary) {
#ifdef __APPLE__
    return read_tombstone_macho(binary);
#else
    return read_tombstone_elf(binary);
#endif
}

}  // namespace meld::manifest
