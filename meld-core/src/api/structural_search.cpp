#include "meld/api/structural_search.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include "meld/parser/parser.hpp"
#include "meld/compiler/ast_printer.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <boost/variant/get.hpp>
#include <boost/variant/apply_visitor.hpp>

namespace meld::api {

// Helper visitor for AST traversal
class ASTVisitor : public boost::static_visitor<void> {
public:
    ASTVisitor(std::function<void(const parser::ast::expression&)> callback) 
        : callback_(callback) {}
    
    template<typename T>
    void operator()(const T& node) const {
        // Recursively visit child nodes
        visitChildren(node);
    }
    
private:
    std::function<void(const parser::ast::expression&)> callback_;
    
    void visitChildren(const parser::ast::function_call& node) const {
        for (const auto& arg : node.arguments) {
            boost::apply_visitor(*this, static_cast<const parser::ast::expression&>(arg.get()));
        }
        for (const auto& named_arg : node.named_arguments) {
            boost::apply_visitor(*this, named_arg.value.get());
        }
    }
    
    void visitChildren(const parser::ast::binary_operation& node) const {
        boost::apply_visitor(*this, node.left.get());
        boost::apply_visitor(*this, node.right.get());
    }
    
    void visitChildren(const parser::ast::unary_operation& node) const {
        boost::apply_visitor(*this, node.operand.get());
    }
    
    void visitChildren(const parser::ast::block_expression& node) const {
        for (const auto& stmt : node.statements) {
            boost::apply_visitor(*this, stmt.get());
        }
    }
    
    void visitChildren(const parser::ast::lambda_expression& node) const {
        boost::apply_visitor(*this, node.body.get());
    }
    
    void visitChildren(const parser::ast::list_expression& node) const {
        for (const auto& elem : node.elements) {
            boost::apply_visitor(*this, elem.get());
        }
    }
    
    void visitChildren(const parser::ast::val_declaration& node) const {
        boost::apply_visitor(*this, node.value.get());
    }
    
    void visitChildren(const parser::ast::var_declaration& node) const {
        boost::apply_visitor(*this, node.value.get());
    }
    
    void visitChildren(const parser::ast::tuple_literal& node) const {
        for (const auto& elem : node.elements) {
            boost::apply_visitor(*this, elem.value.get());
        }
    }
    
    void visitChildren(const parser::ast::pipeline_expression& node) const {
        boost::apply_visitor(*this, node.value.get());
        boost::apply_visitor(*this, node.function.get());
    }
    
    // Default case for leaf nodes
    template<typename T>
    void visitChildren(const T&) const {
        // Leaf nodes have no children
    }
};

// PatternMatch implementation
void PatternMatch::replace(const parser::ast::expression& replacement) {
    matched_node_ = replacement;
}

// Visitor for pattern matching against AST nodes
struct MatchVisitor : public boost::static_visitor<bool> {
    const parser::ast::expression& node_;
    std::unordered_map<std::string, parser::ast::expression>& captures_;
    const ASTPattern& pattern_;
    
    MatchVisitor(const parser::ast::expression& node,
                std::unordered_map<std::string, parser::ast::expression>& captures,
                const ASTPattern& pattern)
        : node_(node), captures_(captures), pattern_(pattern) {}
    
    bool operator()(const parser::ast::identifier& pattern_id) const {
        if (pattern_.isPatternVariable(pattern_id.name)) {
            std::string var_name = pattern_id.name.substr(1);
            captures_[var_name] = node_;
            return true;
        }
        if (auto* node_id = boost::get<parser::ast::identifier>(&node_)) {
            return pattern_id.name == node_id->name;
        }
        return false;
    }
    
    bool operator()(const parser::ast::function_call& pattern_call) const {
        return pattern_.matchesFunctionCall(pattern_call, node_, captures_);
    }
    
    bool operator()(const parser::ast::binary_operation& pattern_binop) const {
        return pattern_.matchesBinaryOperation(pattern_binop, node_, captures_);
    }
    
