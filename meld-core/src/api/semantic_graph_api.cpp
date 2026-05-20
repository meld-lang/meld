#include "meld/api/semantic_graph_api.hpp"
#include "meld/parser/parser.hpp" // Assuming parser exists
#include "meld/api/asg_builder.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <queue>
#include <stack>

namespace meld::api {

// Static method implementations

std::unique_ptr<SemanticGraph> SemanticGraphAPI::parse(const std::string& source_code, 
                                                      const std::string& file_path) {
    try {
        // Parse source code to AST
        parser::Parser parser;
        std::vector<parser::ast::expression> ast_nodes;
        if (!parser.parse_file(source_code, ast_nodes)) {
            throw std::runtime_error("Parse error: " + parser.error_message());
        }
        
        // Build semantic graph from AST
        auto graph = std::make_unique<SemanticGraph>();
        ASGBuilder builder(*graph, file_path);
        for (const auto& expr : ast_nodes) {
            builder.buildFromAST(expr);
        }
        return graph;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse source code: " + std::string(e.what()));
    }
}

std::unique_ptr<SemanticGraph> SemanticGraphAPI::parseFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source_code = buffer.str();
    
    return parse(source_code, file_path);
}

std::unique_ptr<SemanticGraph> SemanticGraphAPI::fromAST(const parser::ast::expression& ast, 
                                                        const std::string& file_path) {
    auto graph = std::make_unique<SemanticGraph>();
    ASGBuilder builder(*graph, file_path);
    builder.buildFromAST(ast);
    return graph;
}

std::vector<UsageEdge> SemanticGraphAPI::findUsage(const SemanticGraph& graph, const std::string& symbol) {
    return graph.findUsage(symbol);
}

std::vector<DataFlowPath> SemanticGraphAPI::dataFlow(const SemanticGraph& graph, const std::string& variable) {
    return graph.dataFlow(variable);
}

std::vector<NodeId> SemanticGraphAPI::controlFlow(const SemanticGraph& graph, NodeId node) {
    return graph.controlFlow(node);
}

std::unordered_set<std::string> SemanticGraphAPI::scopeAt(const SemanticGraph& graph, 
                                                         const SourceLocation& location) {
    return graph.scopeAt(location);
}

std::vector<NodeId> SemanticGraphAPI::getIncomingNodes(const SemanticGraph& graph, NodeId node_id, 
                                                      EdgeType edge_type) {
    return graph.getIncomingNodes(node_id, edge_type);
}

std::vector<NodeId> SemanticGraphAPI::getOutgoingNodes(const SemanticGraph& graph, NodeId node_id, 
                                                      EdgeType edge_type) {
    return graph.getOutgoingNodes(node_id, edge_type);
}

// Advanced query methods

std::vector<NodeId> SemanticGraphAPI::findFunctions(const SemanticGraph& graph) {
    auto function_nodes = graph.getNodesByType(NodeType::FUNCTION_DECLARATION);
    std::vector<NodeId> result;
    result.reserve(function_nodes.size());
    
    for (const auto& node : function_nodes) {
        result.push_back(node->getId());
    }
    
    return result;
}

std::vector<NodeId> SemanticGraphAPI::findVariables(const SemanticGraph& graph) {
    auto variable_nodes = graph.getNodesByType(NodeType::VARIABLE_DECLARATION);
    std::vector<NodeId> result;
    result.reserve(variable_nodes.size());
    
    for (const auto& node : variable_nodes) {
        result.push_back(node->getId());
    }
    
    return result;
}

std::vector<NodeId> SemanticGraphAPI::findTypes(const SemanticGraph& graph) {
    auto type_nodes = graph.getNodesByType(NodeType::TYPE_DECLARATION);
    std::vector<NodeId> result;
    result.reserve(type_nodes.size());
    
    for (const auto& node : type_nodes) {
        result.push_back(node->getId());
    }
    
    return result;
}

std::vector<NodeId> SemanticGraphAPI::findNodesByType(const SemanticGraph& graph, NodeType type) {
    auto nodes = graph.getNodesByType(type);
    std::vector<NodeId> result;
    result.reserve(nodes.size());
    
    for (const auto& node : nodes) {
        result.push_back(node->getId());
    }
    
    return result;
}

