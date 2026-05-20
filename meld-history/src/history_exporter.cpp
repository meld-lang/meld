#include "meld/history/history_exporter.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace meld {
namespace history {

HistoryExporter::HistoryExporter(HistoryAPI& historyAPI)
    : historyAPI_(historyAPI) {}

std::string HistoryExporter::formatTimestamp(const Timestamp& ts) {
    auto time = std::chrono::system_clock::to_time_t(ts);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

bool HistoryExporter::exportAll(const std::filesystem::path& outputPath,
                                const ExportOptions& options) {
    // Get all history entries
    auto entries = historyAPI_.queryByFile("");  // Empty string gets all
    
    switch (options.format) {
        case ExportFormat::Markdown:
            return exportMarkdown(entries, outputPath, options);
        case ExportFormat::JSON:
            return exportJSON(entries, outputPath, options);
        case ExportFormat::HTML:
            return exportHTML(entries, outputPath, options);
        case ExportFormat::Archive:
            return createArchive(outputPath, options);
        default:
            return false;
    }
}

bool HistoryExporter::exportFunction(const std::string& functionName,
                                     const std::filesystem::path& outputPath,
                                     const ExportOptions& options) {
    auto entry = historyAPI_.query(functionName);
    if (!entry) {
        return false;
    }
    
    std::vector<HistoryEntry> entries = { *entry };
    
    switch (options.format) {
        case ExportFormat::Markdown:
            return exportMarkdown(entries, outputPath, options);
        case ExportFormat::JSON:
            return exportJSON(entries, outputPath, options);
        case ExportFormat::HTML:
            return exportHTML(entries, outputPath, options);
        default:
            return false;
    }
}

bool HistoryExporter::exportFile(const std::string& filePath,
                                 const std::filesystem::path& outputPath,
                                 const ExportOptions& options) {
    auto entries = historyAPI_.queryByFile(filePath);
    
    if (entries.empty()) {
        return false;
    }
    
    switch (options.format) {
        case ExportFormat::Markdown:
            return exportMarkdown(entries, outputPath, options);
        case ExportFormat::JSON:
            return exportJSON(entries, outputPath, options);
        case ExportFormat::HTML:
            return exportHTML(entries, outputPath, options);
        default:
            return false;
    }
}

bool HistoryExporter::exportMarkdown(const std::vector<HistoryEntry>& entries,
                                     const std::filesystem::path& outputPath,
                                     const ExportOptions& options) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Meld Shadow History Export\n\n";
    file << "Generated: " << formatTimestamp(std::chrono::system_clock::now()) << "\n\n";
    file << "---\n\n";
    
    for (const auto& entry : entries) {
        file << "## " << entry.functionName << "\n\n";
        
        if (options.includeMetadata) {
            file << "- **Node ID**: `" << entry.nodeId << "`\n";
            file << "- **File**: `" << entry.filePath << "`\n";
            file << "- **Agent Model**: " << entry.agentModel << "\n";
            file << "- **Created**: " << formatTimestamp(entry.createdAt) << "\n";
            file << "- **Last Updated**: " << formatTimestamp(entry.updatedAt) << "\n";
            file << "- **Versions**: " << entry.getVersionCount() << "\n\n";
        }
        
        if (options.includeConversations && !entry.conversation.empty()) {
            file << "### Conversation\n\n";
            for (const auto& msg : entry.conversation) {
                file << "**" << msg.role << "** (" << formatTimestamp(msg.timestamp) << "):\n\n";
                file << msg.content << "\n\n";
            }
        }
        
        if (options.includeVersions && !entry.versions.empty()) {
            file << "### Version History\n\n";
            for (const auto& version : entry.versions) {
                file << "#### Version " << version.versionNumber << "\n\n";
                file << "- **Timestamp**: " << formatTimestamp(version.timestamp) << "\n";
                file << "- **Reason**: " << version.changeReason << "\n\n";
                file << "```meld\n" << version.code << "\n```\n\n";
            }
        }
        
        if (options.includeBlueprints && (entry.originalBlueprint || entry.currentBlueprint)) {
            file << "### Blueprint Evolution\n\n";
            if (entry.originalBlueprint) {
                file << "**Original Blueprint**:\n\n";
                file << *entry.originalBlueprint << "\n\n";
            }
            if (entry.currentBlueprint) {
                file << "**Current Blueprint**:\n\n";
                file << *entry.currentBlueprint << "\n\n";
            }
        }
        
        file << "---\n\n";
    }
    
    file.close();
    return true;
}

std::string HistoryExporter::escapeJSON(const std::string& text) {
    std::ostringstream escaped;
    for (char c : text) {
        switch (c) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

bool HistoryExporter::exportJSON(const std::vector<HistoryEntry>& entries,
                                 const std::filesystem::path& outputPath,
                                 const ExportOptions& options) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"exportDate\": \"" << formatTimestamp(std::chrono::system_clock::now()) << "\",\n";
    file << "  \"entries\": [\n";
    
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        
        file << "    {\n";
        file << "      \"nodeId\": \"" << entry.nodeId << "\",\n";
        file << "      \"functionName\": \"" << entry.functionName << "\",\n";
        file << "      \"filePath\": \"" << escapeJSON(entry.filePath) << "\",\n";
        file << "      \"agentModel\": \"" << entry.agentModel << "\",\n";
        file << "      \"createdAt\": \"" << formatTimestamp(entry.createdAt) << "\",\n";
        file << "      \"updatedAt\": \"" << formatTimestamp(entry.updatedAt) << "\",\n";
        file << "      \"versionCount\": " << entry.getVersionCount() << ",\n";
        
        if (options.includeConversations) {
            file << "      \"conversation\": [\n";
            for (size_t j = 0; j < entry.conversation.size(); ++j) {
                const auto& msg = entry.conversation[j];
                file << "        {\n";
                file << "          \"role\": \"" << msg.role << "\",\n";
                file << "          \"timestamp\": \"" << formatTimestamp(msg.timestamp) << "\",\n";
                file << "          \"content\": \"" << escapeJSON(msg.content) << "\"\n";
                file << "        }";
                if (j < entry.conversation.size() - 1) file << ",";
                file << "\n";
            }
            file << "      ],\n";
        }
        
        if (options.includeVersions) {
            file << "      \"versions\": [\n";
            for (size_t j = 0; j < entry.versions.size(); ++j) {
                const auto& version = entry.versions[j];
                file << "        {\n";
                file << "          \"versionNumber\": " << version.versionNumber << ",\n";
                file << "          \"timestamp\": \"" << formatTimestamp(version.timestamp) << "\",\n";
                file << "          \"changeReason\": \"" << escapeJSON(version.changeReason) << "\",\n";
                file << "          \"code\": \"" << escapeJSON(version.code) << "\"\n";
                file << "        }";
                if (j < entry.versions.size() - 1) file << ",";
                file << "\n";
            }
            file << "      ],\n";
        }
        
        if (options.includeBlueprints) {
            file << "      \"blueprints\": {\n";
            file << "        \"original\": ";
            if (entry.originalBlueprint) {
                file << "\"" << escapeJSON(*entry.originalBlueprint) << "\"";
            } else {
                file << "null";
            }
            file << ",\n";
            file << "        \"current\": ";
            if (entry.currentBlueprint) {
                file << "\"" << escapeJSON(*entry.currentBlueprint) << "\"";
            } else {
                file << "null";
            }
            file << "\n";
            file << "      }\n";
        } else {
            // Remove trailing comma from last field
            file.seekp(-2, std::ios_base::cur);
            file << "\n";
        }
        
        file << "    }";
        if (i < entries.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    return true;
}

std::string HistoryExporter::escapeHTML(const std::string& text) {
    std::ostringstream escaped;
    for (char c : text) {
        switch (c) {
            case '<': escaped << "&lt;"; break;
            case '>': escaped << "&gt;"; break;
            case '&': escaped << "&amp;"; break;
            case '"': escaped << "&quot;"; break;
            case '\'': escaped << "&#39;"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

bool HistoryExporter::exportHTML(const std::vector<HistoryEntry>& entries,
                                 const std::filesystem::path& outputPath,
                                 const ExportOptions& options) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "<!DOCTYPE html>\n";
    file << "<html>\n<head>\n";
    file << "  <meta charset=\"UTF-8\">\n";
    file << "  <title>Meld Shadow History Export</title>\n";
    file << "  <style>\n";
    file << "    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }\n";
    file << "    .entry { background: white; padding: 20px; margin: 20px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n";
    file << "    .header { border-bottom: 2px solid #007acc; padding-bottom: 10px; margin-bottom: 20px; }\n";
    file << "    .metadata { color: #666; font-size: 0.9em; }\n";
    file << "    .conversation { background: #f9f9f9; padding: 15px; margin: 10px 0; border-left: 4px solid #007acc; }\n";
    file << "    .message { margin: 10px 0; }\n";
    file << "    .role { font-weight: bold; color: #007acc; }\n";
    file << "    .version { background: #f0f0f0; padding: 10px; margin: 10px 0; border-radius: 4px; }\n";
    file << "    pre { background: #2d2d2d; color: #f8f8f2; padding: 15px; border-radius: 4px; overflow-x: auto; }\n";
    file << "  </style>\n";
    file << "</head>\n<body>\n";
    
    file << "<h1>Meld Shadow History Export</h1>\n";
    file << "<p class=\"metadata\">Generated: " << escapeHTML(formatTimestamp(std::chrono::system_clock::now())) << "</p>\n";
    
    for (const auto& entry : entries) {
        file << "<div class=\"entry\">\n";
        file << "  <div class=\"header\">\n";
        file << "    <h2>" << escapeHTML(entry.functionName) << "</h2>\n";
        file << "  </div>\n";
        
        if (options.includeMetadata) {
            file << "  <div class=\"metadata\">\n";
            file << "    <p><strong>Node ID:</strong> " << escapeHTML(entry.nodeId) << "</p>\n";
            file << "    <p><strong>File:</strong> " << escapeHTML(entry.filePath) << "</p>\n";
            file << "    <p><strong>Agent Model:</strong> " << escapeHTML(entry.agentModel) << "</p>\n";
            file << "    <p><strong>Created:</strong> " << escapeHTML(formatTimestamp(entry.createdAt)) << "</p>\n";
            file << "    <p><strong>Versions:</strong> " << entry.getVersionCount() << "</p>\n";
            file << "  </div>\n";
        }
        
        if (options.includeConversations && !entry.conversation.empty()) {
            file << "  <h3>Conversation</h3>\n";
            file << "  <div class=\"conversation\">\n";
            for (const auto& msg : entry.conversation) {
                file << "    <div class=\"message\">\n";
                file << "      <span class=\"role\">" << escapeHTML(msg.role) << "</span> ";
                file << "      <span class=\"metadata\">(" << escapeHTML(formatTimestamp(msg.timestamp)) << ")</span>\n";
                file << "      <p>" << escapeHTML(msg.content) << "</p>\n";
                file << "    </div>\n";
            }
            file << "  </div>\n";
        }
        
        if (options.includeVersions && !entry.versions.empty()) {
            file << "  <h3>Version History</h3>\n";
            for (const auto& version : entry.versions) {
                file << "  <div class=\"version\">\n";
                file << "    <h4>Version " << version.versionNumber << "</h4>\n";
                file << "    <p class=\"metadata\">" << escapeHTML(formatTimestamp(version.timestamp)) << "</p>\n";
                file << "    <p><strong>Reason:</strong> " << escapeHTML(version.changeReason) << "</p>\n";
                file << "    <pre><code>" << escapeHTML(version.code) << "</code></pre>\n";
                file << "  </div>\n";
            }
        }
        
        file << "</div>\n";
    }
    
    file << "</body>\n</html>\n";
    
    file.close();
    return true;
}

bool HistoryExporter::createArchive(const std::filesystem::path& archivePath,
                                    const ExportOptions& options) {
    // Create a directory for the archive
    std::filesystem::path archiveDir = archivePath;
    archiveDir.replace_extension("");  // Remove extension if present
    
    std::filesystem::create_directories(archiveDir);
    
    // Export main index as markdown
    exportMarkdown(historyAPI_.queryByFile(""), archiveDir / "index.md", options);
    
    // Export as JSON for machine readability
    exportJSON(historyAPI_.queryByFile(""), archiveDir / "history.json", options);
    
    // Copy conversation files
    if (options.includeConversations) {
        std::filesystem::path conversationsDir = archiveDir / "conversations";
        std::filesystem::create_directories(conversationsDir);
        
        // Note: In a real implementation, we would copy the actual conversation files
        // from the history directory. This is a simplified version.
    }
    
    // Copy version files
    if (options.includeVersions) {
        std::filesystem::path versionsDir = archiveDir / "versions";
        std::filesystem::create_directories(versionsDir);
        
        // Note: In a real implementation, we would copy the actual version files
        // from the history directory. This is a simplified version.
    }
    
    return true;
}

} // namespace history
} // namespace meld
