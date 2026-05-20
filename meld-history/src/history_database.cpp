#include "meld/history/history_database.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace meld {
namespace history {

HistoryDatabase::HistoryDatabase(const std::filesystem::path& dbPath)
    : db_(nullptr), dbPath_(dbPath) {
    
    // Create directory if it doesn't exist
    std::filesystem::create_directories(dbPath_.parent_path());
    
    // Open database
    int rc = sqlite3_open(dbPath_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to open history database: " + 
                                 std::string(sqlite3_errmsg(db_)));
    }
}

HistoryDatabase::~HistoryDatabase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

HistoryDatabase::HistoryDatabase(HistoryDatabase&& other) noexcept
    : db_(other.db_), dbPath_(std::move(other.dbPath_)) {
    other.db_ = nullptr;
}

HistoryDatabase& HistoryDatabase::operator=(HistoryDatabase&& other) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }
        db_ = other.db_;
        dbPath_ = std::move(other.dbPath_);
        other.db_ = nullptr;
    }
    return *this;
}

bool HistoryDatabase::initialize() {
    // Create tables
    if (!executeSQL(getHistoryTableSchema())) return false;
    if (!executeSQL(getVersionsTableSchema())) return false;
    if (!executeSQL(getConversationsTableSchema())) return false;
    if (!executeSQL(getBlueprintsTableSchema())) return false;
    if (!executeSQL(getIndicesSchema())) return false;
    
    return true;
}

std::string HistoryDatabase::getHistoryTableSchema() {
    return R"(
        CREATE TABLE IF NOT EXISTS history (
            node_id TEXT PRIMARY KEY,
            function_name TEXT,
            file_path TEXT,
            created_at TEXT,
            updated_at TEXT,
            agent_model TEXT,
            conversation_path TEXT,
            version_count INTEGER DEFAULT 0
        )
    )";
}

std::string HistoryDatabase::getVersionsTableSchema() {
    return R"(
        CREATE TABLE IF NOT EXISTS versions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id TEXT NOT NULL,
            version_number INTEGER NOT NULL,
            code TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            change_reason TEXT,
            FOREIGN KEY (node_id) REFERENCES history(node_id) ON DELETE CASCADE
        )
    )";
}

std::string HistoryDatabase::getConversationsTableSchema() {
    return R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id TEXT NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (node_id) REFERENCES history(node_id) ON DELETE CASCADE
        )
    )";
}

std::string HistoryDatabase::getBlueprintsTableSchema() {
    return R"(
        CREATE TABLE IF NOT EXISTS blueprints (
            node_id TEXT PRIMARY KEY,
            original_blueprint TEXT,
            current_blueprint TEXT,
            last_modified TEXT,
            FOREIGN KEY (node_id) REFERENCES history(node_id) ON DELETE CASCADE
        )
    )";
}

std::string HistoryDatabase::getIndicesSchema() {
    return R"(
        CREATE INDEX IF NOT EXISTS idx_node_id ON history(node_id);
        CREATE INDEX IF NOT EXISTS idx_function_name ON history(function_name);
        CREATE INDEX IF NOT EXISTS idx_file_path ON history(file_path);
        CREATE INDEX IF NOT EXISTS idx_version_node_id ON versions(node_id);
        CREATE INDEX IF NOT EXISTS idx_conversation_node_id ON conversations(node_id);
    )";
}

bool HistoryDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

