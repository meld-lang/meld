#include "meld/history/version_storage.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace meld {
namespace history {

VersionStorage::VersionStorage(const std::filesystem::path& versionsDir)
    : versionsDir_(versionsDir) {
    std::filesystem::create_directories(versionsDir_);
}

std::filesystem::path VersionStorage::getVersionPath(const std::string& nodeId, int versionNumber) {
    std::ostringstream filename;
    filename << nodeId << "-v" << versionNumber << ".meld";
    return versionsDir_ / filename.str();
}

bool VersionStorage::saveVersion(const std::string& nodeId,
                                 const CodeVersion& version) {
    auto path = getVersionPath(nodeId, version.versionNumber);
    std::ofstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    std::string content = serializeVersion(version);
    file << content;
    file.close();
    
    return true;
}

std::optional<CodeVersion> VersionStorage::loadVersion(const std::string& nodeId,
                                                       int versionNumber) {
    auto path = getVersionPath(nodeId, versionNumber);
    
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return deserializeVersion(buffer.str());
}

bool VersionStorage::versionExists(const std::string& nodeId, int versionNumber) {
    return std::filesystem::exists(getVersionPath(nodeId, versionNumber));
}

bool VersionStorage::deleteVersion(const std::string& nodeId, int versionNumber) {
    auto path = getVersionPath(nodeId, versionNumber);
    
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    return std::filesystem::remove(path);
}

bool VersionStorage::deleteAllVersions(const std::string& nodeId) {
    bool success = true;
    
    // Find all version files for this node
    for (const auto& entry : std::filesystem::directory_iterator(versionsDir_)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find(nodeId) == 0 && filename.find("-v") != std::string::npos) {
                if (!std::filesystem::remove(entry.path())) {
                    success = false;
                }
            }
        }
    }
    
    return success;
}

std::string VersionStorage::serializeVersion(const CodeVersion& version) {
    std::ostringstream content;
    
    // Write metadata header as comments
    content << "// Version: " << version.versionNumber << "\n";
    content << "// Timestamp: ";
    
    auto time = std::chrono::system_clock::to_time_t(version.timestamp);
    content << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S") << "\n";
    
    content << "// Change Reason: " << version.changeReason << "\n";
    content << "// ----------------------------------------\n\n";
    
    // Write the actual code
    content << version.code;
    
    return content.str();
}

std::optional<CodeVersion> VersionStorage::deserializeVersion(const std::string& content) {
    CodeVersion version;
    std::istringstream stream(content);
    std::string line;
    
    // Parse metadata from comments
    while (std::getline(stream, line)) {
        if (line.find("// Version:") == 0) {
            version.versionNumber = std::stoi(line.substr(12));
        } else if (line.find("// Timestamp:") == 0) {
            std::string tsStr = line.substr(14);
            std::tm tm = {};
            std::stringstream ss(tsStr);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            auto time = std::mktime(&tm);
            version.timestamp = std::chrono::system_clock::from_time_t(time);
        } else if (line.find("// Change Reason:") == 0) {
            version.changeReason = line.substr(18);
        } else if (line.find("// ----------------------------------------") == 0) {
            // End of metadata, rest is code
            break;
        }
    }
    
    // Read the code (rest of the file)
    std::ostringstream codeStream;
    while (std::getline(stream, line)) {
        codeStream << line << "\n";
    }
    version.code = codeStream.str();
    
    // Remove trailing newline if present
    if (!version.code.empty() && version.code.back() == '\n') {
        version.code.pop_back();
    }
    
    return version;
}

} // namespace history
} // namespace meld
