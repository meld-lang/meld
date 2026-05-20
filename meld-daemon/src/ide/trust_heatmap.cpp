#include "meld/ide/trust_heatmap.hpp"
#include "meld/provenance/provenance.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace meld {
namespace ide {

// Color constants for different trust levels
const TrustHeatmapColor TrustVisualization::HUMAN_VERIFIED_COLOR(0, 255, 0, 80);        // Green tint
const TrustHeatmapColor TrustVisualization::HIGH_CONFIDENCE_AGENT_COLOR(128, 0, 255, 80); // Purple tint
const TrustHeatmapColor TrustVisualization::MEDIUM_CONFIDENCE_AGENT_COLOR(255, 165, 0, 80); // Orange tint
const TrustHeatmapColor TrustVisualization::LOW_CONFIDENCE_AGENT_COLOR(255, 0, 0, 100);  // Red tint
const TrustHeatmapColor TrustVisualization::UNTRUSTED_COLOR(128, 128, 128, 60);          // Gray tint

// TrustHeatmapColor implementation
std::string TrustHeatmapColor::toHex() const {
    std::ostringstream oss;
    oss << "#" << std::hex << std::setfill('0') 
        << std::setw(2) << red 
        << std::setw(2) << green 
        << std::setw(2) << blue;
    if (alpha != 255) {
        oss << std::setw(2) << alpha;
    }
    return oss.str();
}

std::string TrustHeatmapColor::toCssRgba() const {
    std::ostringstream oss;
    oss << "rgba(" << red << ", " << green << ", " << blue << ", " 
        << std::fixed << std::setprecision(2) << (alpha / 255.0) << ")";
    return oss.str();
}

std::string TrustHeatmapColor::toJson() const {
    std::ostringstream oss;
    oss << "{\"red\":" << red << ",\"green\":" << green << ",\"blue\":" << blue 
        << ",\"alpha\":" << alpha << ",\"hex\":\"" << toHex() 
        << "\",\"css\":\"" << toCssRgba() << "\"}";
    return oss.str();
}

// GutterIcon implementation
std::string GutterIcon::toJson() const {
    std::ostringstream oss;
    oss << "{\"symbol\":\"" << TrustVisualization::escapeJson(unicode_symbol) 
        << "\",\"tooltip\":\"" << TrustVisualization::escapeJson(tooltip_text) 
        << "\",\"color\":" << color.toJson();
    if (!svg_path.empty()) {
        oss << ",\"svg\":\"" << TrustVisualization::escapeJson(svg_path) << "\"";
    }
    oss << "}";
    return oss.str();
}

// HoverTooltip implementation
std::string HoverTooltip::toHtml() const {
    std::ostringstream oss;
    oss << "<div class=\"meld-trust-tooltip\">";
    oss << "<h4>" << TrustVisualization::escapeHtml(title) << "</h4>";
    
    if (!origin_info.empty()) {
        oss << "<p><strong>Origin:</strong> " << TrustVisualization::escapeHtml(origin_info) << "</p>";
    }
    
    if (!trust_info.empty()) {
        oss << "<p><strong>Trust:</strong> " << TrustVisualization::escapeHtml(trust_info) << "</p>";
    }
    
    if (!timestamp_info.empty()) {
        oss << "<p><strong>Created:</strong> " << TrustVisualization::escapeHtml(timestamp_info) << "</p>";
    }
    
    if (!metadata_info.empty()) {
        oss << "<p><strong>Details:</strong> " << TrustVisualization::escapeHtml(metadata_info) << "</p>";
    }
    
    if (!warnings.empty()) {
        oss << "<div class=\"warnings\">";
        oss << "<strong>⚠️ Warnings:</strong>";
        oss << "<ul>";
        for (const auto& warning : warnings) {
            oss << "<li>" << TrustVisualization::escapeHtml(warning) << "</li>";
        }
        oss << "</ul>";
        oss << "</div>";
    }
    
    oss << "</div>";
    return oss.str();
}

std::string HoverTooltip::toMarkdown() const {
    std::ostringstream oss;
    oss << "**" << title << "**\n\n";
    
    if (!origin_info.empty()) {
        oss << "**Origin:** " << origin_info << "\n";
    }
    
    if (!trust_info.empty()) {
        oss << "**Trust:** " << trust_info << "\n";
    }
    
    if (!timestamp_info.empty()) {
        oss << "**Created:** " << timestamp_info << "\n";
    }
    
    if (!metadata_info.empty()) {
        oss << "**Details:** " << metadata_info << "\n";
    }
    
    if (!warnings.empty()) {
        oss << "\n**⚠️ Warnings:**\n";
        for (const auto& warning : warnings) {
            oss << "- " << warning << "\n";
        }
    }
    
    return oss.str();
}

std::string HoverTooltip::toJson() const {
    std::ostringstream oss;
    oss << "{\"title\":\"" << TrustVisualization::escapeJson(title) << "\"";
    
    if (!origin_info.empty()) {
        oss << ",\"origin\":\"" << TrustVisualization::escapeJson(origin_info) << "\"";
    }
    
    if (!trust_info.empty()) {
        oss << ",\"trust\":\"" << TrustVisualization::escapeJson(trust_info) << "\"";
    }
    
    if (!timestamp_info.empty()) {
        oss << ",\"timestamp\":\"" << TrustVisualization::escapeJson(timestamp_info) << "\"";
    }
    
    if (!metadata_info.empty()) {
        oss << ",\"metadata\":\"" << TrustVisualization::escapeJson(metadata_info) << "\"";
    }
    
    if (!warnings.empty()) {
        oss << ",\"warnings\":[";
        for (size_t i = 0; i < warnings.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << TrustVisualization::escapeJson(warnings[i]) << "\"";
        }
        oss << "]";
    }
    
    oss << ",\"html\":\"" << TrustVisualization::escapeJson(toHtml()) << "\"";
    oss << ",\"markdown\":\"" << TrustVisualization::escapeJson(toMarkdown()) << "\"";
    oss << "}";
    return oss.str();
}

// TrustVisualization implementation
std::optional<TrustHeatmapColor> TrustVisualization::getColorTinting(const Value& node) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return std::nullopt;
    }
    
    double trust_score = provenance::Provenance::calculateTrustScore(*metadata);
    return getTrustColor(trust_score);
}

