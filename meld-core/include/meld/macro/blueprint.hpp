#pragma once

#include "macro.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <expected>
#include <optional>
#include <chrono>

namespace meld::macro {

// Input-output pair for examples
struct ExamplePair {
    std::string input;
    std::string output;
    std::optional<std::string> description;
    auto operator<=>(const ExamplePair&) const = default;
};

// Executable spec pair — compiler-verified action-result assertion (AI_DX Req 140)
struct SpecPair {
    std::string action;                          // Source text of the action expression
    std::string result;                          // Source text of the expected result value
    std::vector<std::string> declared_effects;   // Effects declared via @uses(...) on this spec
    
    // Verification status (set by SpecVerificationPass during --release builds)
    enum class Status { Unverified, Verified, Failed };
    Status status = Status::Unverified;
    
    // Failure details (populated when status == Failed)
    std::optional<std::string> actual_value;     // Actual value produced (on failure)
    std::optional<std::string> failure_message;  // Human-readable failure description
};

// Blueprint metadata structure (v2.0 - updated for Doc-Driven Generation)
struct BlueprintMetadata {
    std::string summary;
    std::vector<std::string> rules;  // Array of generation constraints
    std::vector<ExamplePair> examples;  // Input-output pairs (v2.0 update)
    std::vector<SpecPair> specs;         // Executable spec pairs — compiler-verified (AI_DX Req 140)
    std::vector<std::string> tags;
    std::optional<std::string> parent_blueprint;  // For inheritance
    std::map<std::string, std::string> custom_fields;
    
    // New v2.1 fields for enhanced documentation
    std::map<std::string, std::string> inputs;   // Input parameter descriptions (key-value pairs)
    std::map<std::string, std::string> outputs;  // Output descriptions (key-value pairs)
    std::vector<std::string> links;              // Array of reference links
    
    // Vector embedding for semantic search (computed at compile time)
    std::vector<float> embedding;
    
    // Unique identifier for this blueprint
    std::string id;
    
    // AST node ID for linking to implementation (v2.0 addition)
    std::string ast_node_id;
    
    // Shadow provenance tracking (v2.0 addition)
    std::string shadow_history_id;
    std::optional<std::string> conversation_id;
    
    // Source location information
    std::string file_path;
    int line_number = 0;
    int column_number = 0;
    
    // Timestamps for tracking evolution
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    
    // Note: cost and intent fields removed (deprecated in v2.0)
};

// Blueprint registry for runtime querying
class BlueprintRegistry {
public:
    static BlueprintRegistry& instance() {
        static BlueprintRegistry registry;
        return registry;
    }
    
    // Register a blueprint
    void register_blueprint(const std::string& function_name, 
                           const BlueprintMetadata& metadata);
    
    // Look up a blueprint by function name
    std::optional<BlueprintMetadata> get_blueprint(const std::string& function_name) const;
    
    // Search blueprints by semantic similarity
    std::vector<std::pair<std::string, float>> 
    search_by_similarity(const std::vector<float>& query_embedding, 
                        float threshold = 0.7f, 
                        size_t max_results = 10) const;
    
    // Search blueprints by tags
    std::vector<std::string> search_by_tags(const std::vector<std::string>& tags) const;
    
    // Search blueprints by text query
    std::vector<std::string> search_by_text(const std::string& query) const;
    
    // Get all registered blueprints
    const std::map<std::string, BlueprintMetadata>& blueprints() const {
        return blueprints_;
    }
    
    // Clear all blueprints (useful for testing)
    void clear();
    
    // Export blueprints for MCP generation
    std::string export_to_mcp_json() const;
    
    // Shadow provenance integration (v2.0)
    void link_to_shadow_history(const std::string& function_name, 
                               const std::string& shadow_history_id,
                               const std::string& conversation_id = "");
    
    // AST node linking (v2.0)
    void link_to_ast_node(const std::string& function_name, 
                         const std::string& ast_node_id);
    
    // Runtime querying (v2.0)
    std::vector<std::string> query_by_rules(const std::vector<std::string>& rule_patterns) const;
    std::vector<std::string> query_by_examples(const std::string& input_pattern) const;
    std::optional<BlueprintMetadata> get_by_ast_node_id(const std::string& ast_node_id) const;
    
    // Blueprint evolution tracking (v2.0)
    void track_blueprint_update(const std::string& function_name, 
                               const BlueprintMetadata& old_metadata,
                               const BlueprintMetadata& new_metadata);
    
    // Get blueprint history
    std::vector<BlueprintMetadata> get_blueprint_history(const std::string& function_name) const;
    
private:
    BlueprintRegistry() = default;
    