std::vector<EdgeId> SemanticGraphAPI::findEdgesByType(const SemanticGraph& graph, EdgeType type) {
    auto edges = graph.getEdgesByType(type);
    std::vector<EdgeId> result;
    result.reserve(edges.size());
    
    for (const auto& edge : edges) {
        result.push_back(edge->getId());
    }
    
    return result;
}

std::vector<NodeId> SemanticGraphAPI::findCallsTo(const SemanticGraph& graph, const std::string& function_name) {
    std::vector<NodeId> calls;
    
    // Get all call edges
    auto call_edges = graph.getEdgesByType(EdgeType::CALL_GRAPH);
    
    for (const auto& edge : call_edges) {
        auto call_edge = std::dynamic_pointer_cast<CallEdge>(edge);
        if (call_edge && call_edge->getFunctionName() == function_name) {
            calls.push_back(call_edge->getFromNode());
        }
    }
    
    return calls;
}

std::vector<NodeId> SemanticGraphAPI::findAssignmentsTo(const SemanticGraph& graph, const std::string& variable_name) {
    std::vector<NodeId> assignments;
    
    // Get all data flow edges
    auto data_flow_edges = graph.getEdgesByType(EdgeType::DATA_FLOW);
    
    for (const auto& edge : data_flow_edges) {
        auto df_edge = std::dynamic_pointer_cast<DataFlowEdge>(edge);
        if (df_edge && 
            df_edge->getSymbolName() == variable_name &&
            (df_edge->getFlowType() == DataFlowEdge::FlowType::DEFINITION ||
             df_edge->getFlowType() == DataFlowEdge::FlowType::MODIFICATION)) {
            assignments.push_back(df_edge->getFromNode());
        }
    }
    
    return assignments;
}

std::vector<NodeId> SemanticGraphAPI::findReturnsIn(const SemanticGraph& graph, NodeId function_node) {
    std::vector<NodeId> returns;
    
    // Find all return statements that are control-flow reachable from the function
    std::unordered_set<NodeId> visited;
    std::queue<NodeId> to_visit;
    to_visit.push(function_node);
    
    while (!to_visit.empty()) {
        NodeId current = to_visit.front();
        to_visit.pop();
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        auto node = graph.getNode(current);
        if (!node) continue;
        
        // Check if this is a return statement
        if (node->getType() == NodeType::RETURN_STATEMENT) {
            returns.push_back(current);
        }
        
        // Add control flow successors
        auto successors = graph.getOutgoingNodes(current, EdgeType::CONTROL_FLOW);
        for (NodeId successor : successors) {
            if (!visited.count(successor)) {
                to_visit.push(successor);
            }
        }
    }
    
    return returns;
}

// Graph analysis methods

bool SemanticGraphAPI::hasPath(const SemanticGraph& graph, NodeId from, NodeId to, EdgeType edge_type) {
    std::unordered_set<NodeId> visited;
    std::queue<NodeId> to_visit;
    to_visit.push(from);
    
    while (!to_visit.empty()) {
        NodeId current = to_visit.front();
        to_visit.pop();
        
        if (current == to) {
            return true;
        }
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        auto successors = graph.getOutgoingNodes(current, edge_type);
        for (NodeId successor : successors) {
            if (!visited.count(successor)) {
                to_visit.push(successor);
            }
        }
    }
    
    return false;
}

std::vector<NodeId> SemanticGraphAPI::shortestPath(const SemanticGraph& graph, NodeId from, NodeId to, 
                                                  EdgeType edge_type) {
    std::unordered_set<NodeId> visited;
    std::queue<std::pair<NodeId, std::vector<NodeId>>> to_visit;
    to_visit.push({from, {from}});
    
    while (!to_visit.empty()) {
        auto [current, path] = to_visit.front();
        to_visit.pop();
        
        if (current == to) {
            return path;
        }
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        auto successors = graph.getOutgoingNodes(current, edge_type);
        for (NodeId successor : successors) {
            if (!visited.count(successor)) {
                auto new_path = path;
                new_path.push_back(successor);
                to_visit.push({successor, new_path});
            }
        }
    }
    
    return {}; // No path found
}

