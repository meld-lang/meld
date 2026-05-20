#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// Verification mode for integrity checks
enum class VerificationMode { Offline, Online };

/// Result of an integrity verification check
struct VerificationResult {
    bool passed{false};
    std::string failed_check;       // Empty if passed
    std::string expected_value;     // Expected hash/signature
    std::string actual_value;       // Actual hash/signature
    std::filesystem::path binary_path;
};

/// Audit log entry for verification failures
struct AuditLogEntry {
    std::filesystem::path binary_path;
    std::string check_name;         // "signature", "code_hash", "manifest_hash"
    std::string expected;
    std::string actual;
    std::string timestamp;
};

/// Tombstone data read from .note.meld ELF section
struct Tombstone {
    std::string signature_blob;
    std::string manifest_hash;
    std::string code_hash;
    std::string signer_identity;
};

/// Manifest data read from co-located .meld manifest file
struct ManifestData {
    std::string code_hash;
    std::string effect_map_json;
    std::string version;
};

/// Pre-execution integrity verifier.
/// Reads the target binary's .note.meld Tombstone and co-located .meld manifest,
/// verifies signature, code_hash, and manifest_hash before allowing execution.
class IntegrityVerifier {
public:
    explicit IntegrityVerifier(VerificationMode mode = VerificationMode::Offline);
    ~IntegrityVerifier() = default;

    /// Verify a binary before execution. Returns the verification result.
    VerificationResult verify(const std::filesystem::path& binary_path,
                              const std::filesystem::path& manifest_path) const;

    /// Get the verification mode
    VerificationMode mode() const { return mode_; }

    /// Set the trust root (public key path or Sigstore bundle)
    void set_trust_root(const std::filesystem::path& trust_root);

    /// Get audit log entries for failed verifications
    const std::vector<AuditLogEntry>& audit_log() const { return audit_log_; }

private:
    /// Read the .note.meld Tombstone section from an ELF binary
    std::optional<Tombstone> read_tombstone(const std::filesystem::path& binary) const;

    /// Read and parse the .meld manifest file
    std::optional<ManifestData> read_manifest(const std::filesystem::path& manifest) const;

    /// Verify the signature blob against manifest content
    bool verify_signature(const Tombstone& tombstone,
                          const std::filesystem::path& manifest_path) const;

    /// Compute SHA-256 hash of a file or file segment
    std::string compute_hash(const std::filesystem::path& path) const;

    /// Compute hash of executable segments only
    std::string compute_code_hash(const std::filesystem::path& binary) const;

    /// Log a verification failure
    void log_failure(const std::filesystem::path& binary,
                     const std::string& check,
                     const std::string& expected,
                     const std::string& actual) const;

    VerificationMode mode_;
    std::filesystem::path trust_root_;
    mutable std::vector<AuditLogEntry> audit_log_;
};

}  // namespace meld::daemon
