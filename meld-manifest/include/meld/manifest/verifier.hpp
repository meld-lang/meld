#pragma once

#include "meld/manifest/manifest.hpp"
#include "meld/manifest/signing.hpp"
#include "meld/manifest/tombstone.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace meld::manifest {

/// Verification mode
enum class VerificationMode { Offline, OnlineAudit };

/// Audit log entry for verification events
struct AuditEntry {
    std::filesystem::path binary_path;
    std::string check_name;
    std::string expected;
    std::string actual;
    bool passed{false};
    std::string timestamp;
};

/// Pre-execution integrity verifier (Req 7).
class Verifier {
public:
    explicit Verifier(VerificationMode mode = VerificationMode::Offline);

    /// Full verification pipeline: tombstone + manifest + signature + hashes
    VerifyResult verify_binary(const std::filesystem::path& binary_path,
                               const std::filesystem::path& manifest_path) const;

    /// Set the trust root for signature verification
    void set_trust_root(const std::filesystem::path& path);

    /// Get audit log
    const std::vector<AuditEntry>& audit_log() const { return audit_log_; }

    VerificationMode mode() const { return mode_; }

private:
    VerificationMode mode_;
    std::filesystem::path trust_root_;
    mutable std::vector<AuditEntry> audit_log_;
    SigningEngine signing_engine_;
};

}  // namespace meld::manifest
