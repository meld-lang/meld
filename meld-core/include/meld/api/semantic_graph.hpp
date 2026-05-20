#pragma once

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>

namespace meld::api {

// Forward declarations
class SemanticGraph;
class GraphNode;
class GraphEdge;
class DataFlowEdge;
class ControlFlowEdge;
class ScopeEdge;
class TypeEdge;
class CallEdge;

/**
 * Unique identifier for graph nodes
 */
using NodeId = size_t;

/**
 * Unique identifier for graph edges
 */
using EdgeId = size_t;

/**
 * Location information for AST nodes
 */
struct SourceLocation {
    std::string file_path;
    size_t line;
    size_t column;
    size_t offset;
    
    SourceLocation() : line(0), column(0), offset(0) {}
    SourceLocation(const std::string& path, size_t l, size_t c, size_t o)
        : file_path(path), line(l), column(c), offset(o) {}
};

/**
 * Types of graph nodes representing different language constructs
 */
enum class NodeType {
    FUNCTION_DECLARATION,
    VARIABLE_DECLARATION,
    TYPE_DECLARATION,
    EXPRESSION,
    STATEMENT,
    IDENTIFIER,
    LITERAL,
    BLOCK,
    PARAMETER,
    RETURN_STATEMENT,
    FUNCTION_CALL,
    BINARY_OPERATION,
    UNARY_OPERATION,
    ASSIGNMENT,
    CONTROL_FLOW,
    SCOPE_BOUNDARY
};

/**
 * Types of graph edges representing different relationships
 */
enum class EdgeType {
    DATA_FLOW,      // Variable definition to usage
    CONTROL_FLOW,   // Execution path between statements
    SCOPE,          // Declaration to its lexical scope
    TYPE_RELATION,  // Value to its type
    CALL_GRAPH      // Function call to function definition
};

/**
 * Base class for all graph nodes
 */
class GraphNode {
public:
    GraphNode(NodeId id, NodeType type, const parser::ast::expression& ast_node, 
              const SourceLocation& location)
        : id_(id), type_(type), ast_node_(ast_node), location_(location) {}
    
    virtual ~GraphNode() = default;
    
    NodeId getId() const { return id_; }
    NodeType getType() const { return type_; }
    const parser::ast::expression& getASTNode() const { return ast_node_; }
    const SourceLocation& getLocation() const { return location_; }
    
    // Edge management
    void addIncomingEdge(EdgeId edge_id) { incoming_edges_.insert(edge_id); }
    void addOutgoingEdge(EdgeId edge_id) { outgoing_edges_.insert(edge_id); }
    void removeIncomingEdge(EdgeId edge_id) { incoming_edges_.erase(edge_id); }
    void removeOutgoingEdge(EdgeId edge_id) { outgoing_edges_.erase(edge_id); }
    
    const std::unordered_set<EdgeId>& getIncomingEdges() const { return incoming_edges_; }
    const std::unordered_set<EdgeId>& getOutgoingEdges() const { return outgoing_edges_; }
    
    // Metadata
    void setMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    
    std::optional<std::string> getMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? std::optional<std::string>(it->second) : std::nullopt;
    }
    
protected:
    NodeId id_;
    NodeType type_;
    parser::ast::expression ast_node_;
    SourceLocation location_;
    std::unordered_set<EdgeId> incoming_edges_;
    std::unordered_set<EdgeId> outgoing_edges_;
    std::unordered_map<std::string, std::string> metadata_;
};

/**
 * Base class for all graph edges
 */
class GraphEdge {
public:
    GraphEdge(EdgeId id, EdgeType type, NodeId from, NodeId to)
        : id_(id), type_(type), from_node_(from), to_node_(to) {}
    
    virtual ~GraphEdge() = default;
    
    EdgeId getId() const { return id_; }
    EdgeType getType() const { return type_; }
    NodeId getFromNode() const { return from_node_; }
    NodeId getToNode() const { return to_node_; }
    
    // Metadata
    void setMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    
    std::optional<std::string> getMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? std::optional<std::string>(it->second) : std::nullopt;
    }
    
protected:
    EdgeId id_;
    EdgeType type_;
    NodeId from_node_;
    NodeId to_node_;
    std::unordered_map<std::string, std::string> metadata_;
};

/**
 * Data flow edge connecting variable definitions to usage sites
 */
