#include "meld/manifest/signing.hpp"

#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>

namespace meld::manifest {

std::string compute_combined_hash(const std::string& code_hash,
                                  const std::string& manifest_hash,
                                  const std::string& debug_id) {
    // H_total = Hash(code_hash + manifest_hash + debug_id)
    std::hash<std::string> hasher;
    auto h = hasher(code_hash + manifest_hash + debug_id);
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(16) << h;
    return hex.str();
}

SigningResult SigningEngine::sign_with_key(
    const std::vector<uint8_t>& data,
    const std::filesystem::path& private_key) const {

    // Read private key
    std::ifstream ifs(private_key);
    if (!ifs.is_open()) {
        return {false, "", "Cannot open private key: " + private_key.string()};
    }

    // Simplified: HMAC-like signature using std::hash
    // Production: RSA/ECDSA signature
    std::hash<std::string> hasher;
    std::string key_content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
    std::string data_str(data.begin(), data.end());
    auto h = hasher(key_content + data_str);

    std::ostringstream hex;
    hex << "sig:" << std::hex << std::setfill('0') << std::setw(16) << h;
    return {true, hex.str(), ""};
}

SigningResult SigningEngine::sign_with_sigstore(
    const std::vector<uint8_t>& data) const {

    // Simplified: generate a mock Sigstore bundle
    // Production: OIDC flow → Fulcio cert → sign → Rekor log
    std::hash<std::string> hasher;
    std::string data_str(data.begin(), data.end());
    auto h = hasher(data_str);

    std::ostringstream hex;
    hex << "sigstore:" << std::hex << std::setfill('0') << std::setw(16) << h;
    return {true, hex.str(), ""};
}

VerifyResult SigningEngine::verify_with_key(
    const std::vector<uint8_t>& data,
    const std::string& signature_blob,
    const std::filesystem::path& public_key) const {

    // Re-sign and compare
    auto re_signed = sign_with_key(data, public_key);
    if (!re_signed.success) {
        return {false, "key_read", re_signed.error_message};
    }
    if (re_signed.signature_blob != signature_blob) {
        return {false, "signature_mismatch", "Signature does not match"};
    }
    return {true, "", ""};
}

VerifyResult SigningEngine::verify_sigstore_offline(
    const std::vector<uint8_t>& /*data*/,
    const std::string& signature_blob) const {

    // Simplified: check that signature starts with "sigstore:"
    if (signature_blob.find("sigstore:") != 0) {
        return {false, "invalid_bundle", "Not a valid Sigstore bundle"};
    }
    return {true, "", ""};
}

VerifyResult SigningEngine::verify_sigstore_online(
    const std::vector<uint8_t>& data,
    const std::string& signature_blob) const {

    // Online mode: first do offline check, then would query Rekor
    auto offline = verify_sigstore_offline(data, signature_blob);
    if (!offline.valid) return offline;
    // Production: query Rekor transparency log for revocation
    return {true, "", ""};
}

bool SigningEngine::sign_and_embed(
    const std::filesystem::path& binary_path,
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& key_path) const {

    // Read manifest bytes
    std::ifstream ifs(manifest_path, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::vector<uint8_t> manifest_bytes((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());

    // Compute the three components of the combined hash
    auto code_hash = compute_code_hash(binary_path);
    auto manifest_hash = compute_file_hash(manifest_path);
    auto debug_id = compute_code_hash(binary_path);  // debug_id derived from binary content

    // Compute combined integrity hash
    auto combined = compute_combined_hash(code_hash, manifest_hash, debug_id);
    std::vector<uint8_t> combined_bytes(combined.begin(), combined.end());

    // Sign the combined hash
    auto result = sign_with_key(combined_bytes, key_path);
    if (!result.success) return false;

    // Create tombstone with all fields
    Tombstone ts;
    ts.manifest_hash = manifest_hash;
    ts.signature_blob = result.signature_blob;
    ts.effect_id = code_hash;
    ts.debug_id = debug_id;

    // Embed
    return embed_tombstone(binary_path, ts);
}

}  // namespace meld::manifest
