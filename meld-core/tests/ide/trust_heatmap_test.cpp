#include <gtest/gtest.h>
#include "meld/ide/trust_heatmap.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::ide;
using namespace meld::kernel;
using namespace meld::provenance;

class TrustHeatmapTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test nodes with different provenance types
        human_node = Value(std::make_shared<Symbol>("human_function"));
        agent_node = Value(std::make_shared<Symbol>("agent_function"));
        verified_node = Value(std::make_shared<Symbol>("verified_function"));
        no_provenance_node = Value(std::make_shared<Symbol>("no_provenance_function"));
        
        // Attach provenance metadata
        ProvenanceMetadata human_metadata("developer@example.com", "abc123");
        human_node = Provenance::attachProvenance(human_node, human_metadata);
        
        ProvenanceMetadata agent_metadata("gpt-4", 0.85, "blueprint-123");
        agent_node = Provenance::attachProvenance(agent_node, agent_metadata);
        
        ProvenanceMetadata verified_metadata("gpt-4", 0.75, "blueprint-456");
        verified_node = Provenance::attachProvenance(verified_node, verified_metadata);
        verified_node = Provenance::markAsVerified(verified_node, "reviewer@example.com");
    }
    
    Value human_node;
    Value agent_node;
    Value verified_node;
    Value no_provenance_node;
};

// Test color tinting generation
TEST_F(TrustHeatmapTest, ColorTintingGeneration) {
    // Test human-authored code (should be green)
    auto human_color = TrustVisualization::getColorTinting(human_node);
    ASSERT_TRUE(human_color.has_value());
    EXPECT_EQ(human_color->red, 0);
    EXPECT_EQ(human_color->green, 255);
    EXPECT_EQ(human_color->blue, 0);
    
    // Test AI-generated code (should be purple for high confidence)
    auto agent_color = TrustVisualization::getColorTinting(agent_node);
    ASSERT_TRUE(agent_color.has_value());
    EXPECT_EQ(agent_color->red, 128);
    EXPECT_EQ(agent_color->green, 0);
    EXPECT_EQ(agent_color->blue, 255);
    
    // Test verified code (should be green)
    auto verified_color = TrustVisualization::getColorTinting(verified_node);
    ASSERT_TRUE(verified_color.has_value());
    EXPECT_EQ(verified_color->red, 0);
    EXPECT_EQ(verified_color->green, 255);
    EXPECT_EQ(verified_color->blue, 0);
    
    // Test node without provenance (should return nullopt)
    auto no_color = TrustVisualization::getColorTinting(no_provenance_node);
    EXPECT_FALSE(no_color.has_value());
}

// Test gutter icon generation
TEST_F(TrustHeatmapTest, GutterIconGeneration) {
    // Test human-authored code (should have human icon)
    auto human_icon = TrustVisualization::getGutterIcon(human_node);
    ASSERT_TRUE(human_icon.has_value());
    EXPECT_EQ(human_icon->unicode_symbol, "👤");
    EXPECT_EQ(human_icon->tooltip_text, "Human-authored code");
    
    // Test AI-generated code (should have robot icon)
    auto agent_icon = TrustVisualization::getGutterIcon(agent_node);
    ASSERT_TRUE(agent_icon.has_value());
    EXPECT_EQ(agent_icon->unicode_symbol, "🤖");
    EXPECT_EQ(agent_icon->tooltip_text, "AI-generated code");
    
    // Test verified code (should have checkmark icon)
    auto verified_icon = TrustVisualization::getGutterIcon(verified_node);
    ASSERT_TRUE(verified_icon.has_value());
    EXPECT_EQ(verified_icon->unicode_symbol, "✅");
    EXPECT_EQ(verified_icon->tooltip_text, "Human-verified code");
    
    // Test node without provenance (should return nullopt)
    auto no_icon = TrustVisualization::getGutterIcon(no_provenance_node);
    EXPECT_FALSE(no_icon.has_value());
}

