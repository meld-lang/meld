#include "meld/manifest/manifest.hpp"

#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>

namespace meld::manifest {

namespace {

void write_u32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

uint32_t read_u32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

void write_string(std::vector<uint8_t>& buf, const std::string& s) {
    write_u32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

std::string read_string(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) return "";
    uint32_t len = read_u32(ptr);
    ptr += 4;
    if (ptr + len > end) return "";
    std::string s(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return s;
}

}  // namespace

std::vector<uint8_t> serialize_manifest(const Manifest& m) {
    std::vector<uint8_t> buf;
    // Magic bytes
    buf.push_back('M'); buf.push_back('E'); buf.push_back('L'); buf.push_back('D');
    write_u32(buf, m.format_version);
    write_string(buf, m.project_name);
    write_string(buf, m.project_version);
    write_string(buf, m.code_hash);
    write_u32(buf, static_cast<uint32_t>(m.symbols.size()));
    for (const auto& sym : m.symbols) {
        write_string(buf, sym.symbol_name);
        buf.push_back(sym.effects.raw());
        buf.push_back(sym.is_ffi ? 1 : 0);
        // Resource bounds
        write_u32(buf, static_cast<uint32_t>(sym.bounds.allowed_domains.size()));
        for (const auto& d : sym.bounds.allowed_domains) write_string(buf, d);
        write_u32(buf, static_cast<uint32_t>(sym.bounds.read_paths.size()));
        for (const auto& p : sym.bounds.read_paths) write_string(buf, p);
        write_u32(buf, static_cast<uint32_t>(sym.bounds.write_paths.size()));
        for (const auto& p : sym.bounds.write_paths) write_string(buf, p);
        write_u32(buf, static_cast<uint32_t>(sym.bounds.exec_paths.size()));
        for (const auto& p : sym.bounds.exec_paths) write_string(buf, p);
    }
    return buf;
}

std::optional<Manifest> deserialize_manifest(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return std::nullopt;
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    // Check magic
    if (ptr[0] != 'M' || ptr[1] != 'E' || ptr[2] != 'L' || ptr[3] != 'D')
        return std::nullopt;
    ptr += 4;

    Manifest m;
    m.format_version = read_u32(ptr); ptr += 4;
    m.project_name = read_string(ptr, end);
    m.project_version = read_string(ptr, end);
    m.code_hash = read_string(ptr, end);

    if (ptr + 4 > end) return std::nullopt;
    uint32_t sym_count = read_u32(ptr); ptr += 4;

    for (uint32_t i = 0; i < sym_count; ++i) {
        SymbolEffectEntry sym;
        sym.symbol_name = read_string(ptr, end);
        if (ptr + 2 > end) return std::nullopt;
        sym.effects = EffectBitmask(*ptr++);
        sym.is_ffi = (*ptr++ != 0);

        auto read_string_vec = [&]() -> std::vector<std::string> {
            if (ptr + 4 > end) return {};
            uint32_t count = read_u32(ptr); ptr += 4;
            std::vector<std::string> vec;
            for (uint32_t j = 0; j < count; ++j)
                vec.push_back(read_string(ptr, end));
            return vec;
        };
        sym.bounds.allowed_domains = read_string_vec();
        sym.bounds.read_paths = read_string_vec();
        sym.bounds.write_paths = read_string_vec();
        sym.bounds.exec_paths = read_string_vec();
        m.symbols.push_back(std::move(sym));
    }
    return m;
}

std::string compute_code_hash(const std::filesystem::path& binary_path) {
    // Simplified: hash entire file. Production: hash only executable segments.
    return compute_file_hash(binary_path);
}

std::string compute_file_hash(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    auto content = oss.str();
    // Simplified hash using std::hash. Production: SHA-256.
    std::hash<std::string> hasher;
    auto h = hasher(content);
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(16) << h;
    return hex.str();
}

void ManifestEmitter::set_project(const std::string& name, const std::string& version) {
    project_name_ = name;
    project_version_ = version;
}

void ManifestEmitter::add_symbol(SymbolEffectEntry entry) {
    symbols_.push_back(std::move(entry));
}

Manifest ManifestEmitter::emit(const std::filesystem::path& binary_path) const {
    Manifest m;
    m.format_version = 1;
    m.project_name = project_name_;
    m.project_version = project_version_;
    m.code_hash = compute_code_hash(binary_path);
    m.symbols = symbols_;
    return m;
}

void ManifestEmitter::write_sidecar(const std::filesystem::path& binary_path) const {
    auto manifest = emit(binary_path);
    auto data = serialize_manifest(manifest);
    auto sidecar_path = binary_path;
    sidecar_path.replace_extension(".meld");
    std::ofstream ofs(sidecar_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

}  // namespace meld::manifest
