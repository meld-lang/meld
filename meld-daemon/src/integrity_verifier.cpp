#include "meld/daemon/integrity_verifier.hpp"

#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>

namespace meld::daemon {

IntegrityVerifier::IntegrityVerifier(VerificationMode mode)
    : mode_(mode) {}

VerificationResult IntegrityVerifier::verify(
    const std::filesystem::path& binary_path,
    const std::filesystem::path& manifest_path) const {

    VerificationResult result;
    result.binary_path = binary_path;

    // 1. Read tombstone from binary's .note.meld section (Req 6.1)
    auto tombstone = read_tombstone(binary_path);
    if (!tombstone) {
        result.failed_check = "tombstone_read";
        result.expected_value = "valid .note.meld section";
        result.actual_value = "section not found or unreadable";
        log_failure(binary_path, "tombstone_read", result.expected_value, result.actual_value);
        return result;
    }

    // 2. Read manifest (Req 6.1)
    auto manifest = read_manifest(manifest_path);
    if (!manifest) {
        result.failed_check = "manifest_read";
        result.expected_value = "valid .meld manifest";
        result.actual_value = "manifest not found or unreadable";
        log_failure(binary_path, "manifest_read", result.expected_value, result.actual_value);
        return result;
    }

    // 3. Verify signature (Req 6.2)
    if (!verify_signature(*tombstone, manifest_path)) {
        result.failed_check = "signature";
        result.expected_value = "valid signature";
        result.actual_value = "signature verification failed";
        log_failure(binary_path, "signature", result.expected_value, result.actual_value);
        return result;
    }

    // 4. Verify code_hash (Req 6.3)
    auto actual_code_hash = compute_code_hash(binary_path);
    if (actual_code_hash != manifest->code_hash) {
        result.failed_check = "code_hash";
        result.expected_value = manifest->code_hash;
        result.actual_value = actual_code_hash;
        log_failure(binary_path, "code_hash", manifest->code_hash, actual_code_hash);
        return result;
    }

    // 5. Verify manifest_hash (Req 6.4)
    auto actual_manifest_hash = compute_hash(manifest_path);
    if (actual_manifest_hash != tombstone->manifest_hash) {
        result.failed_check = "manifest_hash";
        result.expected_value = tombstone->manifest_hash;
        result.actual_value = actual_manifest_hash;
        log_failure(binary_path, "manifest_hash", tombstone->manifest_hash, actual_manifest_hash);
        return result;
    }

    // All checks passed
    result.passed = true;
    return result;
}

void IntegrityVerifier::set_trust_root(const std::filesystem::path& trust_root) {
    trust_root_ = trust_root;
}

std::optional<Tombstone> IntegrityVerifier::read_tombstone(
    const std::filesystem::path& binary) const {
    // In production: read ELF .note.meld section using libelf or similar
    // For now, look for a co-located .tombstone file as a stand-in
    auto tombstone_path = binary;
    tombstone_path.replace_extension(".tombstone");

    std::ifstream ifs(tombstone_path);
    if (!ifs.is_open()) return std::nullopt;

    Tombstone ts;
    std::string line;
    while (std::getline(ifs, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "signature_blob") ts.signature_blob = val;
        else if (key == "manifest_hash") ts.manifest_hash = val;
        else if (key == "code_hash") ts.code_hash = val;
        else if (key == "signer_identity") ts.signer_identity = val;
    }
    return ts;
}

std::optional<ManifestData> IntegrityVerifier::read_manifest(
    const std::filesystem::path& manifest) const {
    std::ifstream ifs(manifest);
    if (!ifs.is_open()) return std::nullopt;

    ManifestData md;
    std::string line;
    while (std::getline(ifs, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "code_hash") md.code_hash = val;
        else if (key == "effect_map") md.effect_map_json = val;
        else if (key == "version") md.version = val;
    }
    return md;
}

bool IntegrityVerifier::verify_signature(const Tombstone& tombstone,
                                         const std::filesystem::path& /*manifest_path*/) const {
    // In production: verify using Sigstore or traditional PKI
    // For now, accept any non-empty signature
    return !tombstone.signature_blob.empty();
}

std::string IntegrityVerifier::compute_hash(const std::filesystem::path& path) const {
    // Simplified hash: use std::hash on file content
    // In production: SHA-256
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return "";

    std::ostringstream oss;
    oss << ifs.rdbuf();
    auto content = oss.str();

    std::hash<std::string> hasher;
    auto h = hasher(content);
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(16) << h;
    return hex.str();
}

std::string IntegrityVerifier::compute_code_hash(
    const std::filesystem::path& binary) const {
    // In production: hash only executable segments (ELF .text, .rodata, etc.)
    // For now, hash the entire file
    return compute_hash(binary);
}

void IntegrityVerifier::log_failure(const std::filesystem::path& binary,
                                    const std::string& check,
                                    const std::string& expected,
                                    const std::string& actual) const {
    // Req 6.5: Structured audit log entry
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts;
    ts << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");

    AuditLogEntry entry;
    entry.binary_path = binary;
    entry.check_name = check;
    entry.expected = expected;
    entry.actual = actual;
    entry.timestamp = ts.str();
    audit_log_.push_back(std::move(entry));
}

}  // namespace meld::daemon
