#pragma once

#include "shadow_provenance.hpp"
#include "history_api.hpp"
#include <string>
#include <memory>

namespace meld {
namespace history {

/**
 * Color representation for IDE visualization
 */
struct Color {
    int r, g, b;  // RGB values 0-255
    
    Color(int red, int green, int blue) : r(red), g(green), b(blue) {}
    
    static Color Gray() { return Color(128, 128, 128); }
    static Color Green() { return Color(0, 200, 0); }
    static Color Yellow() { return Color(255, 255, 0); }
    static Color Orange() { return Color(255, 165, 0); }
    static Color Red() { return Color(255, 0, 0); }
};

/**
 * Provides IDE integration features for shadow history visualization
 */
class IDEIntegration {
public:
    explicit IDEIntegration(HistoryAPI& historyAPI);
    
    // Gutter heatmap - returns color based on version count
    Color getGutterColor(const std::string& nodeId);
    
    // Hover tooltip - returns formatted tooltip text
    std::string getHistoryTooltip(const std::string& nodeId);
    
    // History panel - returns formatted history panel content
    std::string getHistoryPanel(const std::string& nodeId);
    
    // Version timeline - returns formatted timeline
    std::string getVersionTimeline(const std::string& nodeId);
    
    // Conversation view - returns formatted conversation
    std::string getConversationView(const std::string& nodeId);
    
    // Trust heatmap intensity (0.0 to 1.0)
    float getTrustIntensity(const std::string& nodeId);
    
    // Get summary for quick view
    std::string getHistorySummary(const std::string& nodeId);
    
private:
    HistoryAPI& historyAPI_;
    
    // Formatting helpers
    std::string formatTimestamp(const Timestamp& ts);
    std::string formatMessage(const Message& msg);
    std::string formatVersion(const CodeVersion& version);
};

} // namespace history
} // namespace meld