    bool operator()(const parser::ast::integer_literal& pattern_int) const {
        if (auto* node_int = boost::get<parser::ast::integer_literal>(&node_)) {
            return pattern_int.value == node_int->value;
        }
        return false;
    }
    
    bool operator()(const parser::ast::string_literal& pattern_str) const {
        if (auto* node_str = boost::get<parser::ast::string_literal>(&node_)) {
            return pattern_str.value == node_str->value;
        }
        return false;
    }
    
    bool operator()(const parser::ast::boolean_literal& pattern_bool) const {
        if (auto* node_bool = boost::get<parser::ast::boolean_literal>(&node_)) {
            return pattern_bool.value == node_bool->value;
        }
        return false;
    }
    
    template<typename T>
    bool operator()(const T&) const {
        return false;
    }
};

// ASTPattern implementation
bool ASTPattern::matches(const parser::ast::expression& node, 
                        std::unordered_map<std::string, parser::ast::expression>& captures) const {
    return boost::apply_visitor(MatchVisitor(node, captures, *this), pattern_ast_);
}

bool ASTPattern::matchesFunctionCall(const parser::ast::function_call& pattern,
                                   const parser::ast::expression& node,
                                   std::unordered_map<std::string, parser::ast::expression>& captures) const {
    
    auto* node_call = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_call>>(&node);
    if (!node_call) return false;
    
    // Match function name
    if (pattern.function_name.name != node_call->get().function_name.name) {
        return false;
    }
    
    // Match arguments
    if (pattern.arguments.size() != node_call->get().arguments.size()) {
        return false;
    }
    
    for (size_t i = 0; i < pattern.arguments.size(); ++i) {
        ASTPattern arg_pattern(pattern.arguments[i]);
        if (!arg_pattern.matches(node_call->get().arguments[i], captures)) {
            return false;
        }
    }
    
    // Match named arguments
    if (pattern.named_arguments.size() != node_call->get().named_arguments.size()) {
        return false;
    }
    
    for (size_t i = 0; i < pattern.named_arguments.size(); ++i) {
        if (pattern.named_arguments[i].name.name != node_call->get().named_arguments[i].name.name) {
            return false;
        }
        
        ASTPattern arg_pattern(pattern.named_arguments[i].value);
        if (!arg_pattern.matches(node_call->get().named_arguments[i].value, captures)) {
            return false;
        }
    }
    
    return true;
}

bool ASTPattern::matchesBinaryOperation(const parser::ast::binary_operation& pattern,
                                      const parser::ast::expression& node,
                                      std::unordered_map<std::string, parser::ast::expression>& captures) const {
    
    auto* node_binop = boost::get<boost::spirit::x3::forward_ast<parser::ast::binary_operation>>(&node);
    if (!node_binop) return false;
    
    // Match operator
    if (pattern.op != node_binop->get().op) {
        return false;
    }
    
    // Match left and right operands
    ASTPattern left_pattern(pattern.left);
    ASTPattern right_pattern(pattern.right);
    
    return left_pattern.matches(node_binop->get().left, captures) &&
           right_pattern.matches(node_binop->get().right, captures);
}

bool ASTPattern::isPatternVariable(const std::string& name) const {
    return !name.empty() && name[0] == '$';
}

// SearchableAST implementation
void SearchableAST::buildSemanticGraph(const std::string& file_path) {
    if (!semantic_graph_) {
        semantic_graph_ = SemanticGraphAPI::fromAST(root_ast_, file_path);
    }
}

NodeId SearchableAST::findNodeIdForAST(const parser::ast::expression& ast_node) const {
    if (!semantic_graph_) {
        return 0; // No ASG available
    }
    
    // This is a simplified implementation - in practice, we'd need to maintain
    // a mapping between AST nodes and ASG node IDs during graph construction
    auto all_nodes = semantic_graph_->getAllNodes();
    for (const auto& node : all_nodes) {
        // Compare AST nodes (this is simplified - real implementation would need
        // proper AST node comparison or maintain explicit mapping)
        if (&node->getASTNode() == &ast_node) {
            return node->getId();
        }
    }
    return 0;
}

std::vector<PatternMatch> SearchableAST::findAll(const ASTPattern& pattern) const {
    std::vector<PatternMatch> matches;
    findMatchesRecursive(root_ast_, pattern, matches);
    return matches;
}

std::optional<PatternMatch> SearchableAST::findFirst(const ASTPattern& pattern) const {
    auto matches = findAll(pattern);
    if (matches.empty()) {
        return std::nullopt;
    }
    return matches[0];
}

void SearchableAST::replaceAll(const ASTPattern& pattern, 
                              std::function<parser::ast::expression(const PatternMatch&)> replacer) {
    replaceMatchesRecursive(root_ast_, pattern, replacer);
}

void SearchableAST::replaceAll(const ASTPattern& pattern, const parser::ast::expression& replacement) {
    replaceAll(pattern, [replacement](const PatternMatch&) { return replacement; });
}

std::string SearchableAST::toSourceCode() const {
    // Use the AST printer to convert back to source code
    compiler::ASTPrinter printer;
    return printer.print(root_ast_);
}

void SearchableAST::findMatchesRecursive(const parser::ast::expression& node, 
                                        const ASTPattern& pattern,
                                        std::vector<PatternMatch>& matches) const {
    
    // Try to match this node
    std::unordered_map<std::string, parser::ast::expression> captures;
    if (pattern.matches(node, captures)) {
        NodeId node_id = findNodeIdForAST(node);
        if (node_id != 0) {
            matches.emplace_back(node, captures, node_id);
        } else {
            matches.emplace_back(node, captures);
        }
    }
    
    // Recursively search child nodes
    ASTVisitor visitor([&](const parser::ast::expression& child_node) {
        if (&child_node != &node) { // Avoid infinite recursion
            findMatchesRecursive(child_node, pattern, matches);
        }
    });
    
    boost::apply_visitor(visitor, node);
}

// Visitor for replacing matched AST nodes
struct ReplaceVisitor : public boost::static_visitor<void> {
    const ASTPattern& pattern_;
    std::function<parser::ast::expression(const PatternMatch&)> replacer_;
    SearchableAST* ast_;
    