// Test hover tooltip generation
TEST_F(TrustHeatmapTest, HoverTooltipGeneration) {
    // Test human-authored code tooltip
    auto human_tooltip = TrustVisualization::getHoverTooltip(human_node);
    ASSERT_TRUE(human_tooltip.has_value());
    EXPECT_EQ(human_tooltip->title, "Code Provenance - Human-authored");
    EXPECT_EQ(human_tooltip->origin_info, "Human-authored");
    EXPECT_TRUE(human_tooltip->trust_info.find("Verified") != std::string::npos);
    EXPECT_TRUE(human_tooltip->metadata_info.find("developer@example.com") != std::string::npos);
    
    // Test AI-generated code tooltip
    auto agent_tooltip = TrustVisualization::getHoverTooltip(agent_node);
    ASSERT_TRUE(agent_tooltip.has_value());
    EXPECT_EQ(agent_tooltip->title, "Code Provenance - AI-generated");
    EXPECT_EQ(agent_tooltip->origin_info, "AI-generated (gpt-4)");
    EXPECT_TRUE(agent_tooltip->trust_info.find("High Confidence") != std::string::npos);
    EXPECT_TRUE(agent_tooltip->metadata_info.find("AI Confidence: 85%") != std::string::npos);
    
    // Test verified code tooltip
    auto verified_tooltip = TrustVisualization::getHoverTooltip(verified_node);
    ASSERT_TRUE(verified_tooltip.has_value());
    EXPECT_EQ(verified_tooltip->title, "Code Provenance - Human-verified");
    EXPECT_EQ(verified_tooltip->origin_info, "Human-verified (gpt-4)");
    EXPECT_TRUE(verified_tooltip->metadata_info.find("reviewer@example.com") != std::string::npos);
    
    // Test node without provenance (should return nullopt)
    auto no_tooltip = TrustVisualization::getHoverTooltip(no_provenance_node);
    EXPECT_FALSE(no_tooltip.has_value());
}

// Test complete visualization data generation
TEST_F(TrustHeatmapTest, VisualizationDataGeneration) {
    // Test human-authored code
    std::string human_data = TrustVisualization::generateVisualizationData(human_node);
    EXPECT_TRUE(human_data.find("\"hasProvenance\":true") != std::string::npos);
    EXPECT_TRUE(human_data.find("\"trustScore\":1.000") != std::string::npos);
    EXPECT_TRUE(human_data.find("\"trustLevel\":\"Verified\"") != std::string::npos);
    EXPECT_TRUE(human_data.find("\"origin\":\"Human-authored\"") != std::string::npos);
    
    // Test AI-generated code
    std::string agent_data = TrustVisualization::generateVisualizationData(agent_node);
    EXPECT_TRUE(agent_data.find("\"hasProvenance\":true") != std::string::npos);
    EXPECT_TRUE(agent_data.find("\"trustScore\":0.850") != std::string::npos);
    EXPECT_TRUE(agent_data.find("\"trustLevel\":\"High Confidence\"") != std::string::npos);
    EXPECT_TRUE(agent_data.find("\"origin\":\"AI-generated\"") != std::string::npos);
    
    // Test node without provenance
    std::string no_data = TrustVisualization::generateVisualizationData(no_provenance_node);
    EXPECT_TRUE(no_data.find("\"hasProvenance\":false") != std::string::npos);
}

// Test batch visualization data generation
TEST_F(TrustHeatmapTest, BatchVisualizationDataGeneration) {
    std::vector<Value> nodes = {human_node, agent_node, verified_node, no_provenance_node};
    std::string batch_data = TrustVisualization::generateBatchVisualizationData(nodes);
    
    // Should be a JSON array with 4 elements
    EXPECT_TRUE(batch_data.front() == '[');
    EXPECT_TRUE(batch_data.back() == ']');
    
    // Should contain data for all nodes
    EXPECT_TRUE(batch_data.find("\"origin\":\"Human-authored\"") != std::string::npos);
    EXPECT_TRUE(batch_data.find("\"origin\":\"AI-generated\"") != std::string::npos);
    EXPECT_TRUE(batch_data.find("\"origin\":\"Human-verified\"") != std::string::npos);
    EXPECT_TRUE(batch_data.find("\"hasProvenance\":false") != std::string::npos);
}

// Test trust level descriptions
TEST_F(TrustHeatmapTest, TrustLevelDescriptions) {
    EXPECT_EQ(TrustVisualization::getTrustLevelDescription(1.0), "Verified");
    EXPECT_EQ(TrustVisualization::getTrustLevelDescription(0.9), "High Confidence");
    EXPECT_EQ(TrustVisualization::getTrustLevelDescription(0.7), "Medium Confidence");
    EXPECT_EQ(TrustVisualization::getTrustLevelDescription(0.5), "Low Confidence");
    EXPECT_EQ(TrustVisualization::getTrustLevelDescription(0.3), "Untrusted");
}

// Test origin descriptions
TEST_F(TrustHeatmapTest, OriginDescriptions) {
    EXPECT_EQ(TrustVisualization::getOriginDescription(OriginType::Human), "Human-authored");
    EXPECT_EQ(TrustVisualization::getOriginDescription(OriginType::Agent), "AI-generated");
    EXPECT_EQ(TrustVisualization::getOriginDescription(OriginType::Verified), "Human-verified");
}

