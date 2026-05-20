#include "meld/history/conversation_storage.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace meld {
namespace history {

ConversationStorage::ConversationStorage(const std::filesystem::path& conversationsDir)
    : conversationsDir_(conversationsDir) {
    std::filesystem::create_directories(conversationsDir_);
}

std::filesystem::path ConversationStorage::getConversationPath(const std::string& nodeId) {
    return conversationsDir_ / (nodeId + ".json");
}

bool ConversationStorage::saveConversation(const std::string& nodeId,
                                           const std::vector<Message>& conversation,
                                           const std::string& agentModel) {
    auto path = getConversationPath(nodeId);
    std::ofstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    std::string json = serializeConversation(conversation, agentModel);
    file << json;
    file.close();
    
    return true;
}

std::optional<std::vector<Message>> ConversationStorage::loadConversation(const std::string& nodeId) {
    auto path = getConversationPath(nodeId);
    
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return deserializeConversation(buffer.str());
}

bool ConversationStorage::conversationExists(const std::string& nodeId) {
    return std::filesystem::exists(getConversationPath(nodeId));
}

bool ConversationStorage::deleteConversation(const std::string& nodeId) {
    auto path = getConversationPath(nodeId);
    
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    return std::filesystem::remove(path);
}

std::string ConversationStorage::timestampToISO8601(const Timestamp& ts) {
    auto time = std::chrono::system_clock::to_time_t(ts);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

Timestamp ConversationStorage::iso8601ToTimestamp(const std::string& str) {
    std::tm tm = {};
    std::stringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    auto time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

std::string ConversationStorage::serializeConversation(const std::vector<Message>& conversation,
                                                       const std::string& agentModel) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"agentModel\": \"" << agentModel << "\",\n";
    json << "  \"timestamp\": \"" << timestampToISO8601(std::chrono::system_clock::now()) << "\",\n";
    json << "  \"messages\": [\n";
    
    for (size_t i = 0; i < conversation.size(); ++i) {
        const auto& msg = conversation[i];
        json << "    {\n";
        json << "      \"role\": \"" << msg.role << "\",\n";
        json << "      \"timestamp\": \"" << timestampToISO8601(msg.timestamp) << "\",\n";
        json << "      \"content\": \"";
        
        // Escape special characters in content
        for (char c : msg.content) {
            switch (c) {
                case '"': json << "\\\""; break;
                case '\\': json << "\\\\"; break;
                case '\n': json << "\\n"; break;
                case '\r': json << "\\r"; break;
                case '\t': json << "\\t"; break;
                default: json << c; break;
            }
        }
        
        json << "\"\n";
        json << "    }";
        
        if (i < conversation.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    
    json << "  ]\n";
    json << "}\n";
    
    return json.str();
}

std::optional<std::vector<Message>> ConversationStorage::deserializeConversation(const std::string& json) {
    // Simple JSON parsing (in production, use a proper JSON library)
    std::vector<Message> messages;
    
    // Find messages array
    size_t messagesStart = json.find("\"messages\":");
    if (messagesStart == std::string::npos) {
        return std::nullopt;
    }
    
    size_t arrayStart = json.find('[', messagesStart);
    size_t arrayEnd = json.rfind(']');
    
    if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
        return std::nullopt;
    }
    
    // Parse each message object
    size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        // Find next message object
        size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos || objStart >= arrayEnd) break;
        
        size_t objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos || objEnd >= arrayEnd) break;
        
        std::string objStr = json.substr(objStart, objEnd - objStart + 1);
        
        // Extract role
        size_t roleStart = objStr.find("\"role\":");
        size_t roleValueStart = objStr.find('"', roleStart + 7);
        size_t roleValueEnd = objStr.find('"', roleValueStart + 1);
        std::string role = objStr.substr(roleValueStart + 1, roleValueEnd - roleValueStart - 1);
        
        // Extract timestamp
        size_t tsStart = objStr.find("\"timestamp\":");
        size_t tsValueStart = objStr.find('"', tsStart + 12);
        size_t tsValueEnd = objStr.find('"', tsValueStart + 1);
        std::string tsStr = objStr.substr(tsValueStart + 1, tsValueEnd - tsValueStart - 1);
        Timestamp timestamp = iso8601ToTimestamp(tsStr);
        
        // Extract content
        size_t contentStart = objStr.find("\"content\":");
        size_t contentValueStart = objStr.find('"', contentStart + 10);
        size_t contentValueEnd = objStr.rfind('"');
        std::string content = objStr.substr(contentValueStart + 1, contentValueEnd - contentValueStart - 1);
        
        // Unescape content
        std::string unescaped;
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\\' && i + 1 < content.size()) {
                switch (content[i + 1]) {
                    case 'n': unescaped += '\n'; ++i; break;
                    case 'r': unescaped += '\r'; ++i; break;
                    case 't': unescaped += '\t'; ++i; break;
                    case '"': unescaped += '"'; ++i; break;
                    case '\\': unescaped += '\\'; ++i; break;
                    default: unescaped += content[i]; break;
                }
            } else {
                unescaped += content[i];
            }
        }
        
        messages.emplace_back(role, unescaped, timestamp);
        pos = objEnd + 1;
    }
    
    return messages;
}

} // namespace history
} // namespace meld