    ReplaceVisitor(const ASTPattern& pattern,
                  std::function<parser::ast::expression(const PatternMatch&)> replacer,
                  SearchableAST* ast)
        : pattern_(pattern), replacer_(replacer), ast_(ast) {}
    
    void operator()(parser::ast::function_call& node) const {
        for (auto& arg : node.arguments) {
            ast_->replaceMatchesRecursive(arg, pattern_, replacer_);
        }
        for (auto& named_arg : node.named_arguments) {
            ast_->replaceMatchesRecursive(named_arg.value, pattern_, replacer_);
        }
    }
    
    void operator()(parser::ast::binary_operation& node) const {
        ast_->replaceMatchesRecursive(node.left, pattern_, replacer_);
        ast_->replaceMatchesRecursive(node.right, pattern_, replacer_);
    }
    
    void operator()(parser::ast::unary_operation& node) const {
        ast_->replaceMatchesRecursive(node.operand, pattern_, replacer_);
    }
    
    void operator()(parser::ast::block_expression& node) const {
        for (auto& stmt : node.statements) {
            ast_->replaceMatchesRecursive(stmt, pattern_, replacer_);
        }
    }
    
    void operator()(parser::ast::lambda_expression& node) const {
        ast_->replaceMatchesRecursive(node.body, pattern_, replacer_);
    }
    
    void operator()(parser::ast::list_expression& node) const {
        for (auto& elem : node.elements) {
            ast_->replaceMatchesRecursive(elem, pattern_, replacer_);
        }
    }
    