std::optional<GutterIcon> TrustVisualization::getGutterIcon(const Value& node) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return std::nullopt;
    }
    
    double trust_score = provenance::Provenance::calculateTrustScore(*metadata);
    return getOriginIcon(metadata->origin, trust_score);
}

std::optional<HoverTooltip> TrustVisualization::getHoverTooltip(const Value& node) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return std::nullopt;
    }
    
    HoverTooltip tooltip;
    double trust_score = provenance::Provenance::calculateTrustScore(*metadata);
    
    // Set title based on origin
    tooltip.title = "Code Provenance - " + getOriginDescription(metadata->origin);
    
    // Origin information
    tooltip.origin_info = getOriginDescription(metadata->origin);
    if (metadata->origin == provenance::OriginType::Agent && metadata->agent_model) {
        tooltip.origin_info += " (" + *metadata->agent_model + ")";
    }
    
    // Trust information
    tooltip.trust_info = getTrustLevelDescription(trust_score) + 
                        " (" + std::to_string(static_cast<int>(trust_score * 100)) + "%)";
    
    // Timestamp information
    tooltip.timestamp_info = formatTimestamp(metadata->creation_timestamp) + 
                           " (" + getTimeAgo(metadata->creation_timestamp) + ")";
    
    // Additional metadata
    std::ostringstream metadata_oss;
    if (metadata->author_email) {
        metadata_oss << "Author: " << *metadata->author_email;
    }
    if (metadata->reviewer_email) {
        if (!metadata_oss.str().empty()) metadata_oss << ", ";
        metadata_oss << "Reviewer: " << *metadata->reviewer_email;
    }
    if (metadata->confidence_score && metadata->origin == provenance::OriginType::Agent) {
        if (!metadata_oss.str().empty()) metadata_oss << ", ";
        metadata_oss << "AI Confidence: " << static_cast<int>(*metadata->confidence_score * 100) << "%";
    }
    if (metadata->blueprint_id) {
        if (!metadata_oss.str().empty()) metadata_oss << ", ";
        metadata_oss << "Blueprint: " << *metadata->blueprint_id;
    }
    tooltip.metadata_info = metadata_oss.str();
    
    // Check for warnings
    if (metadata->origin == provenance::OriginType::Agent) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - metadata->creation_timestamp);
        
        if (age.count() > 24 * 7) { // Older than a week
            tooltip.warnings.push_back("AI-generated code is older than 7 days and may need review");
        }
        
        if (metadata->confidence_score && *metadata->confidence_score < 0.7) {
            tooltip.warnings.push_back("Low AI confidence score - consider human review");
        }
    }
    
    if (metadata->origin == provenance::OriginType::Verified && metadata->verification_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - *metadata->verification_timestamp);
        
        if (age.count() > 24 * 30) { // Older than 30 days
            tooltip.warnings.push_back("Verification has expired - consider re-verification");
        }
    }
    
    return tooltip;
}

