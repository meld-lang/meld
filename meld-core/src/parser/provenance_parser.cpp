#include "meld/parser/provenance_parser.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <chrono>
#include <fstream>
#include <sstream>

namespace meld {
namespace parser {

// Import kernel types used in this file
using kernel::Value;
using kernel::String;
using kernel::meta_set;

// Global state for parser integration
namespace {
    thread_local std::string current_author_email;
    thread_local std::string current_commit_hash;
    thread_local bool provenance_tracking_enabled = false;
}

// ProvenanceParser implementation
ProvenanceParser::ProvenanceParser(const std::string& author_email, const std::string& commit_hash)
    : author_email_(author_email), commit_hash_(commit_hash) {
}

bool ProvenanceParser::parse(const std::string& source, ast::expression& result) {
    // Parse using the underlying parser
    if (!parser_.parse_expression(source, result)) {
        error_message_ = parser_.error_message();
        return false;
    }
    
    // Attach provenance metadata to all nodes
    attachProvenanceToAST(result);
    
    return true;
}

bool ProvenanceParser::parseFile(const std::string& filename, ast::expression& result) {
    // Read file contents
    std::ifstream file(filename);
    if (!file.is_open()) {
        error_message_ = "Could not open file: " + filename;
        return false;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    // Parse the source
    return parse(source, result);
}

void ProvenanceParser::setAuthor(const std::string& author_email, const std::string& commit_hash) {
    author_email_ = author_email;
    commit_hash_ = commit_hash;
}

void ProvenanceParser::attachProvenanceToAST(ast::expression& expr) {
    // Convert AST to Value and attach provenance
    Value node = astToValue(expr);
    attachProvenanceToNode(node);
    
    // TODO: Recursively process child nodes
    // This would require visiting all variants in the AST expression
    // For now, we'll attach to the root node
}

void ProvenanceParser::attachProvenanceToNode(Value& node) {
    provenance::ProvenanceMetadata metadata = createMetadata();
    provenance::Provenance::attachProvenance(node, metadata);
}

Value ProvenanceParser::astToValue(const ast::expression& expr) {
    // For now, create a simple representation
    // In a full implementation, this would convert the entire AST to Values
    return Value(std::make_shared<String>("ast_expression"));
}

Value ProvenanceParser::astToValue(const ast::identifier& id) {
    return Value(kernel::SymbolTable::instance().intern(id.name));
}

Value ProvenanceParser::astToValue(const ast::integer_literal& lit) {
    return Value(std::make_shared<kernel::Integer>(lit.value));
}

Value ProvenanceParser::astToValue(const ast::string_literal& lit) {
    return Value(std::make_shared<String>(lit.value));
}

provenance::ProvenanceMetadata ProvenanceParser::createMetadata() const {
    if (!author_email_.empty()) {
        return provenance::ProvenanceMetadata(author_email_, commit_hash_);
    } else {
        return provenance::ProvenanceMetadata(); // Default Human origin
    }
}

// Integration functions implementation
namespace integration {

void initializeProvenanceTracking(const std::string& author_email, const std::string& commit_hash) {
    current_author_email = author_email;
    current_commit_hash = commit_hash;
    provenance_tracking_enabled = true;
}

void attachProvenanceToNewNode(Value& node) {
    if (!provenance_tracking_enabled) {
        return;
    }
    
    provenance::ProvenanceMetadata metadata = getCurrentMetadata();
    provenance::Provenance::attachProvenance(node, metadata);
}

void markAsUserTyped(Value& node, const std::string& source_location) {
    if (!provenance_tracking_enabled) {
        return;
    }
    
    provenance::ProvenanceMetadata metadata = getCurrentMetadata();
    
    // Add source location if provided
    if (!source_location.empty()) {
        // Store source location in metadata (could extend ProvenanceMetadata for this)
        meta_set(node, "source_location", Value(std::make_shared<String>(source_location)));
    }
    
    provenance::Provenance::attachProvenance(node, metadata);
}

provenance::ProvenanceMetadata getCurrentMetadata() {
    if (!current_author_email.empty()) {
        return provenance::ProvenanceMetadata(current_author_email, current_commit_hash);
    } else {
        return provenance::ProvenanceMetadata(); // Default Human origin
    }
}

void setCurrentAuthor(const std::string& author_email, const std::string& commit_hash) {
    current_author_email = author_email;
    current_commit_hash = commit_hash;
}

} // namespace integration

} // namespace parser
} // namespace meld