std::unordered_set<NodeId> SemanticGraphAPI::reachableNodes(const SemanticGraph& graph, NodeId start, 
                                                           EdgeType edge_type) {
    std::unordered_set<NodeId> reachable;
    std::vector<NodeId> result;
    
    dfsVisit(graph, start, edge_type, reachable, result);
    
    return reachable;
}

std::vector<std::vector<NodeId>> SemanticGraphAPI::stronglyConnectedComponents(const SemanticGraph& graph, 
                                                                              EdgeType edge_type) {
    std::unordered_map<NodeId, int> indices;
    std::unordered_map<NodeId, int> lowlinks;
    std::stack<NodeId> stack;
    std::unordered_set<NodeId> on_stack;
    std::vector<std::vector<NodeId>> components;
    int index = 0;
    
    // Get all nodes
    auto all_nodes = graph.getAllNodes();
    
    for (const auto& node : all_nodes) {
        NodeId node_id = node->getId();
        if (indices.find(node_id) == indices.end()) {
            tarjanSCC(graph, node_id, edge_type, indices, lowlinks, stack, on_stack, components, index);
        }
    }
    
    return components;
}

// Export methods

std::string SemanticGraphAPI::toDOT(const SemanticGraph& graph) {
    std::ostringstream dot;
    dot << "digraph SemanticGraph {\n";
    dot << "  rankdir=TB;\n";
    dot << "  node [shape=box];\n";
    
    // Add nodes
    auto all_nodes = graph.getAllNodes();
    for (const auto& node : all_nodes) {
        dot << "  " << node->getId() << " [label=\"";
        
        // Add node type
        switch (node->getType()) {
            case NodeType::FUNCTION_DECLARATION: dot << "FUNC"; break;
            case NodeType::VARIABLE_DECLARATION: dot << "VAR"; break;
            case NodeType::TYPE_DECLARATION: dot << "TYPE"; break;
            case NodeType::EXPRESSION: dot << "EXPR"; break;
            case NodeType::STATEMENT: dot << "STMT"; break;
            case NodeType::IDENTIFIER: dot << "ID"; break;
            case NodeType::LITERAL: dot << "LIT"; break;
            case NodeType::BLOCK: dot << "BLOCK"; break;
            case NodeType::PARAMETER: dot << "PARAM"; break;
            case NodeType::RETURN_STATEMENT: dot << "RETURN"; break;
            case NodeType::FUNCTION_CALL: dot << "CALL"; break;
            case NodeType::BINARY_OPERATION: dot << "BINOP"; break;
            case NodeType::UNARY_OPERATION: dot << "UNOP"; break;
            case NodeType::ASSIGNMENT: dot << "ASSIGN"; break;
            case NodeType::CONTROL_FLOW: dot << "CTRL"; break;
            case NodeType::SCOPE_BOUNDARY: dot << "SCOPE"; break;
        }
        
        dot << "\\n" << node->getId() << "\"];\n";
    }
    
    // Add edges
    auto all_edges = graph.getAllEdges();
    for (const auto& edge : all_edges) {
        dot << "  " << edge->getFromNode() << " -> " << edge->getToNode();
        
        // Add edge styling based on type
        switch (edge->getType()) {
            case EdgeType::DATA_FLOW:
                dot << " [color=blue, label=\"data\"]";
                break;
            case EdgeType::CONTROL_FLOW:
                dot << " [color=red, label=\"ctrl\"]";
                break;
            case EdgeType::SCOPE:
                dot << " [color=green, label=\"scope\"]";
                break;
            case EdgeType::TYPE_RELATION:
                dot << " [color=purple, label=\"type\"]";
                break;
            case EdgeType::CALL_GRAPH:
                dot << " [color=orange, label=\"call\"]";
                break;
        }
        
        dot << ";\n";
    }
    
    dot << "}\n";
    return dot.str();
}

std::string SemanticGraphAPI::toJSON(const SemanticGraph& graph) {
    return graph.toJSON();
}