std::string TrustVisualization::generateVisualizationData(const Value& node) {
    std::ostringstream oss;
    oss << "{";
    
    auto color = getColorTinting(node);
    auto icon = getGutterIcon(node);
    auto tooltip = getHoverTooltip(node);
    
    oss << "\"hasProvenance\":" << (color.has_value() ? "true" : "false");
    
    if (color) {
        oss << ",\"color\":" << color->toJson();
    }
    
    if (icon) {
        oss << ",\"icon\":" << icon->toJson();
    }
    
    if (tooltip) {
        oss << ",\"tooltip\":" << tooltip->toJson();
    }
    
    // Add trust score if available
    auto metadata = provenance::Provenance::getProvenance(node);
    if (metadata) {
        double trust_score = provenance::Provenance::calculateTrustScore(*metadata);
        oss << ",\"trustScore\":" << std::fixed << std::setprecision(3) << trust_score;
        oss << ",\"trustLevel\":\"" << getTrustLevelDescription(trust_score) << "\"";
        oss << ",\"origin\":\"" << getOriginDescription(metadata->origin) << "\"";
    }
    
    oss << "}";
    return oss.str();
}

std::string TrustVisualization::generateBatchVisualizationData(const std::vector<Value>& nodes) {
    std::ostringstream oss;
    oss << "[";
    
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << generateVisualizationData(nodes[i]);
    }
    
    oss << "]";
    return oss.str();
}

std::string TrustVisualization::getTrustLevelDescription(double trust_score) {
    if (trust_score >= provenance::TrustLevels::VERIFIED) {
        return "Verified";
    } else if (trust_score >= provenance::TrustLevels::HIGH_CONFIDENCE) {
        return "High Confidence";
    } else if (trust_score >= provenance::TrustLevels::MEDIUM_CONFIDENCE) {
        return "Medium Confidence";
    } else if (trust_score >= provenance::TrustLevels::LOW_CONFIDENCE) {
        return "Low Confidence";
    } else {
        return "Untrusted";
    }
}

std::string TrustVisualization::getOriginDescription(provenance::OriginType origin) {
    switch (origin) {
        case provenance::OriginType::Human:
            return "Human-authored";
        case provenance::OriginType::Agent:
            return "AI-generated";
        case provenance::OriginType::Verified:
            return "Human-verified";
        default:
            return "Unknown";
    }
}

std::string TrustVisualization::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string TrustVisualization::getTimeAgo(const std::chrono::system_clock::time_point& timestamp) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
    
    auto seconds = duration.count();
    
    if (seconds < 60) {
        return "just now";
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        return std::to_string(minutes) + (minutes == 1 ? " minute ago" : " minutes ago");
    } else if (seconds < 86400) {
        int hours = seconds / 3600;
        return std::to_string(hours) + (hours == 1 ? " hour ago" : " hours ago");
    } else if (seconds < 2592000) {
        int days = seconds / 86400;
        return std::to_string(days) + (days == 1 ? " day ago" : " days ago");
    } else if (seconds < 31536000) {
        int months = seconds / 2592000;
        return std::to_string(months) + (months == 1 ? " month ago" : " months ago");
    } else {
        int years = seconds / 31536000;
        return std::to_string(years) + (years == 1 ? " year ago" : " years ago");
    }
}

TrustHeatmapColor TrustVisualization::getTrustColor(double trust_score) {
    if (trust_score >= provenance::TrustLevels::VERIFIED) {
        return HUMAN_VERIFIED_COLOR;
    } else if (trust_score >= provenance::TrustLevels::HIGH_CONFIDENCE) {
        return HIGH_CONFIDENCE_AGENT_COLOR;
    } else if (trust_score >= provenance::TrustLevels::MEDIUM_CONFIDENCE) {
        return MEDIUM_CONFIDENCE_AGENT_COLOR;
    } else if (trust_score >= provenance::TrustLevels::LOW_CONFIDENCE) {
        return LOW_CONFIDENCE_AGENT_COLOR;
    } else {
        return UNTRUSTED_COLOR;
    }
}

GutterIcon TrustVisualization::getOriginIcon(provenance::OriginType origin, double trust_score) {
    TrustHeatmapColor icon_color = getTrustColor(trust_score);
    
    switch (origin) {
        case provenance::OriginType::Human:
            return GutterIcon("👤", "Human-authored code", icon_color);
        case provenance::OriginType::Agent:
            return GutterIcon("🤖", "AI-generated code", icon_color);
        case provenance::OriginType::Verified:
            return GutterIcon("✅", "Human-verified code", icon_color);
        default:
            return GutterIcon("❓", "Unknown origin", UNTRUSTED_COLOR);
    }
}

