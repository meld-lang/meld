#pragma once

#include "shadow_provenance.hpp"
#include "history_manager.hpp"
#include "conversation_storage.hpp"
#include "version_storage.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace meld {
namespace history {

/**
 * Public API for querying shadow history
 * This is the interface exposed to Meld code and IDE integrations
 */
class HistoryAPI {
public:
    explicit HistoryAPI(const std::filesystem::path& historyDir);
    ~HistoryAPI() = default;
    
    // Initialize the history system
    bool initialize();
    
    // Query history by function name
    std::optional<HistoryEntry> query(const std::string& functionName);
    
    // Query history by node ID
    std::optional<HistoryEntry> queryByNodeId(const std::string& nodeId);
    
    // Query history by file path
    std::vector<HistoryEntry> queryByFile(const std::string& filePath);
    
    // Get conversation for a function
    std::vector<Message> getConversation(const std::string& functionName);
    
    // Get conversation by node ID
    std::vector<Message> getConversationByNodeId(const std::string& nodeId);
    
    // Get versions for a function
    std::vector<CodeVersion> getVersions(const std::string& functionName);
    
    // Get versions by node ID
    std::vector<CodeVersion> getVersionsByNodeId(const std::string& nodeId);
    
    // Get blueprint history for a function
    std::optional<BlueprintHistory> getBlueprintHistory(const std::string& functionName);
    
    // Get blueprint history by node ID
    std::optional<BlueprintHistory> getBlueprintHistoryByNodeId(const std::string& nodeId);
    
    // Record new generation (for compiler integration)
    bool recordGeneration(const std::string& nodeId,
                          const std::string& functionName,
                          const std::string& filePath,
                          const std::vector<Message>& conversation,
                          const std::string& agentModel);
    
    // Record refinement (for compiler integration)
    bool recordRefinement(const std::string& nodeId,
                          const std::string& oldCode,
                          const std::string& newCode,
                          const std::string& reason);
    
    // Update blueprint history (for compiler integration)
    bool updateBlueprintHistory(const std::string& nodeId,
                                const std::string& originalBlueprint,
                                const std::string& currentBlueprint);
    
    // Statistics
    int getTotalHistoryCount();
    int getVersionCount(const std::string& functionName);
    int getVersionCountByNodeId(const std::string& nodeId);
    
    // Cleanup
    bool cleanOldHistory(int daysOld);
    bool deleteHistory(const std::string& functionName);
    bool deleteHistoryByNodeId(const std::string& nodeId);
    
    // Export history
    bool exportHistory(const std::string& outputPath, bool includeConversations = true);
    
    // Get manager for advanced operations
    HistoryManager& getManager() { return *manager_; }
    
private:
    std::unique_ptr<HistoryManager> manager_;
    std::unique_ptr<ConversationStorage> conversationStorage_;
    std::unique_ptr<VersionStorage> versionStorage_;
};

} // namespace history
} // namespace meld
