#pragma once

#include "shadow_provenance.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace meld {
namespace history {

/**
 * Handles storage and retrieval of conversation JSON files
 */
class ConversationStorage {
public:
    explicit ConversationStorage(const std::filesystem::path& conversationsDir);
    
    // Save conversation to JSON file
    bool saveConversation(const std::string& nodeId,
                          const std::vector<Message>& conversation,
                          const std::string& agentModel);
    
    // Load conversation from JSON file
    std::optional<std::vector<Message>> loadConversation(const std::string& nodeId);
    
    // Check if conversation exists
    bool conversationExists(const std::string& nodeId);
    
    // Delete conversation file
    bool deleteConversation(const std::string& nodeId);
    
    // Get conversation file path
    std::filesystem::path getConversationPath(const std::string& nodeId);
    
private:
    std::filesystem::path conversationsDir_;
    
    // JSON serialization helpers
    std::string serializeConversation(const std::vector<Message>& conversation,
                                       const std::string& agentModel);
    std::optional<std::vector<Message>> deserializeConversation(const std::string& json);
    std::string timestampToISO8601(const Timestamp& ts);
    Timestamp iso8601ToTimestamp(const std::string& str);
};

} // namespace history
} // namespace meld