class DataFlowEdge : public GraphEdge {
public:
    enum class FlowType {
        DEFINITION,     // Variable definition
        USE,           // Variable usage
        MODIFICATION   // Variable modification
    };
    
    DataFlowEdge(EdgeId id, NodeId from, NodeId to, FlowType flow_type, 
                 const std::string& symbol_name)
        : GraphEdge(id, EdgeType::DATA_FLOW, from, to), 
          flow_type_(flow_type), symbol_name_(symbol_name) {}
    
    FlowType getFlowType() const { return flow_type_; }
    const std::string& getSymbolName() const { return symbol_name_; }
    
private:
    FlowType flow_type_;
    std::string symbol_name_;
};

/**
 * Control flow edge representing execution paths through the program
 */
class ControlFlowEdge : public GraphEdge {
public:
    enum class FlowCondition {
        UNCONDITIONAL,  // Always taken
        TRUE_BRANCH,    // Taken when condition is true
        FALSE_BRANCH,   // Taken when condition is false
        EXCEPTION,      // Taken on exception
        RETURN,         // Function return
        BREAK,          // Loop break
        CONTINUE        // Loop continue
    };
    
    ControlFlowEdge(EdgeId id, NodeId from, NodeId to, FlowCondition condition)
        : GraphEdge(id, EdgeType::CONTROL_FLOW, from, to), condition_(condition) {}
    
    FlowCondition getCondition() const { return condition_; }
    
private:
    FlowCondition condition_;
};

/**
 * Scope edge linking declarations to their lexical scopes
 */
class ScopeEdge : public GraphEdge {
public:
    enum class ScopeType {
        DECLARATION,    // Declaration defines a new binding in scope
        ACCESS,         // Access to a binding from outer scope
        CLOSURE         // Closure captures binding from outer scope
    };
    
    ScopeEdge(EdgeId id, NodeId from, NodeId to, ScopeType scope_type, 
              const std::string& symbol_name)
        : GraphEdge(id, EdgeType::SCOPE, from, to), 
          scope_type_(scope_type), symbol_name_(symbol_name) {}
    
    ScopeType getScopeType() const { return scope_type_; }
    const std::string& getSymbolName() const { return symbol_name_; }
    
private:
    ScopeType scope_type_;
    std::string symbol_name_;
};

/**
 * Type edge linking values to their types
 */
class TypeEdge : public GraphEdge {
public:
    enum class TypeRelation {
        INSTANCE_OF,    // Value is instance of type
        SUBTYPE_OF,     // Type is subtype of another type
        GENERIC_PARAM,  // Generic type parameter relationship
        TYPE_ALIAS      // Type alias relationship
    };
    
    TypeEdge(EdgeId id, NodeId from, NodeId to, TypeRelation relation, 
             const std::string& type_name)
        : GraphEdge(id, EdgeType::TYPE_RELATION, from, to), 
          relation_(relation), type_name_(type_name) {}
    
    TypeRelation getRelation() const { return relation_; }
    const std::string& getTypeName() const { return type_name_; }
    
private:
    TypeRelation relation_;
    std::string type_name_;
};

/**
 * Call edge for function invocations
 */
class CallEdge : public GraphEdge {
public:
    enum class CallType {
        DIRECT_CALL,    // Direct function call
        VIRTUAL_CALL,   // Virtual method call
        LAMBDA_CALL,    // Lambda invocation
        CONSTRUCTOR,    // Constructor call
        OPERATOR        // Operator call
    };
    
    CallEdge(EdgeId id, NodeId from, NodeId to, CallType call_type, 
             const std::string& function_name)
        : GraphEdge(id, EdgeType::CALL_GRAPH, from, to), 
          call_type_(call_type), function_name_(function_name) {}
    
    CallType getCallType() const { return call_type_; }
    const std::string& getFunctionName() const { return function_name_; }
    
private:
    CallType call_type_;
    std::string function_name_;
};

/**
 * Index structures for fast symbol and type lookup
 */
class GraphIndex {
public:
    // Symbol index: symbol name -> set of node IDs
    void addSymbolReference(const std::string& symbol, NodeId node_id) {
        symbol_index_[symbol].insert(node_id);
    }
    