std::string HistoryDatabase::timestampToString(const Timestamp& ts) {
    auto time = std::chrono::system_clock::to_time_t(ts);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

Timestamp HistoryDatabase::stringToTimestamp(const std::string& str) {
    std::tm tm = {};
    std::stringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

bool HistoryDatabase::insertHistory(const HistoryEntry& entry) {
    std::string sql = R"(
        INSERT OR REPLACE INTO history 
        (node_id, function_name, file_path, created_at, updated_at, agent_model, conversation_path, version_count)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, entry.nodeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.functionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, entry.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, timestampToString(entry.createdAt).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, timestampToString(entry.updatedAt).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, entry.agentModel.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, entry.conversationPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, entry.getVersionCount());
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // Insert conversation messages
    for (const auto& msg : entry.conversation) {
        if (!insertMessage(entry.nodeId, msg)) return false;
    }
    
    // Insert versions
    for (const auto& version : entry.versions) {
        if (!insertVersion(entry.nodeId, version)) return false;
    }
    
    // Insert blueprint history if present
    if (entry.originalBlueprint || entry.currentBlueprint) {
        updateBlueprintHistory(entry.nodeId,
                               entry.originalBlueprint.value_or(""),
                               entry.currentBlueprint.value_or(""));
    }
    
    return rc == SQLITE_DONE;
}

bool HistoryDatabase::insertVersion(const std::string& nodeId, const CodeVersion& version) {
    std::string sql = R"(
        INSERT INTO versions (node_id, version_number, code, timestamp, change_reason)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, version.versionNumber);
    sqlite3_bind_text(stmt, 3, version.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, timestampToString(version.timestamp).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, version.changeReason.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool HistoryDatabase::insertMessage(const std::string& nodeId, const Message& message) {
    std::string sql = R"(
        INSERT INTO conversations (node_id, role, content, timestamp)
        VALUES (?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, message.role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, timestampToString(message.timestamp).c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::optional<HistoryEntry> HistoryDatabase::queryByNodeId(const std::string& nodeId) {
    std::string sql = "SELECT * FROM history WHERE node_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;
    
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    
    HistoryEntry entry;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entry.nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.functionName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.createdAt = stringToTimestamp(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        entry.updatedAt = stringToTimestamp(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        entry.agentModel = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        entry.conversationPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    } else {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    
    sqlite3_finalize(stmt);
    
    // Load conversation messages
    sql = "SELECT role, content, timestamp FROM conversations WHERE node_id = ? ORDER BY timestamp";
    rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Message msg;
            msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.timestamp = stringToTimestamp(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            entry.conversation.push_back(msg);
        }
        sqlite3_finalize(stmt);
    }
    
    // Load versions
    sql = "SELECT version_number, code, timestamp, change_reason FROM versions WHERE node_id = ? ORDER BY version_number";
    rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CodeVersion version;
            version.versionNumber = sqlite3_column_int(stmt, 0);
            version.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            version.timestamp = stringToTimestamp(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            version.changeReason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            entry.versions.push_back(version);
        }
        sqlite3_finalize(stmt);
    }
    
    // Load blueprint history
    sql = "SELECT original_blueprint, current_blueprint FROM blueprints WHERE node_id = ?";
    rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* orig = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* curr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (orig) entry.originalBlueprint = orig;
            if (curr) entry.currentBlueprint = curr;
        }
        sqlite3_finalize(stmt);
    }
    
    return entry;
}

std::optional<HistoryEntry> HistoryDatabase::queryByFunctionName(const std::string& functionName) {
    std::string sql = "SELECT node_id FROM history WHERE function_name = ? LIMIT 1";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;
    
    sqlite3_bind_text(stmt, 1, functionName.c_str(), -1, SQLITE_TRANSIENT);
    
    std::string nodeId;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    
    if (nodeId.empty()) return std::nullopt;
    return queryByNodeId(nodeId);
}

std::vector<HistoryEntry> HistoryDatabase::queryByFilePath(const std::string& filePath) {
    std::vector<HistoryEntry> results;
    std::string sql = "SELECT node_id FROM history WHERE file_path = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return results;
    
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        auto entry = queryByNodeId(nodeId);
        if (entry) results.push_back(*entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<HistoryEntry> HistoryDatabase::queryAll() {
    std::vector<HistoryEntry> results;
    std::string sql = "SELECT node_id FROM history";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return results;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        auto entry = queryByNodeId(nodeId);
        if (entry) results.push_back(*entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

bool HistoryDatabase::updateHistory(const HistoryEntry& entry) {
    return insertHistory(entry);  // INSERT OR REPLACE handles updates
}

bool HistoryDatabase::updateBlueprintHistory(const std::string& nodeId,
                                             const std::string& originalBlueprint,
                                             const std::string& currentBlueprint) {
    std::string sql = R"(
        INSERT OR REPLACE INTO blueprints (node_id, original_blueprint, current_blueprint, last_modified)
        VALUES (?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    auto now = std::chrono::system_clock::now();
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, originalBlueprint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, currentBlueprint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, timestampToString(now).c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool HistoryDatabase::deleteHistory(const std::string& nodeId) {
    std::string sql = "DELETE FROM history WHERE node_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool HistoryDatabase::deleteOldHistory(const Timestamp& olderThan) {
    std::string sql = "DELETE FROM history WHERE updated_at < ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, timestampToString(olderThan).c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

int HistoryDatabase::getTotalHistoryCount() {
    std::string sql = "SELECT COUNT(*) FROM history";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int HistoryDatabase::getVersionCount(const std::string& nodeId) {
    std::string sql = "SELECT COUNT(*) FROM versions WHERE node_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

} // namespace history
} // namespace meld