    std::map<std::string, BlueprintMetadata> blueprints_;
    std::map<std::string, std::vector<BlueprintMetadata>> blueprint_history_;  // v2.0: Track evolution
    std::map<std::string, std::string> ast_node_to_function_;  // v2.0: AST node ID -> function name
    mutable std::mutex mutex_;
    
    // Helper for computing cosine similarity
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const;
};

// Blueprint parser - extracts blueprint metadata from AST
class BlueprintParser {
public:
    // Parse blueprint annotation from function AST
    std::expected<BlueprintMetadata, std::string> 
    parse_blueprint(const kernel::Value& blueprint_ast, 
                   const std::string& function_name,
                   const std::string& file_path = "",
                   int line_number = 0) const;
    
    // Parse blueprint fields from a map-like AST structure
    std::expected<std::map<std::string, kernel::Value>, std::string>
    parse_blueprint_fields(const kernel::Value& blueprint_ast) const;
    
    // Extract string value from AST
    std::expected<std::string, std::string>
    extract_string(const kernel::Value& value) const;
    
    // Extract string array from AST
    std::expected<std::vector<std::string>, std::string>
    extract_string_array(const kernel::Value& value) const;
    
    // Extract example pairs from AST (v2.0)
    std::expected<std::vector<ExamplePair>, std::string>
    extract_example_pairs(const kernel::Value& value) const;
    
    // Extract key-value map from AST (v2.1)
    std::expected<std::map<std::string, std::string>, std::string>
    extract_key_value_map(const kernel::Value& value) const;
    
    // Extract executable spec pairs from AST (AI_DX Req 140)
    std::expected<std::vector<SpecPair>, std::string>
    extract_spec_pairs(const kernel::Value& value) const;
    
    // Validate blueprint metadata
    std::expected<void, std::string>
    validate_blueprint(const BlueprintMetadata& metadata) const;
    
private:
    // Generate unique ID for blueprint
    std::string generate_blueprint_id(const std::string& function_name, 
                                     const std::string& file_path) const;
};

// Vector embedding generator for semantic search
class EmbeddingGenerator {
public:
    // Generate embedding for blueprint text (summary + rules + examples)
    std::vector<float> generate_embedding(const BlueprintMetadata& metadata) const;
    
    // Generate embedding for arbitrary text
    std::vector<float> generate_text_embedding(const std::string& text) const;
    
    // Combine multiple text fields into a single embedding
    std::vector<float> combine_embeddings(const std::vector<std::vector<float>>& embeddings) const;
    
private:
    // Simple hash-based embedding for now (can be replaced with ML model)
    std::vector<float> hash_embedding(const std::string& text) const;
    
    // Normalize embedding vector
    void normalize_embedding(std::vector<float>& embedding) const;
};

// Blueprint inheritance resolver
class BlueprintInheritance {
public:
    // Resolve blueprint inheritance chain
    std::expected<BlueprintMetadata, std::string>
    resolve_inheritance(const BlueprintMetadata& child, 
                       const BlueprintRegistry& registry) const;
    
    // Merge parent and child blueprints
    BlueprintMetadata merge_blueprints(const BlueprintMetadata& parent,
                                      const BlueprintMetadata& child) const;
    
    // Check for circular inheritance
    bool has_circular_inheritance(const std::string& blueprint_id,
                                 const BlueprintRegistry& registry) const;
};

// IDE integration support
class BlueprintIDE {
public:
    // Generate hover information for IDE
    std::string generate_hover_info(const BlueprintMetadata& metadata) const;
    
    // Generate completion suggestions based on blueprints
    std::vector<std::string> generate_completions(const std::string& prefix,
                                                 const BlueprintRegistry& registry) const;
    
    // Generate signature help
    std::string generate_signature_help(const BlueprintMetadata& metadata) const;
    
    // Export blueprint data for LSP
    std::string export_for_lsp() const;
};

// Create the @blueprint macro
std::shared_ptr<Macro> create_blueprint_macro();

// Register blueprint-related macros
void register_blueprint_macros();

// Helper functions for blueprint processing

// Extract function name from function definition AST
std::expected<std::string, std::string>
extract_function_name(const kernel::Value& function_ast);

// Check if AST node has blueprint annotation
bool has_blueprint_annotation(const kernel::Value& function_ast);

// Extract blueprint annotation from function AST
std::expected<kernel::Value, std::string>
extract_blueprint_annotation(const kernel::Value& function_ast);

// Generate MCP tool definition from blueprint
std::string generate_mcp_tool(const std::string& function_name,
                             const BlueprintMetadata& metadata);

// Serialize spec pairs to structured JSON (AI_DX Req 140)
std::string specs_to_json(const std::vector<SpecPair>& specs,
                          const std::string& symbol_name = "");

} // namespace meld::macro