std::string TrustVisualization::escapeHtml(const std::string& text) {
    std::string result;
    result.reserve(text.length() * 1.1); // Reserve some extra space
    
    for (char c : text) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

std::string TrustVisualization::escapeJson(const std::string& text) {
    std::string result;
    result.reserve(text.length() * 1.1); // Reserve some extra space
    
    for (char c : text) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

// TrustHeatmapConfig implementation
std::string TrustHeatmapConfig::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"enableColorTinting\":" << (enable_color_tinting ? "true" : "false") << ",";
    oss << "\"enableGutterIcons\":" << (enable_gutter_icons ? "true" : "false") << ",";
    oss << "\"enableHoverTooltips\":" << (enable_hover_tooltips ? "true" : "false") << ",";
    oss << "\"showTrustScores\":" << (show_trust_scores ? "true" : "false") << ",";
    oss << "\"showTimestamps\":" << (show_timestamps ? "true" : "false") << ",";
    oss << "\"showDetailedMetadata\":" << (show_detailed_metadata ? "true" : "false") << ",";
    oss << "\"opacityFactor\":" << std::fixed << std::setprecision(2) << opacity_factor;
    oss << "}";
    return oss.str();
}

TrustHeatmapConfig TrustHeatmapConfig::fromJson(const std::string& json) {
    // Simple JSON parsing - in a real implementation, use a proper JSON library
    TrustHeatmapConfig config;
    
    // This is a simplified parser for demonstration
    // In production, use a proper JSON library like nlohmann/json
    if (json.find("\"enableColorTinting\":false") != std::string::npos) {
        config.enable_color_tinting = false;
    }
    if (json.find("\"enableGutterIcons\":false") != std::string::npos) {
        config.enable_gutter_icons = false;
    }
    if (json.find("\"enableHoverTooltips\":false") != std::string::npos) {
        config.enable_hover_tooltips = false;
    }
    if (json.find("\"showTrustScores\":false") != std::string::npos) {
        config.show_trust_scores = false;
    }
    if (json.find("\"showTimestamps\":false") != std::string::npos) {
        config.show_timestamps = false;
    }
    if (json.find("\"showDetailedMetadata\":true") != std::string::npos) {
        config.show_detailed_metadata = true;
    }
    
    // Extract opacity factor (simplified)
    size_t opacity_pos = json.find("\"opacityFactor\":");
    if (opacity_pos != std::string::npos) {
        size_t start = json.find_first_of("0123456789.", opacity_pos);
        if (start != std::string::npos) {
            size_t end = json.find_first_not_of("0123456789.", start);
            if (end != std::string::npos) {
                std::string opacity_str = json.substr(start, end - start);
                try {
                    config.opacity_factor = std::stod(opacity_str);
                } catch (...) {
                    // Keep default value
                }
            }
        }
    }
    
    return config;
}

// TrustHeatmapProvider implementation
TrustHeatmapProvider::TrustHeatmapProvider(const TrustHeatmapConfig& config) 
    : config_(config) {}

std::string TrustHeatmapProvider::processFile(const std::string& file_path, const std::vector<Value>& ast_nodes) {
    std::ostringstream oss;
    oss << "{";
    oss << generateFileHeader(file_path, static_cast<int>(ast_nodes.size()));
    oss << ",\"nodes\":[";
    
    for (size_t i = 0; i < ast_nodes.size(); ++i) {
        if (i > 0) oss << ",";
        // In a real implementation, you would extract line/column from AST node metadata
        oss << generateNodeVisualization(ast_nodes[i], static_cast<int>(i + 1), 1);
    }
    
    oss << "]";
    oss << ",\"config\":" << config_.toJson();
    oss << "}";
    return oss.str();
}

std::string TrustHeatmapProvider::processNode(const Value& node, int line_number, int column_number) {
    return generateNodeVisualization(node, line_number, column_number);
}

void TrustHeatmapProvider::updateConfig(const TrustHeatmapConfig& config) {
    config_ = config;
}

std::string TrustHeatmapProvider::generateNodeVisualization(const Value& node, int line, int column) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"line\":" << line << ",\"column\":" << column;
    
    auto visualization_data = TrustVisualization::generateVisualizationData(node);
    
    // Remove the outer braces from visualization_data and append
    if (visualization_data.length() > 2) {
        std::string inner_data = visualization_data.substr(1, visualization_data.length() - 2);
        if (!inner_data.empty()) {
            oss << "," << inner_data;
        }
    }
    
    oss << "}";
    return oss.str();
}

std::string TrustHeatmapProvider::generateFileHeader(const std::string& file_path, int total_nodes) {
    std::ostringstream oss;
    oss << "\"file\":\"" << TrustVisualization::escapeJson(file_path) << "\"";
    oss << ",\"totalNodes\":" << total_nodes;
    oss << ",\"timestamp\":\"" << TrustVisualization::formatTimestamp(std::chrono::system_clock::now()) << "\"";
    return oss.str();
}

} // namespace ide
} // namespace meld