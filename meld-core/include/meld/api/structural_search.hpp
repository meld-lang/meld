#pragma once

#include "meld/parser/ast.hpp"
#include "meld/api/semantic_graph.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>

namespace meld::api {

// Forward declarations
class SearchableAST;
class ASTPattern;
class PatternMatch;
class SemanticSearchEngine;
class GraphBasedSearchEngine;

/**
 * Represents a variable capture in an AST pattern
 */
struct PatternVariable {
    std::string name;
    parser::ast::expression captured_value;
    
    PatternVariable(const std::string& n) : name(n) {}
    PatternVariable(const std::string& n, const parser::ast::expression& val) 
        : name(n), captured_value(val) {}
};

/**
 * Enhanced pattern match that includes ASG information
 */
class PatternMatch {
public:
    PatternMatch(const parser::ast::expression& matched_node, 
                 const std::unordered_map<std::string, parser::ast::expression>& captures)
        : matched_node_(matched_node), captures_(captures), node_id_(0) {}
    
    PatternMatch(const parser::ast::expression& matched_node, 
                 const std::unordered_map<std::string, parser::ast::expression>& captures,
                 NodeId node_id)
        : matched_node_(matched_node), captures_(captures), node_id_(node_id) {}
    
    // Get the matched AST node
    const parser::ast::expression& getMatchedNode() const { return matched_node_; }
    
    // Get captured variables
    const std::unordered_map<std::string, parser::ast::expression>& getCaptures() const { 
        return captures_; 
    }
    
    // Get a specific captured variable
    bool hasCapture(const std::string& name) const {
        return captures_.find(name) != captures_.end();
    }
    
    const parser::ast::expression& getCapture(const std::string& name) const {
        auto it = captures_.find(name);
        if (it == captures_.end()) {
            throw std::runtime_error("Capture variable '" + name + "' not found");
        }
        return it->second;
    }
    
    // ASG integration
    NodeId getNodeId() const { return node_id_; }
    bool hasNodeId() const { return node_id_ != 0; }
    
    // Replace the matched node with a new expression
    void replace(const parser::ast::expression& replacement);
    
private:
    parser::ast::expression matched_node_;
    std::unordered_map<std::string, parser::ast::expression> captures_;
    NodeId node_id_;  // Associated ASG node ID
};

/**
 * Represents an AST pattern for matching
 */
class ASTPattern {
public:
    ASTPattern(const parser::ast::expression& pattern_ast) : pattern_ast_(pattern_ast) {}
    
    // Match this pattern against an AST node
    bool matches(const parser::ast::expression& node, 
                 std::unordered_map<std::string, parser::ast::expression>& captures) const;
    
    // Get the pattern AST
    const parser::ast::expression& getPatternAST() const { return pattern_ast_; }
    
private:
    parser::ast::expression pattern_ast_;
    
    // Helper methods for matching different node types
    bool matchesIdentifier(const parser::ast::identifier& pattern, 
                          const parser::ast::expression& node,
                          std::unordered_map<std::string, parser::ast::expression>& captures) const;
    
    bool matchesFunctionCall(const parser::ast::function_call& pattern,
                            const parser::ast::expression& node,
                            std::unordered_map<std::string, parser::ast::expression>& captures) const;
    
    bool matchesBinaryOperation(const parser::ast::binary_operation& pattern,
                               const parser::ast::expression& node,
                               std::unordered_map<std::string, parser::ast::expression>& captures) const;
    
    bool isPatternVariable(const std::string& name) const;

    friend struct MatchVisitor;
};

/**
 * Enhanced searchable AST with ASG integration
 */
class SearchableAST {
public:
    SearchableAST(const parser::ast::expression& root_ast) : root_ast_(root_ast) {}
    
    SearchableAST(const parser::ast::expression& root_ast, 
                  std::shared_ptr<SemanticGraph> semantic_graph)
        : root_ast_(root_ast), semantic_graph_(semantic_graph) {}
    
    // Find all matches for a given pattern
    std::vector<PatternMatch> findAll(const ASTPattern& pattern) const;
    
    // Find the first match for a given pattern
    std::optional<PatternMatch> findFirst(const ASTPattern& pattern) const;
    
    // Replace all matches with a replacement function
    void replaceAll(const ASTPattern& pattern, 
                   std::function<parser::ast::expression(const PatternMatch&)> replacer);
    
    // Replace all matches with a fixed replacement
    void replaceAll(const ASTPattern& pattern, const parser::ast::expression& replacement);
    
    // Get the root AST
    const parser::ast::expression& getRootAST() const { return root_ast_; }
    
    // ASG integration
    std::shared_ptr<SemanticGraph> getSemanticGraph() const { return semantic_graph_; }
    bool hasSemanticGraph() const { return semantic_graph_ != nullptr; }
    
    // Convert to semantic graph if not already available
    void buildSemanticGraph(const std::string& file_path = "");
    
    // Convert back to source code
    std::string toSourceCode() const;
    
private:
    parser::ast::expression root_ast_;
    std::shared_ptr<SemanticGraph> semantic_graph_;
    
    // Helper method to recursively search for matches
    void findMatchesRecursive(const parser::ast::expression& node, 
                             const ASTPattern& pattern,
                             std::vector<PatternMatch>& matches) const;
    