    void operator()(parser::ast::val_declaration& node) const {
        ast_->replaceMatchesRecursive(node.value, pattern_, replacer_);
    }
    
    void operator()(parser::ast::var_declaration& node) const {
        ast_->replaceMatchesRecursive(node.value, pattern_, replacer_);
    }
    
    void operator()(parser::ast::tuple_literal& node) const {
        for (auto& elem : node.elements) {
            ast_->replaceMatchesRecursive(elem.value, pattern_, replacer_);
        }
    }
    
    void operator()(parser::ast::pipeline_expression& node) const {
        ast_->replaceMatchesRecursive(node.value, pattern_, replacer_);
        ast_->replaceMatchesRecursive(node.function, pattern_, replacer_);
    }
    
    template<typename T>
    void operator()(T&) const {
        // Leaf nodes have no children to replace
    }
};

void SearchableAST::replaceMatchesRecursive(parser::ast::expression& node,
                                           const ASTPattern& pattern,
                                           std::function<parser::ast::expression(const PatternMatch&)> replacer) {
    
    // Try to match this node
    std::unordered_map<std::string, parser::ast::expression> captures;
    if (pattern.matches(node, captures)) {
        PatternMatch match(node, captures);
        node = replacer(match);
        return; // Don't recurse into replaced node
    }
    
    boost::apply_visitor(ReplaceVisitor(pattern, replacer, this), node);
}

// SemanticSearchEngine implementation
std::vector<PatternMatch> SemanticSearchEngine::findFunctionsByName(const std::string& name_pattern) const {
    std::vector<PatternMatch> matches;
    
    // Create a pattern for function calls with the given name
    parser::ast::function_call pattern_call;
    pattern_call.function_name.name = name_pattern;
    
    ASTPattern pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::function_call>{pattern_call}}};
    return ast_.findAll(pattern);
}

std::vector<PatternMatch> SemanticSearchEngine::findVariableUsages(const std::string& variable_name) const {
    std::vector<PatternMatch> matches;
    
    // Create a pattern for identifier usage
    parser::ast::identifier pattern_id;
    pattern_id.name = variable_name;
    
    ASTPattern pattern{parser::ast::expression{pattern_id}};
    return ast_.findAll(pattern);
}

std::vector<PatternMatch> SemanticSearchEngine::findFunctionCallsWithArgs(
    const std::string& function_name,
    const std::vector<ASTPattern>& arg_patterns) const {
    
    std::vector<PatternMatch> matches;
    
    // Create a pattern for function call with specific arguments
    parser::ast::function_call pattern_call;
    pattern_call.function_name.name = function_name;
    
    // Add argument patterns
    for (const auto& arg_pattern : arg_patterns) {
        pattern_call.arguments.push_back(arg_pattern.getPatternAST());
    }
    
    ASTPattern pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::function_call>{pattern_call}}};
    return ast_.findAll(pattern);
}

std::vector<PatternMatch> SemanticSearchEngine::findAssignments(const std::string& variable_name) const {
    std::vector<PatternMatch> matches;
    
    // Look for val declarations
    parser::ast::val_declaration val_pattern;
    val_pattern.name.name = variable_name;
    
    ASTPattern val_ast_pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::val_declaration>{val_pattern}}};
    auto val_matches = ast_.findAll(val_ast_pattern);
    matches.insert(matches.end(), val_matches.begin(), val_matches.end());
    
    // Look for var declarations
    parser::ast::var_declaration var_pattern;
    var_pattern.name.name = variable_name;
    
    ASTPattern var_ast_pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::var_declaration>{var_pattern}}};
    auto var_matches = ast_.findAll(var_ast_pattern);
    matches.insert(matches.end(), var_matches.begin(), var_matches.end());
    
    return matches;
}

std::vector<PatternMatch> SemanticSearchEngine::findReturnStatements() const {
    std::vector<PatternMatch> matches;
    
    // Look for function calls to "rtn" (Meld's return keyword)
    parser::ast::function_call return_pattern;
    return_pattern.function_name.name = "rtn";
    
    ASTPattern pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::function_call>{return_pattern}}};
    return ast_.findAll(pattern);
}