    void removeSymbolReference(const std::string& symbol, NodeId node_id) {
        auto it = symbol_index_.find(symbol);
        if (it != symbol_index_.end()) {
            it->second.erase(node_id);
            if (it->second.empty()) {
                symbol_index_.erase(it);
            }
        }
    }
    
    std::unordered_set<NodeId> getSymbolReferences(const std::string& symbol) const {
        auto it = symbol_index_.find(symbol);
        return it != symbol_index_.end() ? it->second : std::unordered_set<NodeId>();
    }
    
    // Type index: type name -> set of node IDs
    void addTypeReference(const std::string& type_name, NodeId node_id) {
        type_index_[type_name].insert(node_id);
    }
    
    void removeTypeReference(const std::string& type_name, NodeId node_id) {
        auto it = type_index_.find(type_name);
        if (it != type_index_.end()) {
            it->second.erase(node_id);
            if (it->second.empty()) {
                type_index_.erase(it);
            }
        }
    }
    
    std::unordered_set<NodeId> getTypeReferences(const std::string& type_name) const {
        auto it = type_index_.find(type_name);
        return it != type_index_.end() ? it->second : std::unordered_set<NodeId>();
    }
    
    // Location index: file path -> set of node IDs
    void addLocationReference(const std::string& file_path, NodeId node_id) {
        location_index_[file_path].insert(node_id);
    }
    
    void removeLocationReference(const std::string& file_path, NodeId node_id) {
        auto it = location_index_.find(file_path);
        if (it != location_index_.end()) {
            it->second.erase(node_id);
            if (it->second.empty()) {
                location_index_.erase(it);
            }
        }
    }
    
    std::unordered_set<NodeId> getLocationReferences(const std::string& file_path) const {
        auto it = location_index_.find(file_path);
        return it != location_index_.end() ? it->second : std::unordered_set<NodeId>();
    }
    
    // Function index: function name -> set of node IDs
    void addFunctionReference(const std::string& function_name, NodeId node_id) {
        function_index_[function_name].insert(node_id);
    }
    
    void removeFunctionReference(const std::string& function_name, NodeId node_id) {
        auto it = function_index_.find(function_name);
        if (it != function_index_.end()) {
            it->second.erase(node_id);
            if (it->second.empty()) {
                function_index_.erase(it);
            }
        }
    }
    
    std::unordered_set<NodeId> getFunctionReferences(const std::string& function_name) const {
        auto it = function_index_.find(function_name);
        return it != function_index_.end() ? it->second : std::unordered_set<NodeId>();
    }
    
    // Clear all indices
    void clear() {
        symbol_index_.clear();
        type_index_.clear();
        location_index_.clear();
        function_index_.clear();
    }
    
private:
    std::unordered_map<std::string, std::unordered_set<NodeId>> symbol_index_;
    std::unordered_map<std::string, std::unordered_set<NodeId>> type_index_;
    std::unordered_map<std::string, std::unordered_set<NodeId>> location_index_;
    std::unordered_map<std::string, std::unordered_set<NodeId>> function_index_;
};

/**
 * Data flow path representing a sequence of data flow edges
 */
struct DataFlowPath {
    std::vector<NodeId> nodes;
    std::vector<EdgeId> edges;
    std::string symbol_name;
    
    DataFlowPath(const std::string& symbol) : symbol_name(symbol) {}
    
    void addStep(NodeId node, EdgeId edge) {
        nodes.push_back(node);
        edges.push_back(edge);
    }
    
    bool isEmpty() const { return nodes.empty(); }
    size_t length() const { return nodes.size(); }
};

/**
 * Usage edge representing a specific usage of a symbol
 */
struct UsageEdge {
    NodeId node_id;
    EdgeId edge_id;
    DataFlowEdge::FlowType usage_type;
    SourceLocation location;
    std::string context;  // Surrounding code context
    
    UsageEdge(NodeId node, EdgeId edge, DataFlowEdge::FlowType type, 
              const SourceLocation& loc, const std::string& ctx)
        : node_id(node), edge_id(edge), usage_type(type), location(loc), context(ctx) {}
};

/**
 * Main SemanticGraph class providing the ASG API
 */
class SemanticGraph {
public:
    SemanticGraph() : next_node_id_(1), next_edge_id_(1) {}
    
    // Node management
    NodeId addNode(NodeType type, const parser::ast::expression& ast_node, 
                   const SourceLocation& location);
    
    void removeNode(NodeId node_id);
    