std::unique_ptr<SemanticGraph> SemanticGraphAPI::fromJSON(const std::string& json) {
    auto graph = std::make_unique<SemanticGraph>();
    graph->fromJSON(json);
    return graph;
}

// Validation methods

bool SemanticGraphAPI::validateGraph(const SemanticGraph& graph) {
    return graph.validateGraph();
}

std::vector<std::string> SemanticGraphAPI::checkGraphIssues(const SemanticGraph& graph) {
    std::vector<std::string> issues;
    
    // Check for orphaned nodes (nodes with no edges)
    auto all_nodes = graph.getAllNodes();
    for (const auto& node : all_nodes) {
        if (node->getIncomingEdges().empty() && node->getOutgoingEdges().empty()) {
            issues.push_back("Orphaned node: " + std::to_string(node->getId()));
        }
    }
    
    // Check for dangling edges (edges referencing non-existent nodes)
    auto all_edges = graph.getAllEdges();
    for (const auto& edge : all_edges) {
        if (!graph.getNode(edge->getFromNode())) {
            issues.push_back("Dangling edge from non-existent node: " + std::to_string(edge->getFromNode()));
        }
        if (!graph.getNode(edge->getToNode())) {
            issues.push_back("Dangling edge to non-existent node: " + std::to_string(edge->getToNode()));
        }
    }
    
    // Check for inconsistent edge references in nodes
    for (const auto& node : all_nodes) {
        for (EdgeId edge_id : node->getIncomingEdges()) {
            auto edge = graph.getEdge(edge_id);
            if (!edge || edge->getToNode() != node->getId()) {
                issues.push_back("Inconsistent incoming edge reference in node: " + std::to_string(node->getId()));
            }
        }
        
        for (EdgeId edge_id : node->getOutgoingEdges()) {
            auto edge = graph.getEdge(edge_id);
            if (!edge || edge->getFromNode() != node->getId()) {
                issues.push_back("Inconsistent outgoing edge reference in node: " + std::to_string(node->getId()));
            }
        }
    }
    
    return issues;
}

// Helper methods for graph algorithms

void SemanticGraphAPI::dfsVisit(const SemanticGraph& graph, NodeId node, EdgeType edge_type,
                               std::unordered_set<NodeId>& visited, std::vector<NodeId>& result) {
    if (visited.count(node)) return;
    
    visited.insert(node);
    result.push_back(node);
    
    auto successors = graph.getOutgoingNodes(node, edge_type);
    for (NodeId successor : successors) {
        dfsVisit(graph, successor, edge_type, visited, result);
    }
}

void SemanticGraphAPI::tarjanSCC(const SemanticGraph& graph, NodeId node, EdgeType edge_type,
                                std::unordered_map<NodeId, int>& indices,
                                std::unordered_map<NodeId, int>& lowlinks,
                                std::stack<NodeId>& stack,
                                std::unordered_set<NodeId>& on_stack,
                                std::vector<std::vector<NodeId>>& components,
                                int& index) {
    // Set the depth index for this node to the smallest unused index
    indices[node] = index;
    lowlinks[node] = index;
    index++;
    stack.push(node);
    on_stack.insert(node);
    
    // Consider successors of node
    auto successors = graph.getOutgoingNodes(node, edge_type);
    for (NodeId successor : successors) {
        if (indices.find(successor) == indices.end()) {
            // Successor has not yet been visited; recurse on it
            tarjanSCC(graph, successor, edge_type, indices, lowlinks, stack, on_stack, components, index);
            lowlinks[node] = std::min(lowlinks[node], lowlinks[successor]);
        } else if (on_stack.count(successor)) {
            // Successor is in stack and hence in the current SCC
            lowlinks[node] = std::min(lowlinks[node], indices[successor]);
        }
    }
    
    // If node is a root node, pop the stack and create an SCC
    if (lowlinks[node] == indices[node]) {
        std::vector<NodeId> component;
        NodeId w;
        do {
            w = stack.top();
            stack.pop();
            on_stack.erase(w);
            component.push_back(w);
        } while (w != node);
        
        components.push_back(component);
    }
}

} // namespace meld::api
