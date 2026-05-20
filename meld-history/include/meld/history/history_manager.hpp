#pragma once

#include "shadow_provenance.hpp"
#include "history_database.hpp"
#include <memory>
#include <filesystem>

namespace meld {
namespace history {

/**
 * Manages recording and retrieval of shadow history
 */
class HistoryManager {
public:
    explicit HistoryManager(const std::filesystem::path& historyDir);
    ~HistoryManager() = default;
    
    // Initialize the history system
    bool initialize();
    
    // Record AI generation
    bool recordGeneration(const std::string& nodeId,
                          const std::string& functionName,
                          const std::string& filePath,
                          const std::vector<Message>& conversation,
                          const std::string& agentModel);
    
    // Record code refinement
    bool recordRefinement(const std::string& nodeId,
                          const std::string& oldCode,
                          const std::string& newCode,
                          const std::string& reason);
    
    // Record version history
    bool recordVersion(const std::string& nodeId,
                       const CodeVersion& version);
    
    // Link history to AST node
    bool linkToNode(const std::string& nodeId,
                    const std::string& functionName,
                    const std::string& filePath);
    
    // Update blueprint history
    bool updateBlueprintHistory(const std::string& nodeId,
                                const std::string& originalBlueprint,
                                const std::string& currentBlueprint);
    
    // Query history
    std::optional<HistoryEntry> getHistory(const std::string& nodeId);
    std::optional<HistoryEntry> getHistoryByFunction(const std::string& functionName);
    std::vector<HistoryEntry> getHistoryByFile(const std::string& filePath);
    
    // Get conversation for a node
    std::vector<Message> getConversation(const std::string& nodeId);
    
    // Get versions for a node
    std::vector<CodeVersion> getVersions(const std::string& nodeId);
    
    // Get blueprint history
    std::optional<BlueprintHistory> getBlueprintHistory(const std::string& nodeId);
    
    // Statistics
    int getTotalHistoryCount();
    int getVersionCount(const std::string& nodeId);
    
    // Cleanup
    bool cleanOldHistory(const Timestamp& olderThan);
    bool deleteHistory(const std::string& nodeId);
    
    // Get paths
    std::filesystem::path getHistoryDir() const { return historyDir_; }
    std::filesystem::path getConversationsDir() const { return conversationsDir_; }
    std::filesystem::path getVersionsDir() const { return versionsDir_; }
    
private:
    std::filesystem::path historyDir_;
    std::filesystem::path conversationsDir_;
    std::filesystem::path versionsDir_;
    std::unique_ptr<HistoryDatabase> database_;
    
    // Helper methods
    std::string generateConversationPath(const std::string& nodeId);
    std::string generateVersionPath(const std::string& nodeId, int versionNumber);
    bool ensureDirectoriesExist();
};

} // namespace history
} // namespace meld