// Test color conversion methods
TEST_F(TrustHeatmapTest, ColorConversions) {
    TrustHeatmapColor color(255, 128, 64, 200);
    
    // Test hex conversion
    std::string hex = color.toHex();
    EXPECT_EQ(hex, "#ff8040c8");
    
    // Test CSS RGBA conversion
    std::string css = color.toCssRgba();
    EXPECT_EQ(css, "rgba(255, 128, 64, 0.78)");
    
    // Test JSON conversion
    std::string json = color.toJson();
    EXPECT_TRUE(json.find("\"red\":255") != std::string::npos);
    EXPECT_TRUE(json.find("\"green\":128") != std::string::npos);
    EXPECT_TRUE(json.find("\"blue\":64") != std::string::npos);
    EXPECT_TRUE(json.find("\"alpha\":200") != std::string::npos);
}

// Test TrustHeatmapProvider
TEST_F(TrustHeatmapTest, TrustHeatmapProvider) {
    TrustHeatmapConfig config;
    config.enable_color_tinting = true;
    config.enable_gutter_icons = true;
    config.enable_hover_tooltips = true;
    
    TrustHeatmapProvider provider(config);
    
    // Test single node processing
    std::string node_data = provider.processNode(human_node, 10, 5);
    EXPECT_TRUE(node_data.find("\"line\":10") != std::string::npos);
    EXPECT_TRUE(node_data.find("\"column\":5") != std::string::npos);
    EXPECT_TRUE(node_data.find("\"hasProvenance\":true") != std::string::npos);
    
    // Test file processing
    std::vector<Value> nodes = {human_node, agent_node};
    std::string file_data = provider.processFile("test.meld", nodes);
    EXPECT_TRUE(file_data.find("\"file\":\"test.meld\"") != std::string::npos);
    EXPECT_TRUE(file_data.find("\"totalNodes\":2") != std::string::npos);
    EXPECT_TRUE(file_data.find("\"nodes\":[") != std::string::npos);
}

// Test configuration serialization
TEST_F(TrustHeatmapTest, ConfigurationSerialization) {
    TrustHeatmapConfig config;
    config.enable_color_tinting = false;
    config.opacity_factor = 0.5;
    
    std::string json = config.toJson();
    EXPECT_TRUE(json.find("\"enableColorTinting\":false") != std::string::npos);
    EXPECT_TRUE(json.find("\"opacityFactor\":0.50") != std::string::npos);
    
    // Test deserialization
    TrustHeatmapConfig loaded = TrustHeatmapConfig::fromJson(json);
    EXPECT_FALSE(loaded.enable_color_tinting);
    EXPECT_DOUBLE_EQ(loaded.opacity_factor, 0.5);
}

// Test HTML and JSON escaping
TEST_F(TrustHeatmapTest, EscapingFunctions) {
    std::string html_input = "<script>alert('test');</script>";
    std::string escaped_html = TrustVisualization::escapeHtml(html_input);
    EXPECT_EQ(escaped_html, "&lt;script&gt;alert(&#39;test&#39;);&lt;/script&gt;");
    
    std::string json_input = "He said \"Hello\\World\"";
    std::string escaped_json = TrustVisualization::escapeJson(json_input);
    EXPECT_EQ(escaped_json, "He said \\\"Hello\\\\World\\\"");
}

// Test tooltip format conversions
TEST_F(TrustHeatmapTest, TooltipFormatConversions) {
    auto tooltip = TrustVisualization::getHoverTooltip(human_node);
    ASSERT_TRUE(tooltip.has_value());
    
    // Test HTML conversion
    std::string html = tooltip->toHtml();
    EXPECT_TRUE(html.find("<div class=\"meld-trust-tooltip\">") != std::string::npos);
    EXPECT_TRUE(html.find("<h4>Code Provenance - Human-authored</h4>") != std::string::npos);
    
    // Test Markdown conversion
    std::string markdown = tooltip->toMarkdown();
    EXPECT_TRUE(markdown.find("**Code Provenance - Human-authored**") != std::string::npos);
    EXPECT_TRUE(markdown.find("**Origin:** Human-authored") != std::string::npos);
    
    // Test JSON conversion
    std::string json = tooltip->toJson();
    EXPECT_TRUE(json.find("\"title\":\"Code Provenance - Human-authored\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"html\":") != std::string::npos);
    EXPECT_TRUE(json.find("\"markdown\":") != std::string::npos);
}