    std::shared_ptr<GraphNode> getNode(NodeId node_id) const;
    
    std::vector<std::shared_ptr<GraphNode>> getAllNodes() const;
    
    std::vector<std::shared_ptr<GraphNode>> getNodesByType(NodeType type) const;
    
    // Edge management
    EdgeId addDataFlowEdge(NodeId from, NodeId to, DataFlowEdge::FlowType flow_type, 
                          const std::string& symbol_name);
    
    EdgeId addControlFlowEdge(NodeId from, NodeId to, ControlFlowEdge::FlowCondition condition);
    
    EdgeId addScopeEdge(NodeId from, NodeId to, ScopeEdge::ScopeType scope_type, 
                       const std::string& symbol_name);
    
    EdgeId addTypeEdge(NodeId from, NodeId to, TypeEdge::TypeRelation relation, 
                      const std::string& type_name);
    
    EdgeId addCallEdge(NodeId from, NodeId to, CallEdge::CallType call_type, 
                      const std::string& function_name);
    
    void removeEdge(EdgeId edge_id);
    
    std::shared_ptr<GraphEdge> getEdge(EdgeId edge_id) const;
    
    std::vector<std::shared_ptr<GraphEdge>> getAllEdges() const;
    
    std::vector<std::shared_ptr<GraphEdge>> getEdgesByType(EdgeType type) const;
    
    // Core API methods (Requirements 38.6, 38.7, 38.8, 38.9)
    
    /**
     * Find all usage edges for a symbol (Requirement 38.6)
     * Returns all edges that represent usage of the given symbol
     */
    std::vector<UsageEdge> findUsage(const std::string& symbol) const;
    
    /**
     * Trace data flow paths for a variable (Requirement 38.7)
     * Returns transitive closure of data flow relationships
     */
    std::vector<DataFlowPath> dataFlow(const std::string& variable) const;
    
    /**
     * Get control flow successors for a node (Requirement 38.8)
     * Returns all nodes that can be reached via control flow edges
     */
    std::vector<NodeId> controlFlow(NodeId node) const;
    
    /**
     * Find all variables in scope at a location (Requirement 38.9)
     * Returns all symbols accessible at the given source location
     */
    std::unordered_set<std::string> scopeAt(const SourceLocation& location) const;
    
    /**
     * Enable bidirectional traversal of relationship edges (Requirement 38.9)
     */
    std::vector<NodeId> getIncomingNodes(NodeId node_id, EdgeType edge_type) const;
    std::vector<NodeId> getOutgoingNodes(NodeId node_id, EdgeType edge_type) const;
    
    // Index access
    const GraphIndex& getIndex() const { return index_; }
    
    // Graph statistics
    size_t getNodeCount() const { return nodes_.size(); }
    size_t getEdgeCount() const { return edges_.size(); }
    
    // Graph validation
    bool validateGraph() const;
    
    // Serialization support
    std::string toJSON() const;
    void fromJSON(const std::string& json);
    
    // Clear the graph
    void clear();
    
private:
    // Storage
    std::unordered_map<NodeId, std::shared_ptr<GraphNode>> nodes_;
    std::unordered_map<EdgeId, std::shared_ptr<GraphEdge>> edges_;
    
    // Indices for fast lookup
    GraphIndex index_;
    
    // ID generators
    NodeId next_node_id_;
    EdgeId next_edge_id_;
    
    // Type-specific node maps for fast lookup
    std::unordered_map<NodeType, std::unordered_set<NodeId>> nodes_by_type_;
    std::unordered_map<EdgeType, std::unordered_set<EdgeId>> edges_by_type_;
    
    // Helper methods for graph traversal
    void collectDataFlowPath(NodeId start_node, const std::string& symbol, 
                           DataFlowPath& path, std::unordered_set<NodeId>& visited) const;
    
    void collectControlFlowSuccessors(NodeId node, std::vector<NodeId>& successors, 
                                    std::unordered_set<NodeId>& visited) const;
    
    std::string extractSymbolName(const parser::ast::expression& ast_node) const;
    std::string extractContext(const parser::ast::expression& ast_node) const;
    
    // Index maintenance
    void updateIndices(NodeId node_id, const std::shared_ptr<GraphNode>& node);
    void removeFromIndices(NodeId node_id, const std::shared_ptr<GraphNode>& node);
};

} // namespace meld::api