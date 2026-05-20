#include "meld/history/history_cleanup.hpp"
#include <fstream>
#include <set>

namespace meld {
namespace history {

HistoryCleanup::HistoryCleanup(HistoryAPI& historyAPI)
    : historyAPI_(historyAPI) {}

Timestamp HistoryCleanup::calculateCutoffDate(int daysOld) {
    auto now = std::chrono::system_clock::now();
    return now - std::chrono::hours(24 * daysOld);
}

int HistoryCleanup::getFileSize(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return 0;
    }
    return static_cast<int>(std::filesystem::file_size(path));
}

bool HistoryCleanup::deleteConversationFile(const std::string& nodeId) {
    auto conversationsDir = historyAPI_.getManager().getConversationsDir();
    auto path = conversationsDir / (nodeId + ".json");
    
    if (std::filesystem::exists(path)) {
        return std::filesystem::remove(path);
    }
    
    return true;
}

bool HistoryCleanup::deleteVersionFiles(const std::string& nodeId) {
    auto versionsDir = historyAPI_.getManager().getVersionsDir();
    bool success = true;
    
    // Find and delete all version files for this node
    for (const auto& entry : std::filesystem::directory_iterator(versionsDir)) {
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

CleanupStats HistoryCleanup::cleanOldHistory(const CleanupOptions& options) {
    CleanupStats stats;
    
    if (options.dryRun) {
        return previewCleanup(options);
    }
    
    auto cutoffDate = calculateCutoffDate(options.olderThanDays);
    
    // Get all history entries
    auto allEntries = historyAPI_.queryByFile("");  // Empty string gets all
    
    for (const auto& entry : allEntries) {
        if (entry.updatedAt < cutoffDate) {
            // Calculate size before deletion
            if (options.cleanConversations) {
                auto conversationsDir = historyAPI_.getManager().getConversationsDir();
                auto convPath = conversationsDir / (entry.nodeId + ".json");
                stats.bytesFreed += getFileSize(convPath);
            }
            
            if (options.cleanVersions) {
                auto versionsDir = historyAPI_.getManager().getVersionsDir();
                for (const auto& version : entry.versions) {
                    std::ostringstream filename;
                    filename << entry.nodeId << "-v" << version.versionNumber << ".meld";
                    auto versionPath = versionsDir / filename.str();
                    stats.bytesFreed += getFileSize(versionPath);
                }
            }
            
            // Delete conversation file
            if (options.cleanConversations) {
                if (deleteConversationFile(entry.nodeId)) {
                    stats.conversationsDeleted++;
                }
            }
            
            // Delete version files
            if (options.cleanVersions) {
                if (deleteVersionFiles(entry.nodeId)) {
                    stats.versionsDeleted += entry.getVersionCount();
                }
            }
            
            // Delete database entry
            if (options.cleanDatabase) {
                if (historyAPI_.deleteHistoryByNodeId(entry.nodeId)) {
                    stats.entriesDeleted++;
                }
            }
        }
    }
    
    return stats;
}

bool HistoryCleanup::cleanFunction(const std::string& functionName) {
    auto entry = historyAPI_.query(functionName);
    if (!entry) {
        return false;
    }
    
    // Delete conversation file
    deleteConversationFile(entry->nodeId);
    
    // Delete version files
    deleteVersionFiles(entry->nodeId);
    
    // Delete database entry
    return historyAPI_.deleteHistory(functionName);
}

CleanupStats HistoryCleanup::cleanFile(const std::string& filePath) {
    CleanupStats stats;
    
    auto entries = historyAPI_.queryByFile(filePath);
    
    for (const auto& entry : entries) {
        // Calculate size
        auto conversationsDir = historyAPI_.getManager().getConversationsDir();
        auto convPath = conversationsDir / (entry.nodeId + ".json");
        stats.bytesFreed += getFileSize(convPath);
        
        auto versionsDir = historyAPI_.getManager().getVersionsDir();
        for (const auto& version : entry.versions) {
            std::ostringstream filename;
            filename << entry.nodeId << "-v" << version.versionNumber << ".meld";
            auto versionPath = versionsDir / filename.str();
            stats.bytesFreed += getFileSize(versionPath);
        }
        
        // Delete files
        if (deleteConversationFile(entry.nodeId)) {
            stats.conversationsDeleted++;
        }
        
        if (deleteVersionFiles(entry.nodeId)) {
            stats.versionsDeleted += entry.getVersionCount();
        }
        
        // Delete database entry
        if (historyAPI_.deleteHistoryByNodeId(entry.nodeId)) {
            stats.entriesDeleted++;
        }
    }
    
    return stats;
}

CleanupStats HistoryCleanup::cleanOrphanedFiles() {
    CleanupStats stats;
    
    // Get all node IDs from database
    auto allEntries = historyAPI_.queryByFile("");
    std::set<std::string> validNodeIds;
    for (const auto& entry : allEntries) {
        validNodeIds.insert(entry.nodeId);
    }
    
    // Check conversation files
    auto conversationsDir = historyAPI_.getManager().getConversationsDir();
    if (std::filesystem::exists(conversationsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(conversationsDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().stem().string();
                if (validNodeIds.find(filename) == validNodeIds.end()) {
                    stats.bytesFreed += getFileSize(entry.path());
                    if (std::filesystem::remove(entry.path())) {
                        stats.conversationsDeleted++;
                    }
                }
            }
        }
    }
    
    // Check version files
    auto versionsDir = historyAPI_.getManager().getVersionsDir();
    if (std::filesystem::exists(versionsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(versionsDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                size_t dashPos = filename.find("-v");
                if (dashPos != std::string::npos) {
                    std::string nodeId = filename.substr(0, dashPos);
                    if (validNodeIds.find(nodeId) == validNodeIds.end()) {
                        stats.bytesFreed += getFileSize(entry.path());
                        if (std::filesystem::remove(entry.path())) {
                            stats.versionsDeleted++;
                        }
                    }
                }
            }
        }
    }
    
    return stats;
}

CleanupStats HistoryCleanup::previewCleanup(const CleanupOptions& options) {
    CleanupStats stats;
    
    auto cutoffDate = calculateCutoffDate(options.olderThanDays);
    
    // Get all history entries
    auto allEntries = historyAPI_.queryByFile("");
    
    for (const auto& entry : allEntries) {
        if (entry.updatedAt < cutoffDate) {
            stats.entriesDeleted++;
            
            if (options.cleanConversations) {
                auto conversationsDir = historyAPI_.getManager().getConversationsDir();
                auto convPath = conversationsDir / (entry.nodeId + ".json");
                stats.bytesFreed += getFileSize(convPath);
                stats.conversationsDeleted++;
            }
            
            if (options.cleanVersions) {
                auto versionsDir = historyAPI_.getManager().getVersionsDir();
                for (const auto& version : entry.versions) {
                    std::ostringstream filename;
                    filename << entry.nodeId << "-v" << version.versionNumber << ".meld";
                    auto versionPath = versionsDir / filename.str();
                    stats.bytesFreed += getFileSize(versionPath);
                    stats.versionsDeleted++;
                }
            }
        }
    }
    
    return stats;
}

bool HistoryCleanup::vacuumDatabase() {
    // Note: This would require access to the underlying SQLite database
    // For now, we'll return true as a placeholder
    // In a real implementation, we would execute "VACUUM" on the database
    return true;
}

} // namespace history
} // namespace meld
