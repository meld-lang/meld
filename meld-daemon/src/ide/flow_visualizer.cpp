#include "meld/ide/flow_visualizer.hpp"
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace meld {
namespace ide {

namespace ast = parser::ast;

FlowchartVisualization FlowVisualizer::createVisualization(const ast::flow_definition& flow) {
    FlowchartVisualization chart(flow.name.name);
    
    // Add metadata
    chart.metadata["type"] = "state_machine";
    chart.metadata["initial_state"] = flow.initial_state.name;
    chart.metadata["state_count"] = std::to_string(flow.states.size());
    
    // Create nodes for each state
    for (const auto& state : flow.states) {
        std::string nodeId = generateNodeId(state.name.name);
        std::string nodeType = "state";
        
        // Determine node type
        if (state.name.name == flow.initial_state.name) {
            nodeType = "initial";
        } else if (isTerminalState(state)) {
            nodeType = "terminal";
        }
        
        FlowchartNode node(nodeId, state.name.name, nodeType);
        
        // Add entry/exit action information
        if (state.has_entry_action) {
            node.properties["has_entry"] = "true";
            node.properties["entry_tooltip"] = "Has entry action";
        }
        if (state.has_exit_action) {
            node.properties["has_exit"] = "true";
            node.properties["exit_tooltip"] = "Has exit action";
        }
        
        chart.nodes.push_back(node);
    }
    
    // Create edges for transitions
    for (const auto& state : flow.states) {
        std::string fromId = generateNodeId(state.name.name);
        
        for (const auto& transition : state.transitions) {
            std::string toId = generateNodeId(transition.target_state.name);
            std::string label = transition.event_name.name;
            
            FlowchartEdge edge(fromId, toId, label);
            
            // Add guard condition if present
            if (transition.has_guard) {
                edge.condition = formatGuardCondition(transition.guard);
                edge.properties["has_guard"] = "true";
                edge.properties["guard_tooltip"] = "Guard: " + edge.condition;
            }
            
            chart.edges.push_back(edge);
        }
    }
    
    return chart;
}

std::string FlowVisualizer::exportToFormat(const FlowchartVisualization& chart, ExportFormat format) {
    switch (format) {
        case ExportFormat::MERMAID:
            return toMermaid(chart);
        case ExportFormat::DOT:
            return toDot(chart);
        case ExportFormat::JSON:
            return toJson(chart);
        case ExportFormat::SVG:
            return toSvg(chart);
        case ExportFormat::PLANTUML:
            return toPlantUml(chart);
        default:
            return "";
    }
}

std::string FlowVisualizer::exportFlow(const ast::flow_definition& flow, ExportFormat format) {
    auto chart = createVisualization(flow);
    return exportToFormat(chart, format);
}

std::string FlowVisualizer::toMermaid(const FlowchartVisualization& chart) {
    std::ostringstream ss;
    
    ss << "flowchart TD\n";
    ss << "    %% " << chart.title << " State Machine\n\n";
    
    // Define nodes
    for (const auto& node : chart.nodes) {
        ss << "    " << node.id << "[\"" << node.label << "\"]\n";
        
        // Add styling based on node type
        if (node.type == "initial") {
            ss << "    " << node.id << ":::initial\n";
        } else if (node.type == "terminal") {
            ss << "    " << node.id << ":::terminal\n";
        }
    }
    
    ss << "\n";
    
    // Define edges
    for (const auto& edge : chart.edges) {
        ss << "    " << edge.from << " --> " << edge.to;
        if (!edge.label.empty()) {
            ss << " : " << edge.label;
            if (!edge.condition.empty()) {
                ss << "\\n[" << edge.condition << "]";
            }
        }
        ss << "\n";
    }
    
    // Add CSS classes
    ss << "\n    classDef initial fill:#90EE90,stroke:#333,stroke-width:2px\n";
    ss << "    classDef terminal fill:#FFB6C1,stroke:#333,stroke-width:2px\n";
    
    return ss.str();
}

std::string FlowVisualizer::toDot(const FlowchartVisualization& chart) {
    std::ostringstream ss;
    
    ss << "digraph \"" << chart.title << "\" {\n";
    ss << "    rankdir=TD;\n";
    ss << "    node [shape=box, style=rounded];\n\n";
    
    // Define nodes
    for (const auto& node : chart.nodes) {
        ss << "    " << node.id << " [label=\"" << node.label << "\"";
        
        if (node.type == "initial") {
            ss << ", fillcolor=lightgreen, style=\"rounded,filled\"";
        } else if (node.type == "terminal") {
            ss << ", fillcolor=lightpink, style=\"rounded,filled\"";
        } else {
            ss << ", fillcolor=lightblue, style=\"rounded,filled\"";
        }
        
        ss << "];\n";
    }
    
    ss << "\n";
    
    // Define edges
    for (const auto& edge : chart.edges) {
        ss << "    " << edge.from << " -> " << edge.to;
        if (!edge.label.empty()) {
            ss << " [label=\"" << edge.label;
            if (!edge.condition.empty()) {
                ss << "\\n[" << edge.condition << "]";
            }
            ss << "\"]";
        }
        ss << ";\n";
    }
    
    ss << "}\n";
    
    return ss.str();
}

std::string FlowVisualizer::toJson(const FlowchartVisualization& chart) {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"title\": \"" << escapeJson(chart.title) << "\",\n";
    ss << "  \"type\": \"flowchart\",\n";
    ss << "  \"metadata\": {\n";
    
    bool first = true;
    for (const auto& [key, value] : chart.metadata) {
        if (!first) ss << ",\n";
        ss << "    \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
        first = false;
    }
    
    ss << "\n  },\n";
    ss << "  \"nodes\": [\n";
    
    first = true;
    for (const auto& node : chart.nodes) {
        if (!first) ss << ",\n";
        ss << "    {\n";
        ss << "      \"id\": \"" << escapeJson(node.id) << "\",\n";
        ss << "      \"label\": \"" << escapeJson(node.label) << "\",\n";
        ss << "      \"type\": \"" << escapeJson(node.type) << "\",\n";
        ss << "      \"shape\": \"" << escapeJson(node.shape) << "\",\n";
        ss << "      \"properties\": {\n";
        
        bool firstProp = true;
        for (const auto& [key, value] : node.properties) {
            if (!firstProp) ss << ",\n";
            ss << "        \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
            firstProp = false;
        }
        
        ss << "\n      }\n";
        ss << "    }";
        first = false;
    }
    
    ss << "\n  ],\n";
    ss << "  \"edges\": [\n";
    
    first = true;
    for (const auto& edge : chart.edges) {
        if (!first) ss << ",\n";
        ss << "    {\n";
        ss << "      \"from\": \"" << escapeJson(edge.from) << "\",\n";
        ss << "      \"to\": \"" << escapeJson(edge.to) << "\",\n";
        ss << "      \"label\": \"" << escapeJson(edge.label) << "\",\n";
        ss << "      \"condition\": \"" << escapeJson(edge.condition) << "\",\n";
        ss << "      \"properties\": {\n";
        
        bool firstProp = true;
        for (const auto& [key, value] : edge.properties) {
            if (!firstProp) ss << ",\n";
            ss << "        \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
            firstProp = false;
        }
        
        ss << "\n      }\n";
        ss << "    }";
        first = false;
    }
    
    ss << "\n  ]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string FlowVisualizer::toSvg(const FlowchartVisualization& chart) {
    std::ostringstream ss;
    
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"0 0 800 600\">\n";
    ss << "  <title>" << escapeXml(chart.title) << "</title>\n";
    ss << "  <defs>\n";
    ss << "    <marker id=\"arrowhead\" markerWidth=\"10\" markerHeight=\"7\" refX=\"9\" refY=\"3.5\" orient=\"auto\">\n";
    ss << "      <polygon points=\"0 0, 10 3.5, 0 7\" fill=\"black\" />\n";
    ss << "    </marker>\n";
    ss << "  </defs>\n\n";
    
    // Simple layout: arrange nodes in a grid
    int cols = std::max(1, (int)std::sqrt(chart.nodes.size()));
    int nodeWidth = 120;
    int nodeHeight = 60;
    int spacing = 150;
    
    std::map<std::string, std::pair<int, int>> nodePositions;
    
    // Position nodes
    for (size_t i = 0; i < chart.nodes.size(); ++i) {
        int col = i % cols;
        int row = i / cols;
        int x = 100 + col * spacing;
        int y = 100 + row * spacing;
        
        nodePositions[chart.nodes[i].id] = {x, y};
        
        const auto& node = chart.nodes[i];
        
        // Draw node
        std::string fill = "#87CEEB"; // Default sky blue
        if (node.type == "initial") {
            fill = "#90EE90"; // Light green
        } else if (node.type == "terminal") {
            fill = "#FFB6C1"; // Light pink
        }
        
        ss << "  <rect x=\"" << (x - nodeWidth/2) << "\" y=\"" << (y - nodeHeight/2) << "\" ";
        ss << "width=\"" << nodeWidth << "\" height=\"" << nodeHeight << "\" ";
        ss << "fill=\"" << fill << "\" stroke=\"black\" stroke-width=\"2\" rx=\"10\" />\n";
        
        ss << "  <text x=\"" << x << "\" y=\"" << y << "\" text-anchor=\"middle\" ";
        ss << "dominant-baseline=\"middle\" font-family=\"Arial\" font-size=\"14\">";
        ss << escapeXml(node.label) << "</text>\n";
    }
    
    // Draw edges
    for (const auto& edge : chart.edges) {
        auto fromPos = nodePositions[edge.from];
        auto toPos = nodePositions[edge.to];
        
        ss << "  <line x1=\"" << fromPos.first << "\" y1=\"" << fromPos.second << "\" ";
        ss << "x2=\"" << toPos.first << "\" y2=\"" << toPos.second << "\" ";
        ss << "stroke=\"black\" stroke-width=\"2\" marker-end=\"url(#arrowhead)\" />\n";
        
        // Add label
        if (!edge.label.empty()) {
            int midX = (fromPos.first + toPos.first) / 2;
            int midY = (fromPos.second + toPos.second) / 2;
            
            ss << "  <text x=\"" << midX << "\" y=\"" << (midY - 10) << "\" text-anchor=\"middle\" ";
            ss << "font-family=\"Arial\" font-size=\"12\" fill=\"blue\">";
            ss << escapeXml(edge.label);
            if (!edge.condition.empty()) {
                ss << " [" << escapeXml(edge.condition) << "]";
            }
            ss << "</text>\n";
        }
    }
    
    ss << "</svg>\n";
    
    return ss.str();
}

std::string FlowVisualizer::toPlantUml(const FlowchartVisualization& chart) {
    std::ostringstream ss;
    
    ss << "@startuml\n";
    ss << "title " << chart.title << " State Machine\n\n";
    
    // Find initial state
    std::string initialState;
    for (const auto& node : chart.nodes) {
        if (node.type == "initial") {
            initialState = node.label;
            break;
        }
    }
    
    if (!initialState.empty()) {
        ss << "[*] --> " << initialState << "\n";
    }
    
    // Add transitions
    for (const auto& edge : chart.edges) {
        // Find node labels
        std::string fromLabel, toLabel;
        for (const auto& node : chart.nodes) {
            if (node.id == edge.from) fromLabel = node.label;
            if (node.id == edge.to) toLabel = node.label;
        }
        
        ss << fromLabel << " --> " << toLabel;
        if (!edge.label.empty()) {
            ss << " : " << edge.label;
            if (!edge.condition.empty()) {
                ss << " [" << edge.condition << "]";
            }
        }
        ss << "\n";
    }
    
    // Add terminal states
    for (const auto& node : chart.nodes) {
        if (node.type == "terminal") {
            ss << node.label << " --> [*]\n";
        }
    }
    
    ss << "@enduml\n";
    
    return ss.str();
}

bool FlowVisualizer::canVisualize(const ast::flow_definition& flow) {
    // Basic validation
    if (flow.states.empty()) {
        return false;
    }
    
    // Check if initial state exists
    bool initialStateExists = false;
    for (const auto& state : flow.states) {
        if (state.name.name == flow.initial_state.name) {
            initialStateExists = true;
            break;
        }
    }
    
    return initialStateExists;
}

std::map<std::string, std::string> FlowVisualizer::getVisualizationMetadata(const ast::flow_definition& flow) {
    std::map<std::string, std::string> metadata;
    
    metadata["name"] = flow.name.name;
    metadata["initial_state"] = flow.initial_state.name;
    metadata["state_count"] = std::to_string(flow.states.size());
    
    // Count transitions
    int transitionCount = 0;
    for (const auto& state : flow.states) {
        transitionCount += state.transitions.size();
    }
    metadata["transition_count"] = std::to_string(transitionCount);
    
    // Count terminal states
    int terminalCount = 0;
    for (const auto& state : flow.states) {
        if (isTerminalState(state)) {
            terminalCount++;
        }
    }
    metadata["terminal_count"] = std::to_string(terminalCount);
    
    // Check for entry/exit actions
    bool hasEntryActions = false;
    bool hasExitActions = false;
    for (const auto& state : flow.states) {
        if (state.has_entry_action) hasEntryActions = true;
        if (state.has_exit_action) hasExitActions = true;
    }
    metadata["has_entry_actions"] = hasEntryActions ? "true" : "false";
    metadata["has_exit_actions"] = hasExitActions ? "true" : "false";
    
    return metadata;
}

std::string FlowVisualizer::sanitizeForFormat(const std::string& input, ExportFormat format) {
    std::string result = input;
    
    switch (format) {
        case ExportFormat::MERMAID:
            // Replace problematic characters for Mermaid
            std::replace(result.begin(), result.end(), '"', '\'');
            break;
        case ExportFormat::DOT:
            // Escape quotes for DOT format
            {
                std::string escaped;
                for (char c : result) {
                    if (c == '"') escaped += "\\\"";
                    else escaped += c;
                }
                result = escaped;
            }
            break;
        case ExportFormat::JSON:
            result = escapeJson(result);
            break;
        case ExportFormat::SVG:
            result = escapeXml(result);
            break;
        case ExportFormat::PLANTUML:
            // PlantUML is generally tolerant
            break;
    }
    
    return result;
}

std::string FlowVisualizer::generateNodeId(const std::string& stateName) {
    std::string id = "state_" + stateName;
    // Replace spaces and special characters with underscores
    std::replace_if(id.begin(), id.end(), [](char c) {
        return !std::isalnum(c) && c != '_';
    }, '_');
    return id;
}

bool FlowVisualizer::isTerminalState(const ast::flow_state& state) {
    return state.transitions.empty();
}

std::string FlowVisualizer::formatGuardCondition(const ast::flow_guard_condition& guard) {
    // For now, return a placeholder. In a full implementation,
    // this would format the actual guard expression
    return "condition";
}

std::string FlowVisualizer::escapeJson(const std::string& input) {
    std::string result;
    for (char c : input) {
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

std::string FlowVisualizer::escapeXml(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c; break;
        }
    }
    return result;
}

// IDE Integration Implementation

std::string FlowIdeIntegration::generateIdeVisualization(const ast::flow_definition& flow) {
    auto chart = FlowVisualizer::createVisualization(flow);
    
    // Generate enhanced JSON with IDE-specific features
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"type\": \"meld_flow_visualization\",\n";
    ss << "  \"version\": \"1.0\",\n";
    ss << "  \"flow\": " << FlowVisualizer::toJson(chart) << ",\n";
    ss << "  \"ide_features\": {\n";
    ss << "    \"interactive\": true,\n";
    ss << "    \"hover_enabled\": true,\n";
    ss << "    \"navigation_enabled\": true,\n";
    ss << "    \"real_time_updates\": true\n";
    ss << "  },\n";
    
    // Add navigation links
    auto navLinks = generateNavigationLinks(flow);
    ss << "  \"navigation\": {\n";
    bool first = true;
    for (const auto& [key, value] : navLinks) {
        if (!first) ss << ",\n";
        ss << "    \"" << FlowVisualizer::escapeJson(key) << "\": \"" << FlowVisualizer::escapeJson(value) << "\"";
        first = false;
    }
    ss << "\n  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string FlowIdeIntegration::createHoverInfo(const ast::flow_state& state) {
    std::ostringstream ss;
    
    ss << "State: " << state.name.name << "\\n";
    ss << "Transitions: " << state.transitions.size() << "\\n";
    
    if (state.has_entry_action) {
        ss << "Has entry action\\n";
    }
    if (state.has_exit_action) {
        ss << "Has exit action\\n";
    }
    
    if (state.transitions.empty()) {
        ss << "Terminal state";
    }
    
    return ss.str();
}

std::string FlowIdeIntegration::createHoverInfo(const ast::flow_transition& transition) {
    std::ostringstream ss;
    
    ss << "Event: " << transition.event_name.name << "\\n";
    ss << "Target: " << transition.target_state.name << "\\n";
    
    if (transition.has_guard) {
        ss << "Has guard condition";
    }
    
    return ss.str();
}

std::map<std::string, std::string> FlowIdeIntegration::generateNavigationLinks(const ast::flow_definition& flow) {
    std::map<std::string, std::string> links;
    
    // Add link to flow definition
    links["flow_definition"] = "goto:" + flow.name.name;
    
    // Add links to each state
    for (const auto& state : flow.states) {
        links["state_" + state.name.name] = "goto:state:" + state.name.name;
    }
    
    return links;
}

std::string FlowIdeIntegration::createInteractiveVisualization(const ast::flow_definition& flow) {
    auto chart = FlowVisualizer::createVisualization(flow);
    
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"interactive_chart\": " << FlowVisualizer::toJson(chart) << ",\n";
    ss << "  \"interactions\": {\n";
    ss << "    \"node_click\": \"navigate_to_definition\",\n";
    ss << "    \"edge_click\": \"show_transition_details\",\n";
    ss << "    \"node_hover\": \"show_state_info\",\n";
    ss << "    \"edge_hover\": \"show_event_info\"\n";
    ss << "  },\n";
    ss << "  \"rendering\": {\n";
    ss << "    \"layout\": \"hierarchical\",\n";
    ss << "    \"animation\": \"enabled\",\n";
    ss << "    \"zoom\": \"enabled\",\n";
    ss << "    \"pan\": \"enabled\"\n";
    ss << "  }\n";
    ss << "}\n";
    
    return ss.str();
}

std::vector<std::string> FlowIdeIntegration::validateForIde(const ast::flow_definition& flow) {
    std::vector<std::string> issues;
    
    if (!FlowVisualizer::canVisualize(flow)) {
        issues.push_back("Flow cannot be visualized: missing initial state or no states defined");
    }
    
    // Check for very large flows that might be hard to visualize
    if (flow.states.size() > 20) {
        issues.push_back("Warning: Flow has many states (" + std::to_string(flow.states.size()) + "), visualization may be cluttered");
    }
    
    // Check for states with many transitions
    for (const auto& state : flow.states) {
        if (state.transitions.size() > 10) {
            issues.push_back("Warning: State '" + state.name.name + "' has many transitions (" + 
                           std::to_string(state.transitions.size()) + "), may be hard to read");
        }
    }
    
    return issues;
}

} // namespace ide
} // namespace meld