#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>

namespace meld::ai {

// Shadow history entry for tracking AI conversation context
struct ShadowHistoryEntry {
    std::string function_name;
    std::string ast_node_id;
    std::string conversation_id;
    std::string generation_context;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> previous_versions;
    
    nlohmann::json to_json() const;
    static ShadowHistoryEntry from_json(const nlohmann::json& j);
};

// Shadow history manager for storing AI conversation history separately from source code
class ShadowHistory {
public:
    ShadowHistory();
    ~ShadowHistory();
    
    // Store history entry for a function
    bool store_entry(const ShadowHistoryEntry& entry);
    
    // Retrieve history for a function
    std::vector<ShadowHistoryEntry> get_history(const std::string& function_name) const;
    
    // Link function to conversation
    bool link_to_conversation(const std::string& function_name, 
                             const std::string& conversation_id,
                             const std::string& generation_context = "");
    
    // Export history for a source file
    std::string export_history(const std::string& source_file) const;
    
    // Import history from JSON
    bool import_history(const std::string& json_data);
    
    // Set history directory
    void set_history_directory(const std::string& directory);
    
private:
    std::string history_directory_;
    std::vector<ShadowHistoryEntry> entries_;
    
    std::string get_history_file_path(const std::string& source_file) const;
    bool load_history_file(const std::string& file_path);
    bool save_history_file(const std::string& file_path) const;
};

} // namespace meld::ai