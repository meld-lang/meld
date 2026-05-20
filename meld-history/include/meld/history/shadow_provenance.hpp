#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <memory>

namespace meld {
namespace history {

using Timestamp = std::chrono::system_clock::time_point;

/**
 * Represents a single message in an AI conversation
 */
struct Message {
    std::string role;        // "user" or "assistant"
    std::string content;     // Message content
    Timestamp timestamp;     // When the message was sent
    
    Message() = default;
    Message(std::string r, std::string c, Timestamp t)
        : role(std::move(r)), content(std::move(c)), timestamp(t) {}
};

/**
 * Represents a previous version of code
 */
struct CodeVersion {
    int versionNumber;           // Sequential version number
    std::string code;            // The code at this version
    Timestamp timestamp;         // When this version was created
    std::string changeReason;    // Why the code was changed
    
    CodeVersion() : versionNumber(0) {}
    CodeVersion(int vn, std::string c, Timestamp t, std::string r)
        : versionNumber(vn), code(std::move(c)), timestamp(t), changeReason(std::move(r)) {}
};

/**
 * Represents the complete history for an AST node
 */
struct HistoryEntry {
    std::string nodeId;              // AST node identifier
    std::string functionName;        // Name of the function
    std::string filePath;            // Path to the source file
    Timestamp createdAt;             // When first created
    Timestamp updatedAt;             // Last update time
    std::string agentModel;          // AI model used (e.g., "gpt-4")
    
    // Conversation history
    std::vector<Message> conversation;
    
    // Previous code versions
    std::vector<CodeVersion> versions;
    
    // Blueprint evolution tracking
    std::optional<std::string> originalBlueprint;
    std::optional<std::string> currentBlueprint;
    
    // Paths to external storage
    std::string conversationPath;    // Path to conversation JSON file
    
    HistoryEntry() = default;
    
    // Get version count
    int getVersionCount() const {
        return static_cast<int>(versions.size());
    }
    
    // Check if this is original code (no versions)
    bool isOriginal() const {
        return versions.empty();
    }
    
    // Get the latest version
    std::optional<CodeVersion> getLatestVersion() const {
        if (versions.empty()) return std::nullopt;
        return versions.back();
    }
};

/**
 * Blueprint history tracking
 */
struct BlueprintHistory {
    std::string nodeId;
    std::string originalBlueprint;
    std::string currentBlueprint;
    std::vector<std::string> evolutionSteps;  // Intermediate blueprints
    Timestamp lastModified;
    
    BlueprintHistory() = default;
};

} // namespace history
} // namespace meld
