#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "meld/parser/ast.hpp"

namespace meld {
namespace ide {

/**
 * Represents a visual node in the flowchart
 */
struct FlowchartNode {
    std::string id;
    std::string label;
    std::string type; // "state", "initial", "terminal"
    std::string shape; // "rectangle", "circle", "diamond"
    std::map<std::string, std::string> properties;
    
    FlowchartNode(const std::string& id, const std::string& label, const std::string& type)
        : id(id), label(label), type(type) {
        // Set default shape based on type
        if (type == "initial") {
            shape = "circle";
            properties["fill"] = "#90EE90"; // Light green
        } else if (type == "terminal") {
            shape = "circle";
            properties["fill"] = "#FFB6C1"; // Light pink
        } else {
            shape = "rectangle";
            properties["fill"] = "#87CEEB"; // Sky blue
        }
    }
};

/**
 * Represents a visual edge in the flowchart
 */
struct FlowchartEdge {
    std::string from;
    std::string to;
    std::string label;
    std::string condition; // Guard condition if any
    std::map<std::string, std::string> properties;
    
    FlowchartEdge(const std::string& from, const std::string& to, const std::string& label)
        : from(from), to(to), label(label) {
        properties["arrow"] = "true";
    }
};

/**
 * Represents a complete flowchart visualization
 */
struct FlowchartVisualization {
    std::string title;
    std::vector<FlowchartNode> nodes;
    std::vector<FlowchartEdge> edges;
    std::map<std::string, std::string> metadata;
    
    FlowchartVisualization(const std::string& title) : title(title) {}
};

/**
 * Export formats supported by the flow visualizer
 */
enum class ExportFormat {
    MERMAID,    // Mermaid.js format for web rendering
    DOT,        // Graphviz DOT format
    JSON,       // JSON format for IDE integration
    SVG,        // SVG format for direct rendering
    PLANTUML    // PlantUML format
};

/**
 * Flow visualizer that converts flow definitions to visual representations
 */
class FlowVisualizer {
public:
    /**
     * Creates a flowchart visualization from a flow definition
     */
    static FlowchartVisualization createVisualization(const parser::ast::flow_definition& flow);
    
    /**
     * Exports a flowchart to the specified format
     */
    static std::string exportToFormat(const FlowchartVisualization& chart, ExportFormat format);
    
    /**
     * Exports a flow definition directly to the specified format
     */
    static std::string exportFlow(const parser::ast::flow_definition& flow, ExportFormat format);
    
    /**
     * Generates Mermaid.js flowchart syntax
     */
    static std::string toMermaid(const FlowchartVisualization& chart);
    
    /**
     * Generates Graphviz DOT syntax
     */
    static std::string toDot(const FlowchartVisualization& chart);
    
    /**
     * Generates JSON representation for IDE integration
     */
    static std::string toJson(const FlowchartVisualization& chart);
    
    /**
     * Generates SVG representation
     */
    static std::string toSvg(const FlowchartVisualization& chart);
    
    /**
     * Generates PlantUML syntax
     */
    static std::string toPlantUml(const FlowchartVisualization& chart);
    
    /**
     * Validates that a flow definition can be visualized
     */
    static bool canVisualize(const parser::ast::flow_definition& flow);
    
    /**
     * Gets visualization metadata for IDE integration
     */
    static std::map<std::string, std::string> getVisualizationMetadata(const parser::ast::flow_definition& flow);

    /**
     * Escapes special characters for JSON output
     */
    static std::string escapeJson(const std::string& input);

private:
    /**
     * Sanitizes a string for use in various export formats
     */
    static std::string sanitizeForFormat(const std::string& input, ExportFormat format);
    
    /**
     * Generates a unique node ID from a state name
     */
    static std::string generateNodeId(const std::string& stateName);
    
    /**
     * Determines if a state is terminal (has no outgoing transitions)
     */
    static bool isTerminalState(const parser::ast::flow_state& state);
    
    /**
     * Formats a guard condition for display
     */
    static std::string formatGuardCondition(const parser::ast::flow_guard_condition& guard);
    
    /**
     * Escapes special characters for XML/SVG output
     */
    static std::string escapeXml(const std::string& input);
};

/**
 * IDE integration helper for flow visualization
 */
class FlowIdeIntegration {
public:
    /**
     * Generates IDE-specific visualization data
     */
    static std::string generateIdeVisualization(const parser::ast::flow_definition& flow);
    
    /**
     * Creates hover information for flow elements
     */
    static std::string createHoverInfo(const parser::ast::flow_state& state);
    static std::string createHoverInfo(const parser::ast::flow_transition& transition);
    
    /**
     * Generates code navigation links for flow elements
     */
    static std::map<std::string, std::string> generateNavigationLinks(const parser::ast::flow_definition& flow);
    
    /**
     * Creates interactive visualization data for IDEs
     */
    static std::string createInteractiveVisualization(const parser::ast::flow_definition& flow);
    
    /**
     * Validates flow definition for IDE rendering
     */
    static std::vector<std::string> validateForIde(const parser::ast::flow_definition& flow);
};

} // namespace ide
} // namespace meld