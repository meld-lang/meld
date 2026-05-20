#pragma once

#include "shadow_provenance.hpp"
#include "history_api.hpp"
#include <filesystem>
#include <chrono>

namespace meld {
namespace history {

/**
 * Cleanup options
 */
struct CleanupOptions {
    int olderThanDays = 90;          // Clean history older than this many days
    bool cleanConversations = true;   // Clean conversation files
    bool cleanVersions = true;        // Clean version files
    bool cleanDatabase = true;        // Clean database entries
    bool dryRun = false;              // Don't actually delete, just report
    
    CleanupOptions() = default;
};

/**
 * Cleanup statistics
 */
struct CleanupStats {
    int entriesDeleted = 0;
    int conversationsDeleted = 0;
    int versionsDeleted = 0;
    int bytesFreed = 0;
    
    CleanupStats() = default;
};

/**
 * Handles cleanup of old shadow history
 */
class HistoryCleanup {
public:
    explicit HistoryCleanup(HistoryAPI& historyAPI);
    
    // Clean old history based on options
    CleanupStats cleanOldHistory(const CleanupOptions& options = CleanupOptions());
    
    // Clean history for specific function
    bool cleanFunction(const std::string& functionName);
    
    // Clean history for specific file
    CleanupStats cleanFile(const std::string& filePath);
    
    // Clean orphaned files (files without database entries)
    CleanupStats cleanOrphanedFiles();
    
    // Get cleanup preview (dry run)
    CleanupStats previewCleanup(const CleanupOptions& options);
    
    // Vacuum database (reclaim space)
    bool vacuumDatabase();
    
private:
    HistoryAPI& historyAPI_;
    
    // Helper methods
    Timestamp calculateCutoffDate(int daysOld);
    int getFileSize(const std::filesystem::path& path);
    bool deleteConversationFile(const std::string& nodeId);
    bool deleteVersionFiles(const std::string& nodeId);
};

} // namespace history
} // namespace meld
