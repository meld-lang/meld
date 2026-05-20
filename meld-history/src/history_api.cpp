#include "meld/history/history_api.hpp"
#include <fstream>

namespace meld {
namespace history {

HistoryAPI::HistoryAPI(const std::filesystem::path& historyDir) {
    manager_ = std::make_unique<HistoryManager>(historyDir);
    conversationStorage_ = std::make_unique<ConversationStorage>(historyDir / "conversations");
    versionStorage_ = std::make_unique<VersionStorage>(historyDir / "versions");
}

bool HistoryAPI::initialize() {
    return manager_->initialize();
}

std::optional<HistoryEntry> HistoryAPI::query(const std::string& functionName) {
    return manager_->getHistoryByFunction(functionName);
}

std::optional<HistoryEntry> HistoryAPI::queryByNodeId(const std::string& nodeId) {
    return manager_->getHistory(nodeId);
}

std::vector<HistoryEntry> HistoryAPI::queryByFile(const std::string& filePath) {
    return manager_->getHistoryByFile(filePath);
}

std::vector<Message> HistoryAPI::getConversation(const std::string& functionName) {
    auto entry = manager_->getHistoryByFunction(functionName);
    if (!entry) {
        return {};
    }
    return entry->conversation;
}

std::vector<Message> HistoryAPI::getConversationByNodeId(const std::string& nodeId) {
    return manager_->getConversation(nodeId);
}

std::vector<CodeVersion> HistoryAPI::getVersions(const std::string& functionName) {
    auto entry = manager_->getHistoryByFunction(functionName);
    if (!entry) {
        return {};
    }
    return entry->versions;
}

std::vector<CodeVersion> HistoryAPI::getVersionsByNodeId(const std::string& nodeId) {
    return manager_->getVersions(nodeId);
}

std::optional<BlueprintHistory> HistoryAPI::getBlueprintHistory(const std::string& functionName) {
    auto entry = manager_->getHistoryByFunction(functionName);
    if (!entry) {
        return std::nullopt;
    }
    return manager_->getBlueprintHistory(entry->nodeId);
}

std::optional<BlueprintHistory> HistoryAPI::getBlueprintHistoryByNodeId(const std::string& nodeId) {
    return manager_->getBlueprintHistory(nodeId);
}

bool HistoryAPI::recordGeneration(const std::string& nodeId,
                                  const std::string& functionName,
                                  const std::string& filePath,
                                  const std::vector<Message>& conversation,
                                  const std::string& agentModel) {
    // Record in database
    if (!manager_->recordGeneration(nodeId, functionName, filePath, conversation, agentModel)) {
        return false;
    }
    
    // Save conversation to JSON file
    if (!conversationStorage_->saveConversation(nodeId, conversation, agentModel)) {
        return false;
    }
    
    return true;
}

bool HistoryAPI::recordRefinement(const std::string& nodeId,
                                  const std::string& oldCode,
                                  const std::string& newCode,
                                  const std::string& reason) {
    // Get current version count
    int versionCount = manager_->getVersionCount(nodeId);
    
    // Create version object
    CodeVersion version;
    version.versionNumber = versionCount + 1;
    version.code = oldCode;
    version.timestamp = std::chrono::system_clock::now();
    version.changeReason = reason;
    
    // Save version to file
    if (!versionStorage_->saveVersion(nodeId, version)) {
        return false;
    }
    
    // Record in database
    return manager_->recordRefinement(nodeId, oldCode, newCode, reason);
}

bool HistoryAPI::updateBlueprintHistory(const std::string& nodeId,
                                        const std::string& originalBlueprint,
                                        const std::string& currentBlueprint) {
    return manager_->updateBlueprintHistory(nodeId, originalBlueprint, currentBlueprint);
}

int HistoryAPI::getTotalHistoryCount() {
    return manager_->getTotalHistoryCount();
}

int HistoryAPI::getVersionCount(const std::string& functionName) {
    auto entry = manager_->getHistoryByFunction(functionName);
    if (!entry) {
        return 0;
    }
    return manager_->getVersionCount(entry->nodeId);
}

int HistoryAPI::getVersionCountByNodeId(const std::string& nodeId) {
    return manager_->getVersionCount(nodeId);
}

bool HistoryAPI::cleanOldHistory(int daysOld) {
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24 * daysOld);
    return manager_->cleanOldHistory(cutoff);
}

bool HistoryAPI::deleteHistory(const std::string& functionName) {
    auto entry = manager_->getHistoryByFunction(functionName);
    if (!entry) {
        return false;
    }
    
    // Delete conversation file
    conversationStorage_->deleteConversation(entry->nodeId);
    
    // Delete version files
    versionStorage_->deleteAllVersions(entry->nodeId);
    
    // Delete from database
    return manager_->deleteHistory(entry->nodeId);
}

bool HistoryAPI::deleteHistoryByNodeId(const std::string& nodeId) {
    // Delete conversation file
    conversationStorage_->deleteConversation(nodeId);
    
    // Delete version files
    versionStorage_->deleteAllVersions(nodeId);
    
    // Delete from database
    return manager_->deleteHistory(nodeId);
}

bool HistoryAPI::exportHistory(const std::string& outputPath, bool includeConversations) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Meld Shadow History Export\n\n";
    
    // Get all history entries
    auto entries = manager_->getHistoryByFile("");  // Empty string gets all
    
    for (const auto& entry : entries) {
        file << "## Function: " << entry.functionName << "\n";
        file << "Node ID: " << entry.nodeId << "\n";
        file << "File: " << entry.filePath << "\n";
        file << "Agent Model: " << entry.agentModel << "\n";
        file << "Versions: " << entry.getVersionCount() << "\n\n";
        
        if (includeConversations && !entry.conversation.empty()) {
            file << "### Conversation\n\n";
            for (const auto& msg : entry.conversation) {
                file << "**" << msg.role << "**: " << msg.content << "\n\n";
            }
        }
        
        if (!entry.versions.empty()) {
            file << "### Version History\n\n";
            for (const auto& version : entry.versions) {
                file << "#### Version " << version.versionNumber << "\n";
                file << "Reason: " << version.changeReason << "\n";
                file << "```meld\n" << version.code << "\n```\n\n";
            }
        }
        
        file << "---\n\n";
    }
    
    file.close();
    return true;
}

} // namespace history
} // namespace meld