std::vector<PatternMatch> SemanticSearchEngine::findLambdaExpressions() const {
    std::vector<PatternMatch> matches;
    
    // Create a pattern for lambda expressions
    parser::ast::lambda_expression lambda_pattern;
    
    ASTPattern pattern{parser::ast::expression{boost::spirit::x3::forward_ast<parser::ast::lambda_expression>{lambda_pattern}}};
    return ast_.findAll(pattern);
}

// GraphBasedSearchEngine implementation
std::vector<UsageEdge> GraphBasedSearchEngine::findUsage(const std::string& symbol) const {
    return SemanticGraphAPI::findUsage(*graph_, symbol);
}

std::vector<DataFlowPath> GraphBasedSearchEngine::dataFlow(const std::string& variable) const {
    return SemanticGraphAPI::dataFlow(*graph_, variable);
}

std::vector<NodeId> GraphBasedSearchEngine::controlFlow(NodeId node) const {
    return SemanticGraphAPI::controlFlow(*graph_, node);
}

std::unordered_set<std::string> GraphBasedSearchEngine::scopeAt(const SourceLocation& location) const {
    return SemanticGraphAPI::scopeAt(*graph_, location);
}

std::vector<NodeId> GraphBasedSearchEngine::findCallsTo(const std::string& function_name) const {
    return SemanticGraphAPI::findCallsTo(*graph_, function_name);
}

std::vector<NodeId> GraphBasedSearchEngine::findAssignmentsTo(const std::string& variable_name) const {
    return SemanticGraphAPI::findAssignmentsTo(*graph_, variable_name);
}

std::vector<NodeId> GraphBasedSearchEngine::findReturnsIn(NodeId function_node) const {
    return SemanticGraphAPI::findReturnsIn(*graph_, function_node);
}

std::vector<NodeId> GraphBasedSearchEngine::findNodesByType(NodeType type) const {
    return SemanticGraphAPI::findNodesByType(*graph_, type);
}

std::vector<EdgeId> GraphBasedSearchEngine::findEdgesByType(EdgeType type) const {
    return SemanticGraphAPI::findEdgesByType(*graph_, type);
}

bool GraphBasedSearchEngine::hasPath(NodeId from, NodeId to, EdgeType edge_type) const {
    return SemanticGraphAPI::hasPath(*graph_, from, to, edge_type);
}

std::vector<NodeId> GraphBasedSearchEngine::shortestPath(NodeId from, NodeId to, EdgeType edge_type) const {
    return SemanticGraphAPI::shortestPath(*graph_, from, to, edge_type);
}

std::unordered_set<NodeId> GraphBasedSearchEngine::reachableNodes(NodeId start, EdgeType edge_type) const {
    return SemanticGraphAPI::reachableNodes(*graph_, start, edge_type);
}

std::vector<PatternMatch> GraphBasedSearchEngine::findPatternsWithDataFlow(
    const ASTPattern& pattern, const std::string& variable) const {
    
    // Find all data flow paths for the variable
    auto data_flows = dataFlow(variable);
    
    std::vector<PatternMatch> matches;
    
    // For each data flow path, check if any nodes match the pattern
    for (const auto& flow : data_flows) {
        for (NodeId node_id : flow.nodes) {
            auto node = graph_->getNode(node_id);
            if (node) {
                std::unordered_map<std::string, parser::ast::expression> captures;
                if (pattern.matches(node->getASTNode(), captures)) {
                    matches.emplace_back(node->getASTNode(), captures, node_id);
                }
            }
        }
    }
    
    return matches;
}

