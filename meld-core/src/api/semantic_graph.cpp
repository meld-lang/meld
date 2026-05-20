#include "meld/api/semantic_graph.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace meld::api {

// SemanticGraph implementation

NodeId SemanticGraph::addNode(NodeType type, const parser::ast::expression& ast_node, 
                             const SourceLocation& location) {
    NodeId node_id = next_node_id_++;
    auto node = std::make_shared<GraphNode>(node_id, type, ast_node, location);
    
    nodes_[node_id] = node;
    nodes_by_type_[type].insert(node_id);
    
    updateIndices(node_id, node);
    
    return node_id;
}

void SemanticGraph::removeNode(NodeId node_id) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return;
    }
    
    auto node = it->second;
    
    // Remove all edges connected to this node
    auto incoming_edges = node->getIncomingEdges();
    auto outgoing_edges = node->getOutgoingEdges();
    
    for (EdgeId edge_id : incoming_edges) {
        removeEdge(edge_id);
    }
    
    for (EdgeId edge_id : outgoing_edges) {
        removeEdge(edge_id);
    }
    
    // Remove from indices
    removeFromIndices(node_id, node);
    
    // Remove from type index
    nodes_by_type_[node->getType()].erase(node_id);
    
    // Remove the node
    nodes_.erase(it);
}

