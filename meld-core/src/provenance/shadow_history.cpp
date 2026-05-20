#include "meld/provenance/shadow_history.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace meld::ai {

nlohmann::json ShadowHistoryEntry::to_json() const {
    nlohmann::json j;
    j["function_name"] = function_name;
    j["ast_node_id"] = ast_node_id;
    j["conversation_id"] = conversation_id;
    j["generation_context"] = generation_context;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    j["previous_versions"] = previous_versions;
    return j;
}

ShadowHistoryEntry ShadowHistoryEntry::from_json(const nlohmann::json& j) {
    ShadowHistoryEntry entry;
    entry.function_name = j.at("function_name").get<std::string>();
    entry.ast_node_id = j.at("ast_node_id").get<std::string>();
    entry.conversation_id = j.value("conversation_id", std::string{});
    entry.generation_context = j.value("generation_context", std::string{});
    
    auto timestamp_seconds = j.at("timestamp").get<int64_t>();
    entry.timestamp = std::chrono::system_clock::time_point(
        std::chrono::seconds(timestamp_seconds));
    
    if (j.contains("previous_versions")) {
        entry.previous_versions = j.at("previous_versions").get<std::vector<std::string>>();
    }
    
    return entry;
}

ShadowHistory::ShadowHistory() 
    : history_directory_(".meld/history") {
}

ShadowHistory::~ShadowHistory() = default;

bool ShadowHistory::store_entry(const ShadowHistoryEntry& entry) {
    try {
        // Add to in-memory storage
        entries_.push_back(entry);
        
        // Create history directory if it doesn't exist
        std::filesystem::create_directories(history_directory_);
        
        // Save to file (simplified - in practice would be more sophisticated)
        std::string file_path = get_history_file_path("current");
        return save_history_file(file_path);
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<ShadowHistoryEntry> ShadowHistory::get_history(const std::string& function_name) const {
    std::vector<ShadowHistoryEntry> result;
    
    std::copy_if(entries_.begin(), entries_.end(), std::back_inserter(result),
        [&function_name](const ShadowHistoryEntry& entry) {
            return entry.function_name == function_name;
        });
    
    return result;
}

bool ShadowHistory::link_to_conversation(const std::string& function_name, 
                                        const std::string& conversation_id,
                                        const std::string& generation_context) {
    try {
        ShadowHistoryEntry entry;
        entry.function_name = function_name;
        entry.conversation_id = conversation_id;
        entry.generation_context = generation_context;
        entry.timestamp = std::chrono::system_clock::now();
        entry.ast_node_id = "node_" + function_name;
        
        return store_entry(entry);
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ShadowHistory::export_history(const std::string& source_file) const {
    nlohmann::json j;
    j["source_file"] = source_file;
    j["export_timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    nlohmann::json entries_json = nlohmann::json::array();
    for (const auto& entry : entries_) {
        entries_json.push_back(entry.to_json());
    }
    j["entries"] = entries_json;
    
    return j.dump(2);
}

bool ShadowHistory::import_history(const std::string& json_data) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_data);
        
        entries_.clear();
        
        if (j.contains("entries")) {
            for (const auto& entry_json : j.at("entries")) {
                entries_.push_back(ShadowHistoryEntry::from_json(entry_json));
            }
        }
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void ShadowHistory::set_history_directory(const std::string& directory) {
    history_directory_ = directory;
}

std::string ShadowHistory::get_history_file_path(const std::string& source_file) const {
    std::filesystem::path source_path(source_file);
    std::string filename = source_path.stem().string() + "_history.json";
    return (std::filesystem::path(history_directory_) / filename).string();
}

bool ShadowHistory::load_history_file(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file) {
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        return import_history(content);
        
    } catch (const std::exception&) {
        return false;
    }
}

bool ShadowHistory::save_history_file(const std::string& file_path) const {
    try {
        std::ofstream file(file_path);
        if (!file) {
            return false;
        }
        
        file << export_history("current");
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace meld::ai