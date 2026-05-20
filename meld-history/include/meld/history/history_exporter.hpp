#pragma once

#include "shadow_provenance.hpp"
#include "history_api.hpp"
#include <filesystem>
#include <string>

namespace meld {
namespace history {

/**
 * Export formats for history
 */
enum class ExportFormat {
    Markdown,
    JSON,
    HTML,
    Archive  // Portable archive with all files
};

/**
 * Export options
 */
struct ExportOptions {
    bool includeConversations = true;
    bool includeVersions = true;
    bool includeBlueprints = true;
    bool includeMetadata = true;
    ExportFormat format = ExportFormat::Markdown;
    
    ExportOptions() = default;
};

/**
 * Handles exporting shadow history in various formats
 */
class HistoryExporter {
public:
    explicit HistoryExporter(HistoryAPI& historyAPI);
    
    // Export all history
    bool exportAll(const std::filesystem::path& outputPath,
                   const ExportOptions& options = ExportOptions());
    
    // Export history for specific function
    bool exportFunction(const std::string& functionName,
                        const std::filesystem::path& outputPath,
                        const ExportOptions& options = ExportOptions());
    
    // Export history for specific file
    bool exportFile(const std::string& filePath,
                    const std::filesystem::path& outputPath,
                    const ExportOptions& options = ExportOptions());
    
    // Create portable archive
    bool createArchive(const std::filesystem::path& archivePath,
                       const ExportOptions& options = ExportOptions());
    
private:
    HistoryAPI& historyAPI_;
    
    // Format-specific exporters
    bool exportMarkdown(const std::vector<HistoryEntry>& entries,
                        const std::filesystem::path& outputPath,
                        const ExportOptions& options);
    
    bool exportJSON(const std::vector<HistoryEntry>& entries,
                    const std::filesystem::path& outputPath,
                    const ExportOptions& options);
    
    bool exportHTML(const std::vector<HistoryEntry>& entries,
                    const std::filesystem::path& outputPath,
                    const ExportOptions& options);
    
    // Helper methods
    std::string formatTimestamp(const Timestamp& ts);
    std::string escapeHTML(const std::string& text);
    std::string escapeJSON(const std::string& text);
};

} // namespace history
} // namespace meld