std::shared_ptr<GraphNode> SemanticGraph::getNode(NodeId node_id) const {
    auto it = nodes_.find(node_id);
    return it != nodes_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<GraphNode>> SemanticGraph::getAllNodes() const {
    std::vector<std::shared_ptr<GraphNode>> result;
    result.reserve(nodes_.size());
    
    for (const auto& pair : nodes_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<GraphNode>> SemanticGraph::getNodesByType(NodeType type) const {
    std::vector<std::shared_ptr<GraphNode>> result;
    
    auto it = nodes_by_type_.find(type);
    if (it != nodes_by_type_.end()) {
        result.reserve(it->second.size());
        
        for (NodeId node_id : it->second) {
            auto node_it = nodes_.find(node_id);
            if (node_it != nodes_.end()) {
                result.push_back(node_it->second);
            }
        }
    }
    
    return result;
}

EdgeId SemanticGraph::addDataFlowEdge(NodeId from, NodeId to, DataFlowEdge::FlowType flow_type, 
                                     const std::string& symbol_name) {
    EdgeId edge_id = next_edge_id_++;
    auto edge = std::make_shared<DataFlowEdge>(edge_id, from, to, flow_type, symbol_name);
    
    edges_[edge_id] = edge;
    edges_by_type_[EdgeType::DATA_FLOW].insert(edge_id);
    
    // Update node connections
    auto from_node = getNode(from);
    auto to_node = getNode(to);
    
    if (from_node) {
        from_node->addOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->addIncomingEdge(edge_id);
    }
    
    return edge_id;
}

EdgeId SemanticGraph::addControlFlowEdge(NodeId from, NodeId to, ControlFlowEdge::FlowCondition condition) {
    EdgeId edge_id = next_edge_id_++;
    auto edge = std::make_shared<ControlFlowEdge>(edge_id, from, to, condition);
    
    edges_[edge_id] = edge;
    edges_by_type_[EdgeType::CONTROL_FLOW].insert(edge_id);
    
    // Update node connections
    auto from_node = getNode(from);
    auto to_node = getNode(to);
    
    if (from_node) {
        from_node->addOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->addIncomingEdge(edge_id);
    }
    
    return edge_id;
}

EdgeId SemanticGraph::addScopeEdge(NodeId from, NodeId to, ScopeEdge::ScopeType scope_type, 
                                  const std::string& symbol_name) {
    EdgeId edge_id = next_edge_id_++;
    auto edge = std::make_shared<ScopeEdge>(edge_id, from, to, scope_type, symbol_name);
    
    edges_[edge_id] = edge;
    edges_by_type_[EdgeType::SCOPE].insert(edge_id);
    
    // Update node connections
    auto from_node = getNode(from);
    auto to_node = getNode(to);
    
    if (from_node) {
        from_node->addOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->addIncomingEdge(edge_id);
    }
    
    return edge_id;
}

EdgeId SemanticGraph::addTypeEdge(NodeId from, NodeId to, TypeEdge::TypeRelation relation, 
                                 const std::string& type_name) {
    EdgeId edge_id = next_edge_id_++;
    auto edge = std::make_shared<TypeEdge>(edge_id, from, to, relation, type_name);
    
    edges_[edge_id] = edge;
    edges_by_type_[EdgeType::TYPE_RELATION].insert(edge_id);
    
    // Update node connections
    auto from_node = getNode(from);
    auto to_node = getNode(to);
    
    if (from_node) {
        from_node->addOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->addIncomingEdge(edge_id);
    }
    
    return edge_id;
}

EdgeId SemanticGraph::addCallEdge(NodeId from, NodeId to, CallEdge::CallType call_type, 
                                 const std::string& function_name) {
    EdgeId edge_id = next_edge_id_++;
    auto edge = std::make_shared<CallEdge>(edge_id, from, to, call_type, function_name);
    
    edges_[edge_id] = edge;
    edges_by_type_[EdgeType::CALL_GRAPH].insert(edge_id);
    
    // Update node connections
    auto from_node = getNode(from);
    auto to_node = getNode(to);
    
    if (from_node) {
        from_node->addOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->addIncomingEdge(edge_id);
    }
    
    return edge_id;
}

void SemanticGraph::removeEdge(EdgeId edge_id) {
    auto it = edges_.find(edge_id);
    if (it == edges_.end()) {
        return;
    }
    
    auto edge = it->second;
    
    // Remove from nodes
    auto from_node = getNode(edge->getFromNode());
    auto to_node = getNode(edge->getToNode());
    
    if (from_node) {
        from_node->removeOutgoingEdge(edge_id);
    }
    
    if (to_node) {
        to_node->removeIncomingEdge(edge_id);
    }
    
    // Remove from type index
    edges_by_type_[edge->getType()].erase(edge_id);
    
    // Remove the edge
    edges_.erase(it);
}

std::shared_ptr<GraphEdge> SemanticGraph::getEdge(EdgeId edge_id) const {
    auto it = edges_.find(edge_id);
    return it != edges_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<GraphEdge>> SemanticGraph::getAllEdges() const {
    std::vector<std::shared_ptr<GraphEdge>> result;
    result.reserve(edges_.size());
    
    for (const auto& pair : edges_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<GraphEdge>> SemanticGraph::getEdgesByType(EdgeType type) const {
    std::vector<std::shared_ptr<GraphEdge>> result;
    
    auto it = edges_by_type_.find(type);
    if (it != edges_by_type_.end()) {
        result.reserve(it->second.size());
        
        for (EdgeId edge_id : it->second) {
            auto edge_it = edges_.find(edge_id);
            if (edge_it != edges_.end()) {
                result.push_back(edge_it->second);
            }
        }
    }
    
    return result;
}

// Core API methods implementation

std::vector<UsageEdge> SemanticGraph::findUsage(const std::string& symbol) const {
    std::vector<UsageEdge> usages;
    
    // Get all nodes that reference this symbol
    auto symbol_nodes = index_.getSymbolReferences(symbol);
    
    for (NodeId node_id : symbol_nodes) {
        auto node = getNode(node_id);
        if (!node) continue;
        
        // Find all data flow edges involving this node and symbol
        for (EdgeId edge_id : node->getIncomingEdges()) {
            auto edge = getEdge(edge_id);
            if (!edge || edge->getType() != EdgeType::DATA_FLOW) continue;
            
            auto data_flow_edge = std::dynamic_pointer_cast<DataFlowEdge>(edge);
            if (data_flow_edge && data_flow_edge->getSymbolName() == symbol) {
                std::string context = extractContext(node->getASTNode());
                usages.emplace_back(node_id, edge_id, data_flow_edge->getFlowType(), 
                                  node->getLocation(), context);
            }
        }
        
        for (EdgeId edge_id : node->getOutgoingEdges()) {
            auto edge = getEdge(edge_id);
            if (!edge || edge->getType() != EdgeType::DATA_FLOW) continue;
            
            auto data_flow_edge = std::dynamic_pointer_cast<DataFlowEdge>(edge);
            if (data_flow_edge && data_flow_edge->getSymbolName() == symbol) {
                std::string context = extractContext(node->getASTNode());
                usages.emplace_back(node_id, edge_id, data_flow_edge->getFlowType(), 
                                  node->getLocation(), context);
            }
        }
    }
    
    return usages;
}

std::vector<DataFlowPath> SemanticGraph::dataFlow(const std::string& variable) const {
    std::vector<DataFlowPath> paths;
    
    // Find all definition nodes for this variable
    auto symbol_nodes = index_.getSymbolReferences(variable);
    
    for (NodeId node_id : symbol_nodes) {
        auto node = getNode(node_id);
        if (!node) continue;
        
        // Check if this node is a definition
        bool is_definition = false;
        for (EdgeId edge_id : node->getOutgoingEdges()) {
            auto edge = getEdge(edge_id);
            if (!edge || edge->getType() != EdgeType::DATA_FLOW) continue;
            
            auto data_flow_edge = std::dynamic_pointer_cast<DataFlowEdge>(edge);
            if (data_flow_edge && 
                data_flow_edge->getSymbolName() == variable &&
                data_flow_edge->getFlowType() == DataFlowEdge::FlowType::DEFINITION) {
                is_definition = true;
                break;
            }
        }
        
        if (is_definition) {
            DataFlowPath path(variable);
            std::unordered_set<NodeId> visited;
            collectDataFlowPath(node_id, variable, path, visited);
            
            if (!path.isEmpty()) {
                paths.push_back(std::move(path));
            }
        }
    }
    
    return paths;
}

std::vector<NodeId> SemanticGraph::controlFlow(NodeId node) const {
    std::vector<NodeId> successors;
    std::unordered_set<NodeId> visited;
    
    collectControlFlowSuccessors(node, successors, visited);
    
    return successors;
}

std::unordered_set<std::string> SemanticGraph::scopeAt(const SourceLocation& location) const {
    std::unordered_set<std::string> symbols_in_scope;
    
    // Find all nodes at or before this location
    for (const auto& pair : nodes_) {
        auto node = pair.second;
        const auto& node_location = node->getLocation();
        
        // Check if this node is in the same file and before/at the target location
        if (node_location.file_path == location.file_path &&
            (node_location.line < location.line || 
             (node_location.line == location.line && node_location.column <= location.column))) {
            
            // Check for scope edges that bring symbols into scope
            for (EdgeId edge_id : node->getOutgoingEdges()) {
                auto edge = getEdge(edge_id);
                if (!edge || edge->getType() != EdgeType::SCOPE) continue;
                
                auto scope_edge = std::dynamic_pointer_cast<ScopeEdge>(edge);
                if (scope_edge && scope_edge->getScopeType() == ScopeEdge::ScopeType::DECLARATION) {
                    symbols_in_scope.insert(scope_edge->getSymbolName());
                }
            }
        }
    }
    
    return symbols_in_scope;
}

std::vector<NodeId> SemanticGraph::getIncomingNodes(NodeId node_id, EdgeType edge_type) const {
    std::vector<NodeId> incoming_nodes;
    
    auto node = getNode(node_id);
    if (!node) return incoming_nodes;
    
    for (EdgeId edge_id : node->getIncomingEdges()) {
        auto edge = getEdge(edge_id);
        if (edge && edge->getType() == edge_type) {
            incoming_nodes.push_back(edge->getFromNode());
        }
    }
    
    return incoming_nodes;
}

std::vector<NodeId> SemanticGraph::getOutgoingNodes(NodeId node_id, EdgeType edge_type) const {
    std::vector<NodeId> outgoing_nodes;
    
    auto node = getNode(node_id);
    if (!node) return outgoing_nodes;
    
    for (EdgeId edge_id : node->getOutgoingEdges()) {
        auto edge = getEdge(edge_id);
        if (edge && edge->getType() == edge_type) {
            outgoing_nodes.push_back(edge->getToNode());
        }
    }
    
    return outgoing_nodes;
}

bool SemanticGraph::validateGraph() const {
    // Check that all edges reference valid nodes
    for (const auto& edge_pair : edges_) {
        auto edge = edge_pair.second;
        
        if (nodes_.find(edge->getFromNode()) == nodes_.end() ||
            nodes_.find(edge->getToNode()) == nodes_.end()) {
            return false;
        }
    }
    
    // Check that node edge lists are consistent
    for (const auto& node_pair : nodes_) {
        auto node = node_pair.second;
        
        for (EdgeId edge_id : node->getIncomingEdges()) {
            auto edge = getEdge(edge_id);
            if (!edge || edge->getToNode() != node->getId()) {
                return false;
            }
        }
        
        for (EdgeId edge_id : node->getOutgoingEdges()) {
            auto edge = getEdge(edge_id);
            if (!edge || edge->getFromNode() != node->getId()) {
                return false;
            }
        }
    }
    
    return true;
}

std::string SemanticGraph::toJSON() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"nodes\": [\n";
    
    bool first_node = true;
    for (const auto& pair : nodes_) {
        if (!first_node) json << ",\n";
        first_node = false;
        
        auto node = pair.second;
        json << "    {\n";
        json << "      \"id\": " << node->getId() << ",\n";
        json << "      \"type\": " << static_cast<int>(node->getType()) << ",\n";
        json << "      \"location\": {\n";
        json << "        \"file\": \"" << node->getLocation().file_path << "\",\n";
        json << "        \"line\": " << node->getLocation().line << ",\n";
        json << "        \"column\": " << node->getLocation().column << "\n";
        json << "      }\n";
        json << "    }";
    }
    
    json << "\n  ],\n";
    json << "  \"edges\": [\n";
    
    bool first_edge = true;
    for (const auto& pair : edges_) {
        if (!first_edge) json << ",\n";
        first_edge = false;
        
        auto edge = pair.second;
        json << "    {\n";
        json << "      \"id\": " << edge->getId() << ",\n";
        json << "      \"type\": " << static_cast<int>(edge->getType()) << ",\n";
        json << "      \"from\": " << edge->getFromNode() << ",\n";
        json << "      \"to\": " << edge->getToNode() << "\n";
        json << "    }";
    }
    
    json << "\n  ]\n";
    json << "}";
    
    return json.str();
}

void SemanticGraph::fromJSON(const std::string& json) {
    // TODO: Implement JSON deserialization
    throw std::runtime_error("JSON deserialization not yet implemented");
}

void SemanticGraph::clear() {
    nodes_.clear();
    edges_.clear();
    nodes_by_type_.clear();
    edges_by_type_.clear();
    index_.clear();
    next_node_id_ = 1;
    next_edge_id_ = 1;
}

// Helper methods

void SemanticGraph::collectDataFlowPath(NodeId start_node, const std::string& symbol, 
                                       DataFlowPath& path, std::unordered_set<NodeId>& visited) const {
    if (visited.count(start_node)) {
        return; // Avoid cycles
    }
    
    visited.insert(start_node);
    
    auto node = getNode(start_node);
    if (!node) return;
    
    // Add current node to path
    path.addStep(start_node, 0); // Edge will be added when we traverse
    
    // Follow data flow edges
    for (EdgeId edge_id : node->getOutgoingEdges()) {
        auto edge = getEdge(edge_id);
        if (!edge || edge->getType() != EdgeType::DATA_FLOW) continue;
        
        auto data_flow_edge = std::dynamic_pointer_cast<DataFlowEdge>(edge);
        if (data_flow_edge && data_flow_edge->getSymbolName() == symbol) {
            // Update the last edge in the path
            if (!path.edges.empty()) {
                path.edges.back() = edge_id;
            } else {
                path.edges.push_back(edge_id);
            }
            
            // Continue following the path
            collectDataFlowPath(edge->getToNode(), symbol, path, visited);
        }
    }
}

void SemanticGraph::collectControlFlowSuccessors(NodeId node, std::vector<NodeId>& successors, 
                                               std::unordered_set<NodeId>& visited) const {
    if (visited.count(node)) {
        return; // Avoid cycles
    }
    
    visited.insert(node);
    
    auto node_ptr = getNode(node);
    if (!node_ptr) return;
    
    // Follow control flow edges
    for (EdgeId edge_id : node_ptr->getOutgoingEdges()) {
        auto edge = getEdge(edge_id);
        if (!edge || edge->getType() != EdgeType::CONTROL_FLOW) continue;
        
        NodeId successor = edge->getToNode();
        successors.push_back(successor);
        
        // Recursively collect successors
        collectControlFlowSuccessors(successor, successors, visited);
    }
}

std::string SemanticGraph::extractSymbolName(const parser::ast::expression& ast_node) const {
    // Extract symbol name from AST node
    if (auto identifier = boost::get<parser::ast::identifier>(&ast_node)) {
        return identifier->name;
    }
    
    // Handle other node types that might contain symbols
    // This is a simplified implementation
    return "";
}

std::string SemanticGraph::extractContext(const parser::ast::expression& ast_node) const {
    // Extract surrounding code context for display
    // This is a simplified implementation that would need proper source reconstruction
    if (auto identifier = boost::get<parser::ast::identifier>(&ast_node)) {
        return identifier->name;
    }
    
    return ""; // TODO: Implement proper context extraction
}

void SemanticGraph::updateIndices(NodeId node_id, const std::shared_ptr<GraphNode>& node) {
    // Update location index
    index_.addLocationReference(node->getLocation().file_path, node_id);
    
    // Extract and index symbols from the AST node
    std::string symbol = extractSymbolName(node->getASTNode());
    if (!symbol.empty()) {
        index_.addSymbolReference(symbol, node_id);
    }
    
    // TODO: Add type and function indexing based on node type and AST content
}

void SemanticGraph::removeFromIndices(NodeId node_id, const std::shared_ptr<GraphNode>& node) {
    // Remove from location index
    index_.removeLocationReference(node->getLocation().file_path, node_id);
    
    // Remove from symbol index
    std::string symbol = extractSymbolName(node->getASTNode());
    if (!symbol.empty()) {
        index_.removeSymbolReference(symbol, node_id);
    }
    
    // TODO: Remove from type and function indices
}

} // namespace meld::api