std::vector<PatternMatch> GraphBasedSearchEngine::findPatternsInScope(
    const ASTPattern& pattern, const SourceLocation& location) const {
    
    // Find all variables in scope at the location
    auto variables = scopeAt(location);
    
    std::vector<PatternMatch> matches;
    
    // For each variable in scope, find its declaration and check pattern
    for (const std::string& var : variables) {
        auto symbol_refs = graph_->getIndex().getSymbolReferences(var);
        for (NodeId node_id : symbol_refs) {
            auto node = graph_->getNode(node_id);
            if (node) {
                std::unordered_map<std::string, parser::ast::expression> captures;
                if (pattern.matches(node->getASTNode(), captures)) {
                    matches.emplace_back(node->getASTNode(), captures, node_id);
                }
            }
        }
    }
    
    return matches;
}

std::vector<PatternMatch> GraphBasedSearchEngine::findPatternsWithControlFlow(
    const ASTPattern& pattern, NodeId control_node) const {
    
    // Find all control flow successors
    auto successors = controlFlow(control_node);
    
    std::vector<PatternMatch> matches;
    
    // Check pattern against each successor node
    for (NodeId node_id : successors) {
        auto node = graph_->getNode(node_id);
        if (node) {
            std::unordered_map<std::string, parser::ast::expression> captures;
            if (pattern.matches(node->getASTNode(), captures)) {
                matches.emplace_back(node->getASTNode(), captures, node_id);
            }
        }
    }
    
    return matches;
}

std::vector<PatternMatch> GraphBasedSearchEngine::nodesToPatternMatches(const std::vector<NodeId>& nodes) const {
    std::vector<PatternMatch> matches;
    
    for (NodeId node_id : nodes) {
        auto node = graph_->getNode(node_id);
        if (node) {
            std::unordered_map<std::string, parser::ast::expression> empty_captures;
            matches.emplace_back(node->getASTNode(), empty_captures, node_id);
        }
    }
    
    return matches;
}

parser::ast::expression GraphBasedSearchEngine::nodeToAST(NodeId node_id) const {
    auto node = graph_->getNode(node_id);
    if (node) {
        return node->getASTNode();
    }
    
    // Return a default expression if node not found
    parser::ast::identifier default_id;
    default_id.name = "unknown";
    return parser::ast::expression{default_id};
}

// Code implementation (updated)
SearchableAST Code::parse(const std::string& source_code, const std::string& file_path) {
    // Parse to AST first
    parser::Parser parser;
    parser::ast::expression result;
    
    if (!parser.parse_expression(source_code, result)) {
        throw std::runtime_error("Parse error: " + parser.error_message());
    }
    
    // Build semantic graph
    auto semantic_graph = std::shared_ptr<SemanticGraph>(SemanticGraphAPI::fromAST(result, file_path));
    
    return SearchableAST(result, semantic_graph);
}

SearchableAST Code::parseFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parse(buffer.str(), file_path);
}

ASTPattern Code::createPattern(const std::string& pattern_code) {
    // Parse the pattern code as a regular expression
    parser::Parser parser;
    parser::ast::expression pattern_ast;
    
    if (!parser.parse_expression(pattern_code, pattern_ast)) {
        throw std::runtime_error("Pattern parse error: " + parser.error_message());
    }
    
    return ASTPattern(pattern_ast);
}

SemanticSearchEngine Code::createSemanticSearch(const SearchableAST& ast) {
    return SemanticSearchEngine(ast);
}

GraphBasedSearchEngine Code::createGraphSearch(const SearchableAST& ast) {
    if (!ast.hasSemanticGraph()) {
        throw std::runtime_error("SearchableAST does not have a semantic graph. Use Code::parse() to create ASG.");
    }
    return GraphBasedSearchEngine(ast.getSemanticGraph());
}

std::shared_ptr<SemanticGraph> Code::parseToGraph(const std::string& source_code, const std::string& file_path) {
    return SemanticGraphAPI::parse(source_code, file_path);
}

std::shared_ptr<SemanticGraph> Code::parseFileToGraph(const std::string& file_path) {
    return SemanticGraphAPI::parseFile(file_path);
}

} // namespace meld::api