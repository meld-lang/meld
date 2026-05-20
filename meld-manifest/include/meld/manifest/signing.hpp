#pragma once

#include "meld/manifest/manifest.hpp"
#include "meld/manifest/tombstone.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace meld::manifest {

/// Compute the Combined Integrity Hash: H(code_hash + manifest_hash + debug_id)
/// This is the value that gets signed, ensuring all three artifacts are bound together.
std::string compute_combined_hash(const std::string& code_hash,
                                  const std::string& manifest_hash,
                                  const std::string& debug_id);

/// Result of a signing operation
struct SigningResult {
    bool success{false};
    std::string signature_blob;
    std::string error_message;
};

/// Result of a verification operation
struct VerifyResult {
    bool valid{false};
    std::string failed_check;  // Empty if valid
    std::string details;
};

/// Cryptographic signing engine (Req 5, 6, 8).
/// Supports traditional key-based and Sigstore keyless signing.
/// All signing methods operate on the Combined Integrity Hash, not raw manifest bytes.
class SigningEngine {
public:
    /// Sign combined hash bytes with a private key (traditional)
    SigningResult sign_with_key(const std::vector<uint8_t>& data,
                               const std::filesystem::path& private_key) const;

    /// Sign combined hash bytes with Sigstore (keyless OIDC)
    SigningResult sign_with_sigstore(const std::vector<uint8_t>& data) const;

    /// Verify a signature against combined hash bytes and public key
    VerifyResult verify_with_key(const std::vector<uint8_t>& data,
                                 const std::string& signature_blob,
                                 const std::filesystem::path& public_key) const;

    /// Verify a Sigstore bundle (offline mode)
    VerifyResult verify_sigstore_offline(const std::vector<uint8_t>& data,
                                         const std::string& signature_blob) const;

    /// Verify a Sigstore bundle (online audit mode — checks Rekor)
    VerifyResult verify_sigstore_online(const std::vector<uint8_t>& data,
                                        const std::string& signature_blob) const;

    /// Full pipeline: compute combined hash, sign, create tombstone, embed in binary
    bool sign_and_embed(const std::filesystem::path& binary_path,
                        const std::filesystem::path& manifest_path,
                        const std::filesystem::path& key_path) const;
};

}  // namespace meld::manifest
