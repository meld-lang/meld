#pragma once

#include "shadow_provenance.hpp"
#include <sqlite3.h>
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <filesystem>

namespace meld {
namespace history {

/**
 * SQLite database for indexing and querying shadow history
 */
class HistoryDatabase {
public:
    explicit HistoryDatabase(const std::filesystem::path& dbPath);
    ~HistoryDatabase();
    
    // Prevent copying
    HistoryDatabase(const HistoryDatabase&) = delete;
    HistoryDatabase& operator=(const HistoryDatabase&) = delete;
    
    // Allow moving
    HistoryDatabase(HistoryDatabase&&) noexcept;
    HistoryDatabase& operator=(HistoryDatabase&&) noexcept;
    
    // Initialize database schema
    bool initialize();
    
    // Insert operations
    bool insertHistory(const HistoryEntry& entry);
    bool insertVersion(const std::string& nodeId, const CodeVersion& version);
    bool insertMessage(const std::string& nodeId, const Message& message);
    
    // Query operations
    std::optional<HistoryEntry> queryByNodeId(const std::string& nodeId);
    std::optional<HistoryEntry> queryByFunctionName(const std::string& functionName);
    std::vector<HistoryEntry> queryByFilePath(const std::string& filePath);
    std::vector<HistoryEntry> queryAll();
    
    // Update operations
    bool updateHistory(const HistoryEntry& entry);
    bool updateBlueprintHistory(const std::string& nodeId, 
                                const std::string& originalBlueprint,
                                const std::string& currentBlueprint);
    
    // Delete operations
    bool deleteHistory(const std::string& nodeId);
    bool deleteOldHistory(const Timestamp& olderThan);
    
    // Statistics
    int getTotalHistoryCount();
    int getVersionCount(const std::string& nodeId);
    
private:
    sqlite3* db_;
    std::filesystem::path dbPath_;
    
    // Helper methods
    bool executeSQL(const std::string& sql);
    std::string timestampToString(const Timestamp& ts);
    Timestamp stringToTimestamp(const std::string& str);
    
    // Schema creation
    std::string getHistoryTableSchema();
    std::string getVersionsTableSchema();
    std::string getConversationsTableSchema();
    std::string getBlueprintsTableSchema();
    std::string getIndicesSchema();
};

} // namespace history
} // namespace meld
