#pragma once

#include "shadow_provenance.hpp"
#include <filesystem>
#include <string>
#include <optional>

namespace meld {
namespace history {

/**
 * Handles storage and retrieval of code version files
 */
class VersionStorage {
public:
    explicit VersionStorage(const std::filesystem::path& versionsDir);
    
    // Save code version to file
    bool saveVersion(const std::string& nodeId,
                     const CodeVersion& version);
    
    // Load code version from file
    std::optional<CodeVersion> loadVersion(const std::string& nodeId,
                                           int versionNumber);
    
    // Check if version exists
    bool versionExists(const std::string& nodeId, int versionNumber);
    
    // Delete version file
    bool deleteVersion(const std::string& nodeId, int versionNumber);
    
    // Delete all versions for a node
    bool deleteAllVersions(const std::string& nodeId);
    
    // Get version file path
    std::filesystem::path getVersionPath(const std::string& nodeId, int versionNumber);
    
private:
    std::filesystem::path versionsDir_;
    
    // File format helpers
    std::string serializeVersion(const CodeVersion& version);
    std::optional<CodeVersion> deserializeVersion(const std::string& content);
};

} // namespace history
} // namespace meld
