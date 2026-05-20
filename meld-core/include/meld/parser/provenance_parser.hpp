#pragma once

#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/provenance/provenance.hpp"
#include <string>
#include <optional>

namespace meld {
namespace parser {

// Import kernel::Value for convenience
using kernel::Value;

/**
 * ProvenanceParser - Parser wrapper that automatically attaches provenance metadata
 * 
 * This class wraps the standard Meld parser and automatically attaches
 * provenance metadata to all AST nodes during parsing, marking them as
 * Human-authored code with creation timestamps.
 */
class ProvenanceParser {
public:
    // Constructor with optional author information
    ProvenanceParser(const std::string& author_email = "", const std::string& commit_hash = "");
    
    // Parse source code and attach provenance metadata
    bool parse(const std::string& source, ast::expression& result);
    
    // Parse from file and attach provenance metadata
    bool parseFile(const std::string& filename, ast::expression& result);
    
    // Set author information for subsequent parses
    void setAuthor(const std::string& author_email, const std::string& commit_hash = "");
    
    // Get the last error message
    std::string getErrorMessage() const { return error_message_; }
    
private:
    // Recursively attach provenance to all nodes in the AST
    void attachProvenanceToAST(ast::expression& expr);
    void attachProvenanceToNode(Value& node);
    
    // Convert AST nodes to Values for metadata attachment
    Value astToValue(const ast::expression& expr);
    Value astToValue(const ast::identifier& id);
    Value astToValue(const ast::integer_literal& lit);
    Value astToValue(const ast::string_literal& lit);
    // Add more conversion functions as needed
    
    // Author information
    std::string author_email_;
    std::string commit_hash_;
    
    // Underlying parser
    Parser parser_;
    
    // Error handling
    std::string error_message_;
    
    // Provenance metadata template
    provenance::ProvenanceMetadata createMetadata() const;
};

/**
 * ParserProvenanceIntegration - Integration functions for existing parser
 * 
 * These functions can be called from the existing parser to add provenance
 * tracking without requiring a complete rewrite.
 */
namespace integration {
    
    // Initialize provenance tracking for a parse session
    void initializeProvenanceTracking(const std::string& author_email = "", 
                                    const std::string& commit_hash = "");
    
    // Attach provenance to a newly created AST node
    void attachProvenanceToNewNode(Value& node);
    
    // Mark a node as user-typed (Human origin)
    void markAsUserTyped(Value& node, const std::string& source_location = "");
    
    // Get current provenance metadata template
    provenance::ProvenanceMetadata getCurrentMetadata();
    
    // Set current author information
    void setCurrentAuthor(const std::string& author_email, const std::string& commit_hash = "");
    
} // namespace integration

} // namespace parser
} // namespace meld