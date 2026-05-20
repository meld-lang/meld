#include "meld/manifest/verifier.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace meld::manifest {

Verifier::Verifier(VerificationMode mode) : mode_(mode) {}

VerifyResult Verifier::verify_binary(
    const std::filesystem::path& binary_path,
    const std::filesystem::path& manifest_path) const {

    // 1. Read tombstone
    auto ts = read_tombstone(binary_path);
    if (!ts) {
        audit_log_.push_back({binary_path, "tombstone_read", "present", "missing", false, ""});
        return {false, "tombstone_read", "Cannot read tombstone from binary"};
    }

    // 2. Read manifest
    std::ifstream ifs(manifest_path, std::ios::binary);
    if (!ifs.is_open()) {
        audit_log_.push_back({binary_path, "manifest_read", "present", "missing", false, ""});
        return {false, "manifest_read", "Cannot read manifest file"};
    }
    std::vector<uint8_t> manifest_bytes((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());

    // 3. Verify manifest_hash
    auto actual_manifest_hash = compute_file_hash(manifest_path);
    if (actual_manifest_hash != ts->manifest_hash) {
        audit_log_.push_back({binary_path, "manifest_hash", ts->manifest_hash,
                              actual_manifest_hash, false, ""});
        return {false, "manifest_hash", "Manifest hash mismatch"};
    }

    // 4. Verify code_hash
    auto manifest = deserialize_manifest(manifest_bytes);
    if (!manifest) {
        return {false, "manifest_parse", "Cannot parse manifest"};
    }
    auto actual_code_hash = compute_code_hash(binary_path);
    if (actual_code_hash != manifest->code_hash) {
        audit_log_.push_back({binary_path, "code_hash", manifest->code_hash,
                              actual_code_hash, false, ""});
        return {false, "code_hash", "Code hash mismatch"};
    }

    // 5. Verify signature against Combined Integrity Hash
    //    H_total = Hash(code_hash + manifest_hash + debug_id)
    auto combined = compute_combined_hash(manifest->code_hash,
                                          actual_manifest_hash,
                                          ts->debug_id);
    std::vector<uint8_t> combined_bytes(combined.begin(), combined.end());

    VerifyResult sig_result;
    if (ts->signature_blob.find("sigstore:") == 0) {
        sig_result = (mode_ == VerificationMode::OnlineAudit)
            ? signing_engine_.verify_sigstore_online(combined_bytes, ts->signature_blob)
            : signing_engine_.verify_sigstore_offline(combined_bytes, ts->signature_blob);
    } else if (!trust_root_.empty()) {
        sig_result = signing_engine_.verify_with_key(combined_bytes, ts->signature_blob, trust_root_);
    } else {
        // No trust root configured — accept any non-empty signature
        sig_result = {!ts->signature_blob.empty(), "no_trust_root", ""};
    }

    if (!sig_result.valid) {
        audit_log_.push_back({binary_path, "signature", "valid", "invalid", false, ""});
        return sig_result;
    }

    audit_log_.push_back({binary_path, "full_verification", "", "", true, ""});
    return {true, "", ""};
}

void Verifier::set_trust_root(const std::filesystem::path& path) {
    trust_root_ = path;
}

}  // namespace meld::manifest
