#pragma once

#include "meld/parser/ast.hpp"
#include "meld/macro/blueprint.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace meld::compiler {

// MCP tool definition structure
struct MCPTool {
    std::string name;
    std::string description;
    std::map<std::string, std::string> input_schema;  // JSON Schema properties
    std::vector<std::string> required_parameters;
    std::optional<std::string> blueprint_id;
    std::string function_signature;
    
    // Convert to JSON string
    std::string to_json() const;
};

// MCP server configuration
struct MCPServerConfig {
    std::string name;
    std::string version;
    std::string description;
    std::vector<MCPTool> tools;
    std::map<std::string, std::string> metadata;
    
    // Convert to JSON string
    std::string to_json() const;
};

// MCP generation options
struct MCPGenerationOptions {
    bool include_private_functions = false;
    bool include_test_functions = false;
    bool include_blueprint_metadata = true;
    bool generate_server_config = true;
    std::string server_name = "meld-mcp-server";
    std::string server_version = "1.0.0";
    std::string output_directory = "./mcp-output";
    std::vector<std::string> include_namespaces;
    std::vector<std::string> exclude_functions;
};

// Main MCP generator class
class MCPGenerator {
public:
    MCPGenerator();
    ~MCPGenerator();
    
    // Generate MCP configuration from parsed expressions
    MCPServerConfig generate_mcp_config(
        const std::vector<parser::ast::expression>& expressions,
        const MCPGenerationOptions& options = {}
    );
    
    // Generate MCP tools from functions
    std::vector<MCPTool> generate_tools_from_functions(
        const std::vector<parser::ast::expression>& expressions,
        const MCPGenerationOptions& options = {}
    );
    
    // Generate MCP tool from a single function
    std::optional<MCPTool> generate_tool_from_function(
        const parser::ast::function_definition& function,
        const MCPGenerationOptions& options = {}
    );
    
    // Extract JSON Schema from function parameters
    std::map<std::string, std::string> generate_input_schema(
        const parser::ast::function_definition& function
    );
    
    // Generate tool description from @blueprint metadata
    std::string generate_tool_description(
        const parser::ast::function_definition& function,
        const macro::BlueprintMetadata* blueprint = nullptr
    );
    
    // Check if function should be included in MCP generation
    bool should_include_function(
        const parser::ast::function_definition& function,
        const MCPGenerationOptions& options
    );
    
    // Write MCP configuration to files
    bool write_mcp_files(
        const MCPServerConfig& config,
        const MCPGenerationOptions& options
    );
    
    // Generate incremental MCP updates
    MCPServerConfig generate_incremental_update(
        const MCPServerConfig& existing_config,
        const std::vector<parser::ast::expression>& new_expressions,
        const MCPGenerationOptions& options = {}
    );
    
    // Validate MCP configuration
    std::vector<std::string> validate_mcp_config(const MCPServerConfig& config);
    
    // Type conversion helper (public for utility function access)
    std::string type_to_json_schema_type(const std::string& meld_type);
    
private:
    // Helper methods
    std::string escape_json_string(const std::string& str);
    std::string generate_function_signature(const parser::ast::function_definition& function);
    bool is_public_function(const parser::ast::function_definition& function);
    bool is_test_function(const parser::ast::function_definition& function);
    std::string sanitize_tool_name(const std::string& name);
    
    // Blueprint integration
    std::optional<macro::BlueprintMetadata> get_blueprint_for_function(
        const parser::ast::function_definition& function
    );
    
    // File I/O helpers
    bool create_directory(const std::string& path);
    bool write_file(const std::string& path, const std::string& content);
    std::string read_existing_config(const std::string& path);
};

// Utility functions for MCP generation

// Convert Meld type annotation to JSON Schema type
std::string meld_type_to_json_schema(const std::string& meld_type);

// Generate MCP server entry point script
std::string generate_mcp_server_script(const MCPServerConfig& config);

// Generate package.json for MCP server
std::string generate_mcp_package_json(const MCPServerConfig& config);

// Generate README.md for MCP server
std::string generate_mcp_readme(const MCPServerConfig& config);

} // namespace meld::compiler