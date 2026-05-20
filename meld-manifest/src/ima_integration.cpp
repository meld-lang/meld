#include "meld/manifest/ima_integration.hpp"

namespace meld::manifest {

bool ImaIntegration::is_available() {
#ifdef __linux__
    // Check for IMA support via /sys/kernel/security/ima
    return std::filesystem::exists("/sys/kernel/security/ima");
#else
    return false;
#endif
}

bool ImaIntegration::register_hash(
    const std::filesystem::path& /*binary*/,
    const std::string& /*hash*/) {
    // In production: write to IMA policy via securityfs
    // This is a privileged operation requiring root
    return false;  // Not implemented — opt-in feature
}

}  // namespace meld::manifest
