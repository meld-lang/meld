#pragma once

#include <filesystem>
#include <string>

namespace meld::manifest {

/// Hardware-level integrity via IMA/TPM (Req 9, optional).
class ImaIntegration {
public:
    /// Check if IMA is available on this platform
    static bool is_available();

    /// Register a binary hash with IMA
    static bool register_hash(const std::filesystem::path& binary,
                              const std::string& hash);

    /// Check if IMA integration is enabled in config
    bool enabled() const { return enabled_; }
    void set_enabled(bool e) { enabled_ = e; }

private:
    bool enabled_{false};
};

}  // namespace meld::manifest
