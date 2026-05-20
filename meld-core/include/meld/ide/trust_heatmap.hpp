#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "meld/kernel/primitives.hpp"
#include "meld/provenance/provenance.hpp"

namespace meld {
namespace ide {

using kernel::Value;

struct TrustHeatmapColor {
    int red, green, blue, alpha;
    TrustHeatmapColor(int r, int g, int b, int a = 255) 
        : red(r), green(g), blue(b), alpha(a) {}
    std::string toHex() const;
    std::string toCssRgba() const;
    std::string toJson() const;
};

struct GutterIcon {
    std::string unicode_symbol;
    std::string svg_path;
    std::string tooltip_text;
    TrustHeatmapColor color;
    GutterIcon(const std::string& symbol, const std::string& tooltip, const TrustHeatmapColor& c)
        : unicode_symbol(symbol), tooltip_text(tooltip), color(c) {}
    std::string toJson() const;
};

struct HoverTooltip {
    std::string title;
    std::string origin_info;
    std::string trust_info;
    std::string timestamp_info;
    std::string metadata_info;
    std::vector<std::string> warnings;
    std::string toHtml() const;
    std::string toMarkdown() const;
    std::string toJson() const;
};

class TrustVisualization {
public:
    static std::optional<TrustHeatmapColor> getColorTinting(const Value& node);
    static std::optional<GutterIcon> getGutterIcon(const Value& node);
    static std::optional<HoverTooltip> getHoverTooltip(const Value& node);
    static std::string generateVisualizationData(const Value& node);
    static std::string generateBatchVisualizationData(const std::vector<Value>& nodes);
    static std::string getTrustLevelDescription(double trust_score);
    static std::string getOriginDescription(provenance::OriginType origin);
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp);
    static std::string getTimeAgo(const std::chrono::system_clock::time_point& timestamp);

    // Utility functions (public for use by related classes)
    static std::string escapeHtml(const std::string& text);
    static std::string escapeJson(const std::string& text);

private:
    static const TrustHeatmapColor HUMAN_VERIFIED_COLOR;
    static const TrustHeatmapColor HIGH_CONFIDENCE_AGENT_COLOR;
    static const TrustHeatmapColor MEDIUM_CONFIDENCE_AGENT_COLOR;
    static const TrustHeatmapColor LOW_CONFIDENCE_AGENT_COLOR;
    static const TrustHeatmapColor UNTRUSTED_COLOR;
    
    static TrustHeatmapColor getTrustColor(double trust_score);
    static GutterIcon getOriginIcon(provenance::OriginType origin, double trust_score);
};

struct TrustHeatmapConfig {
    bool enable_color_tinting = true;
    bool enable_gutter_icons = true;
    bool enable_hover_tooltips = true;
    bool show_trust_scores = true;
    bool show_timestamps = true;
    bool show_detailed_metadata = false;
    double opacity_factor = 0.3;
    std::string toJson() const;
    static TrustHeatmapConfig fromJson(const std::string& json);
};

class TrustHeatmapProvider {
public:
    explicit TrustHeatmapProvider(const TrustHeatmapConfig& config = TrustHeatmapConfig{});
    std::string processFile(const std::string& file_path, const std::vector<Value>& ast_nodes);
    std::string processNode(const Value& node, int line_number, int column_number);
    void updateConfig(const TrustHeatmapConfig& config);
    const TrustHeatmapConfig& getConfig() const { return config_; }

private:
    TrustHeatmapConfig config_;
    std::string generateNodeVisualization(const Value& node, int line, int column);
    std::string generateFileHeader(const std::string& file_path, int total_nodes);
};

} // namespace ide
} // namespace meld
