#include "meld/history/history_manager.hpp"
#include <fstream>
#include <sstream>

namespace meld {
namespace history {

HistoryManager::HistoryManager(const std::filesystem::path& historyDir)
    : historyDir_(historyDir),
      conversationsDir_(historyDir / "conversations"),
      versionsDir_(historyDir / "versions") {
    
    database_ = std::make_unique<HistoryDatabase>(historyDir_ / "index.db");
}

bool HistoryManager::initialize() {
    if (!ensureDirectoriesExist()) {
        return false;
    }
    
    return database_->initialize();
}

bool HistoryManager::ensureDirectoriesExist() {
    try {
        std::filesystem::create_directories(historyDir_);
        std::filesystem::create_directories(conversationsDir_);
        std::filesystem::create_directories(versionsDir_);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool HistoryManager::recordGeneration(const std::string& nodeId,
                                      const std::string& functionName,
                                      const std::string& filePath,
                                      const std::vector<Message>& conversation,
                                      const std::string& agentModel) {
    HistoryEntry entry;
    entry.nodeId = nodeId;
    entry.functionName = functionName;
    entry.filePath = filePath;
    entry.createdAt = std::chrono::system_clock::now();
    entry.updatedAt = entry.createdAt;
    entry.agentModel = agentModel;
    entry.conversation = conversation;
    entry.conversationPath = generateConversationPath(nodeId);
    
    return database_->insertHistory(entry);
}

bool HistoryManager::recordRefinement(const std::string& nodeId,
                                      const std::string& oldCode,
                                      const std::string& newCode,
                                      const std::string& reason) {
    auto entry = database_->queryByNodeId(nodeId);
    if (!entry) {
        return false;
    }
    
    // Create new version
    CodeVersion version;
    version.versionNumber = entry->getVersionCount() + 1;
    version.code = oldCode;
    version.timestamp = std::chrono::system_clock::now();
    version.changeReason = reason;
    
    // Add version to entry
    entry->versions.push_back(version);
    entry->updatedAt = version.timestamp;
    
    // Update database
    if (!database_->insertVersion(nodeId, version)) {
        return false;
    }
    
    return database_->updateHistory(*entry);
}

bool HistoryManager::recordVersion(const std::string& nodeId,
                                   const CodeVersion& version) {
    return database_->insertVersion(nodeId, version);
}

bool HistoryManager::linkToNode(const std::string& nodeId,
                                const std::string& functionName,
                                const std::string& filePath) {
    auto entry = database_->queryByNodeId(nodeId);
    if (entry) {
        // Update existing entry
        entry->functionName = functionName;
        entry->filePath = filePath;
        entry->updatedAt = std::chrono::system_clock::now();
        return database_->updateHistory(*entry);
    } else {
        // Create new entry
        HistoryEntry newEntry;
        newEntry.nodeId = nodeId;
        newEntry.functionName = functionName;
        newEntry.filePath = filePath;
        newEntry.createdAt = std::chrono::system_clock::now();
        newEntry.updatedAt = newEntry.createdAt;
        newEntry.conversationPath = generateConversationPath(nodeId);
        return database_->insertHistory(newEntry);
    }
}

bool HistoryManager::updateBlueprintHistory(const std::string& nodeId,
                                            const std::string& originalBlueprint,
                                            const std::string& currentBlueprint) {
    return database_->updateBlueprintHistory(nodeId, originalBlueprint, currentBlueprint);
}

std::optional<HistoryEntry> HistoryManager::getHistory(const std::string& nodeId) {
    return database_->queryByNodeId(nodeId);
}

std::optional<HistoryEntry> HistoryManager::getHistoryByFunction(const std::string& functionName) {
    return database_->queryByFunctionName(functionName);
}

std::vector<HistoryEntry> HistoryManager::getHistoryByFile(const std::string& filePath) {
    return database_->queryByFilePath(filePath);
}

std::vector<Message> HistoryManager::getConversation(const std::string& nodeId) {
    auto entry = database_->queryByNodeId(nodeId);
    if (entry) {
        return entry->conversation;
    }
    return {};
}

std::vector<CodeVersion> HistoryManager::getVersions(const std::string& nodeId) {
    auto entry = database_->queryByNodeId(nodeId);
    if (entry) {
        return entry->versions;
    }
    return {};
}

std::optional<BlueprintHistory> HistoryManager::getBlueprintHistory(const std::string& nodeId) {
    auto entry = database_->queryByNodeId(nodeId);
    if (!entry || (!entry->originalBlueprint && !entry->currentBlueprint)) {
        return std::nullopt;
    }
    
    BlueprintHistory history;
    history.nodeId = nodeId;
    history.originalBlueprint = entry->originalBlueprint.value_or("");
    history.currentBlueprint = entry->currentBlueprint.value_or("");
    history.lastModified = entry->updatedAt;
    
    return history;
}

int HistoryManager::getTotalHistoryCount() {
    return database_->getTotalHistoryCount();
}

int HistoryManager::getVersionCount(const std::string& nodeId) {
    return database_->getVersionCount(nodeId);
}

bool HistoryManager::cleanOldHistory(const Timestamp& olderThan) {
    return database_->deleteOldHistory(olderThan);
}

bool HistoryManager::deleteHistory(const std::string& nodeId) {
    return database_->deleteHistory(nodeId);
}

std::string HistoryManager::generateConversationPath(const std::string& nodeId) {
    return (conversationsDir_ / (nodeId + ".json")).string();
}

std::string HistoryManager::generateVersionPath(const std::string& nodeId, int versionNumber) {
    std::ostringstream oss;
    oss << nodeId << "-v" << versionNumber << ".meld";
    return (versionsDir_ / oss.str()).string();
}

} // namespace history
} // namespace meld