    // Helper method to recursively replace matches
    void replaceMatchesRecursive(parser::ast::expression& node,
                                const ASTPattern& pattern,
                                std::function<parser::ast::expression(const PatternMatch&)> replacer);
    
    // Helper to find ASG node ID for AST node
    NodeId findNodeIdForAST(const parser::ast::expression& ast_node) const;

    friend struct ReplaceVisitor;
};

/**
 * Graph-based search engine using ASG for advanced queries
 */
class GraphBasedSearchEngine {
public:
    GraphBasedSearchEngine(std::shared_ptr<SemanticGraph> graph) : graph_(graph) {}
    
    // ASG-based queries (Requirements 8.1, 8.2, 8.3)
    
    // Find all usage edges for a symbol
    std::vector<UsageEdge> findUsage(const std::string& symbol) const;
    
    // Trace data flow paths for a variable
    std::vector<DataFlowPath> dataFlow(const std::string& variable) const;
    
    // Get control flow successors for a node
    std::vector<NodeId> controlFlow(NodeId node) const;
    
    // Find all variables in scope at a location
    std::unordered_set<std::string> scopeAt(const SourceLocation& location) const;
    
    // Advanced graph queries
    
    // Find all function calls to a specific function
    std::vector<NodeId> findCallsTo(const std::string& function_name) const;
    
    // Find all assignments to a variable
    std::vector<NodeId> findAssignmentsTo(const std::string& variable_name) const;
    
    // Find all return statements in a function
    std::vector<NodeId> findReturnsIn(NodeId function_node) const;
    
    // Find nodes by type
    std::vector<NodeId> findNodesByType(NodeType type) const;
    
    // Find edges by type
    std::vector<EdgeId> findEdgesByType(EdgeType type) const;
    
    // Graph traversal
    
    // Check if there's a path between two nodes
    bool hasPath(NodeId from, NodeId to, EdgeType edge_type) const;
    
    // Find shortest path between two nodes
    std::vector<NodeId> shortestPath(NodeId from, NodeId to, EdgeType edge_type) const;
    
    // Find all reachable nodes from a starting node
    std::unordered_set<NodeId> reachableNodes(NodeId start, EdgeType edge_type) const;
    
    // Pattern matching with graph context
    
    // Find patterns with additional graph constraints
    std::vector<PatternMatch> findPatternsWithDataFlow(const ASTPattern& pattern,
                                                       const std::string& variable) const;
    
    std::vector<PatternMatch> findPatternsInScope(const ASTPattern& pattern,
                                                  const SourceLocation& location) const;
    
    std::vector<PatternMatch> findPatternsWithControlFlow(const ASTPattern& pattern,
                                                          NodeId control_node) const;
    
    // Convert graph nodes back to pattern matches
    std::vector<PatternMatch> nodesToPatternMatches(const std::vector<NodeId>& nodes) const;
    
private:
    std::shared_ptr<SemanticGraph> graph_;
    
    // Helper to convert ASG node to AST expression
    parser::ast::expression nodeToAST(NodeId node_id) const;
};
class SemanticSearchEngine {
public:
    SemanticSearchEngine(const SearchableAST& ast) : ast_(ast) {}
    
    // Find functions by name pattern
    std::vector<PatternMatch> findFunctionsByName(const std::string& name_pattern) const;
    
    // Find variable usages
    std::vector<PatternMatch> findVariableUsages(const std::string& variable_name) const;
    
    // Find function calls with specific argument patterns
    std::vector<PatternMatch> findFunctionCallsWithArgs(const std::string& function_name,
                                                       const std::vector<ASTPattern>& arg_patterns) const;
    
    // Find all assignments to a variable
    std::vector<PatternMatch> findAssignments(const std::string& variable_name) const;
    
    // Find all return statements
    std::vector<PatternMatch> findReturnStatements() const;
    
    // Find all lambda expressions
    std::vector<PatternMatch> findLambdaExpressions() const;
    
private:
    const SearchableAST& ast_;
};

/**
 * Enhanced Code class with ASG integration
 */
class Code {
public:
    // Parse source code into a searchable AST with ASG
    static SearchableAST parse(const std::string& source_code, const std::string& file_path = "");
    
    // Parse source code from file with ASG
    static SearchableAST parseFile(const std::string& file_path);
    
    // Create an AST pattern from a pattern string
    static ASTPattern createPattern(const std::string& pattern_code);
    
    // Create semantic search engine (legacy)
    static SemanticSearchEngine createSemanticSearch(const SearchableAST& ast);
    
    // Create graph-based search engine (new ASG-based)
    static GraphBasedSearchEngine createGraphSearch(const SearchableAST& ast);
    
    // Direct ASG creation
    static std::shared_ptr<SemanticGraph> parseToGraph(const std::string& source_code, 
                                                       const std::string& file_path = "");
    
    static std::shared_ptr<SemanticGraph> parseFileToGraph(const std::string& file_path);
    
private:
    Code() = delete; // Static class
};

} // namespace meld::api

// Macro for creating AST patterns using ast`` literals
#define ast(pattern_code) meld::api::Code::createPattern(#pattern_code)