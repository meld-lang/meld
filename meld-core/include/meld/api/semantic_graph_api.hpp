#pragma once

#include "meld/api/semantic_graph.hpp"
#include "meld/api/asg_builder.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>

namespace meld::api {

/**
 * Public API for the Abstract Syntax Graph (ASG)
 * This replaces the Code API from structural search (Requirement 38.5)
 */
class SemanticGraphAPI {
public:
    /**
     * Parse source code into a semantic graph
     */
    static std::unique_ptr<SemanticGraph> parse(const std::string& source_code, 
                                               const std::string& file_path = "");
    
    /**
     * Parse source code from file into a semantic graph
     */
    static std::unique_ptr<SemanticGraph> parseFile(const std::string& file_path);
    
    /**
     * Create a semantic graph from an existing AST
     */
    static std::unique_ptr<SemanticGraph> fromAST(const parser::ast::expression& ast, 
                                                  const std::string& file_path = "");
    
    /**
     * Find all usage edges for a symbol (Requirement 38.6)
     * Returns all edges that represent usage of the given symbol
     */
    static std::vector<UsageEdge> findUsage(const SemanticGraph& graph, const std::string& symbol);
    
    /**
     * Trace data flow paths for a variable (Requirement 38.7)
     * Returns transitive closure of data flow relationships
     */
    static std::vector<DataFlowPath> dataFlow(const SemanticGraph& graph, const std::string& variable);
    
    /**
     * Get control flow successors for a node (Requirement 38.8)
     * Returns all nodes that can be reached via control flow edges
     */
    static std::vector<NodeId> controlFlow(const SemanticGraph& graph, NodeId node);
    
    /**
     * Find all variables in scope at a location (Requirement 38.9)
     * Returns all symbols accessible at the given source location
     */
    static std::unordered_set<std::string> scopeAt(const SemanticGraph& graph, 
                                                   const SourceLocation& location);
    
    /**
     * Enable bidirectional traversal of relationship edges (Requirement 38.9)
     */
    static std::vector<NodeId> getIncomingNodes(const SemanticGraph& graph, NodeId node_id, 
                                               EdgeType edge_type);
    static std::vector<NodeId> getOutgoingNodes(const SemanticGraph& graph, NodeId node_id, 
                                               EdgeType edge_type);
    
    /**
     * Advanced query methods
     */
    
    // Find all function declarations
    static std::vector<NodeId> findFunctions(const SemanticGraph& graph);
    
    // Find all variable declarations
    static std::vector<NodeId> findVariables(const SemanticGraph& graph);
    
    // Find all type declarations
    static std::vector<NodeId> findTypes(const SemanticGraph& graph);
    
    // Find nodes by type
    static std::vector<NodeId> findNodesByType(const SemanticGraph& graph, NodeType type);
    
    // Find edges by type
    static std::vector<EdgeId> findEdgesByType(const SemanticGraph& graph, EdgeType type);
    
    // Find all calls to a specific function
    static std::vector<NodeId> findCallsTo(const SemanticGraph& graph, const std::string& function_name);
    
    // Find all assignments to a variable
    static std::vector<NodeId> findAssignmentsTo(const SemanticGraph& graph, const std::string& variable_name);
    
    // Find all return statements in a function
    static std::vector<NodeId> findReturnsIn(const SemanticGraph& graph, NodeId function_node);
    
    // Graph analysis methods
    
    // Check if there's a path between two nodes
    static bool hasPath(const SemanticGraph& graph, NodeId from, NodeId to, EdgeType edge_type);
    
    // Find shortest path between two nodes
    static std::vector<NodeId> shortestPath(const SemanticGraph& graph, NodeId from, NodeId to, 
                                           EdgeType edge_type);
    
    // Find all reachable nodes from a starting node
    static std::unordered_set<NodeId> reachableNodes(const SemanticGraph& graph, NodeId start, 
                                                    EdgeType edge_type);
    
    // Find strongly connected components (for cycle detection)
    static std::vector<std::vector<NodeId>> stronglyConnectedComponents(const SemanticGraph& graph, 
                                                                       EdgeType edge_type);
    
    // Export methods
    
    // Export graph to DOT format for visualization
    static std::string toDOT(const SemanticGraph& graph);
    
    // Export graph to JSON
    static std::string toJSON(const SemanticGraph& graph);
    
    // Import graph from JSON
    static std::unique_ptr<SemanticGraph> fromJSON(const std::string& json);
    
    // Validation methods
    
    // Validate graph consistency
    static bool validateGraph(const SemanticGraph& graph);
    
    // Check for common graph issues
    static std::vector<std::string> checkGraphIssues(const SemanticGraph& graph);
    
private:
    SemanticGraphAPI() = delete; // Static class
    
    // Helper methods for graph algorithms
    static void dfsVisit(const SemanticGraph& graph, NodeId node, EdgeType edge_type,
                        std::unordered_set<NodeId>& visited, std::vector<NodeId>& result);
    
    static void tarjanSCC(const SemanticGraph& graph, NodeId node, EdgeType edge_type,
                         std::unordered_map<NodeId, int>& indices,
                         std::unordered_map<NodeId, int>& lowlinks,
                         std::stack<NodeId>& stack,
                         std::unordered_set<NodeId>& on_stack,
                         std::vector<std::vector<NodeId>>& components,
                         int& index);
};

/**
 * Convenience functions for common operations
 */

// Create a semantic graph from source code
inline std::unique_ptr<SemanticGraph> parseCode(const std::string& source_code, 
                                                const std::string& file_path = "") {
    return SemanticGraphAPI::parse(source_code, file_path);
}

// Create a semantic graph from a file
inline std::unique_ptr<SemanticGraph> parseFile(const std::string& file_path) {
    return SemanticGraphAPI::parseFile(file_path);
}

// Find usage of a symbol
inline std::vector<UsageEdge> findUsage(const SemanticGraph& graph, const std::string& symbol) {
    return SemanticGraphAPI::findUsage(graph, symbol);
}

// Trace data flow for a variable
inline std::vector<DataFlowPath> dataFlow(const SemanticGraph& graph, const std::string& variable) {
    return SemanticGraphAPI::dataFlow(graph, variable);
}

// Get control flow successors
inline std::vector<NodeId> controlFlow(const SemanticGraph& graph, NodeId node) {
    return SemanticGraphAPI::controlFlow(graph, node);
}

// Find variables in scope at a location
inline std::unordered_set<std::string> scopeAt(const SemanticGraph& graph, 
                                               const SourceLocation& location) {
    return SemanticGraphAPI::scopeAt(graph, location);
}

} // namespace meld::api