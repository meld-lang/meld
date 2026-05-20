#include "meld/compiler/mcp_generator.hpp"
#include "meld/macro/blueprint.hpp"
#include "meld/compat/visit.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <format>

namespace meld::compiler {

// MCPTool implementation
std::string MCPTool::to_json() const {
    std::ostringstream json;
    json << "    {\n";
    json << "      \"name\": \"" << name << "\",\n";
    json << "      \"description\": \"" << description << "\",\n";
    json << "      \"inputSchema\": {\n";
    json << "        \"type\": \"object\",\n";
    json << "        \"properties\": {\n";
    
    bool first_prop = true;
    for (const auto& [prop_name, prop_type] : input_schema) {
        if (!first_prop) json << ",\n";
        first_prop = false;
        json << "          \"" << prop_name << "\": {\n";
        json << "            \"type\": \"" << prop_type << "\"\n";
        json << "          }";
    }
    
    json << "\n        },\n";
    json << "        \"required\": [";
    
    bool first_req = true;
    for (const auto& req : required_parameters) {
        if (!first_req) json << ", ";
        first_req = false;
        json << "\"" << req << "\"";
    }
    
    json << "]\n";
    json << "      }";
    
    if (blueprint_id.has_value()) {
        json << ",\n      \"blueprintId\": \"" << *blueprint_id << "\"";
    }
    
    json << ",\n      \"functionSignature\": \"" << function_signature << "\"\n";
    json << "    }";
    
    return json.str();
}

// MCPServerConfig implementation
std::string MCPServerConfig::to_json() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << name << "\",\n";
    json << "  \"version\": \"" << version << "\",\n";
    json << "  \"description\": \"" << description << "\",\n";
    json << "  \"tools\": [\n";
    
    bool first_tool = true;
    for (const auto& tool : tools) {
        if (!first_tool) json << ",\n";
        first_tool = false;
        json << tool.to_json();
    }
    
    json << "\n  ]";
    
    if (!metadata.empty()) {
        json << ",\n  \"metadata\": {\n";
        bool first_meta = true;
        for (const auto& [key, value] : metadata) {
            if (!first_meta) json << ",\n";
            first_meta = false;
            json << "    \"" << key << "\": \"" << value << "\"";
        }
        json << "\n  }";
    }
    
    json << "\n}";
    return json.str();
}

// MCPGenerator implementation
MCPGenerator::MCPGenerator() = default;
MCPGenerator::~MCPGenerator() = default;

MCPServerConfig MCPGenerator::generate_mcp_config(
    const std::vector<parser::ast::expression>& expressions,
    const MCPGenerationOptions& options
) {
    MCPServerConfig config;
    config.name = options.server_name;
    config.version = options.server_version;
    config.description = std::format("Auto-generated MCP server for Meld functions");
    
    // Generate tools from functions
    config.tools = generate_tools_from_functions(expressions, options);
    
    // Add metadata
    config.metadata["generator"] = "meld-compiler";
    config.metadata["generated_at"] = std::format("{}", std::chrono::system_clock::now());
    config.metadata["total_tools"] = std::to_string(config.tools.size());
    
    return config;
}

std::vector<MCPTool> MCPGenerator::generate_tools_from_functions(
    const std::vector<parser::ast::expression>& expressions,
    const MCPGenerationOptions& options
) {
    std::vector<MCPTool> tools;
    
    for (const auto& expr : expressions) {
        meld::compat::visit([&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<parser::ast::function_definition>>) {
                if (should_include_function(*e, options)) {
                    auto tool = generate_tool_from_function(*e, options);
                    if (tool.has_value()) {
                        tools.push_back(std::move(*tool));
                    }
                }
            }
        }, expr);
    }
    
    return tools;
}

std::optional<MCPTool> MCPGenerator::generate_tool_from_function(
    const parser::ast::function_definition& function,
    const MCPGenerationOptions& options
) {
    MCPTool tool;
    
    // Set tool name (sanitized function name)
    tool.name = sanitize_tool_name(function.name.name);
    
    // Generate input schema from function parameters
    tool.input_schema = generate_input_schema(function);
    
    // Extract required parameters
    for (const auto& param : function.parameters) {
        if (!param.has_default) {
            tool.required_parameters.push_back(param.name.name);
        }
    }
    
    // Generate function signature
    tool.function_signature = generate_function_signature(function);
    
    // Get blueprint metadata if available
    auto blueprint = get_blueprint_for_function(function);
    if (blueprint.has_value()) {
        tool.description = blueprint->summary;
        tool.blueprint_id = blueprint->id;
        
        // Enhance description with rules if available
        if (!blueprint->rules.empty()) {
            tool.description += "\n\nRules:\n";
            for (const auto& rule : blueprint->rules) {
                tool.description += "- " + rule + "\n";
            }
        }
        
        // Add examples if available
        if (!blueprint->examples.empty()) {
            tool.description += "\nExamples:\n";
            for (const auto& example : blueprint->examples) {
                tool.description += "- Input: " + example.input + ", Output: " + example.output + "\n";
            }
        }
    } else {
        // Generate basic description from function signature
        tool.description = generate_tool_description(function);
    }
    
    return tool;
}

std::map<std::string, std::string> MCPGenerator::generate_input_schema(
    const parser::ast::function_definition& function
) {
    std::map<std::string, std::string> schema;
    
    for (const auto& param : function.parameters) {
        std::string json_type = type_to_json_schema_type(param.type.type_name.name);
        schema[param.name.name] = json_type;
    }
    
    return schema;
}

std::string MCPGenerator::generate_tool_description(
    const parser::ast::function_definition& function,
    const macro::BlueprintMetadata* blueprint
) {
    if (blueprint && !blueprint->summary.empty()) {
        return blueprint->summary;
    }
    
    // Generate basic description from function name and parameters
    std::ostringstream desc;
    desc << "Function: " << function.name.name;
    
    if (!function.parameters.empty()) {
        desc << "\nParameters:\n";
        for (const auto& param : function.parameters) {
            desc << "- " << param.name.name << ": " << param.type.type_name.name;
            if (param.has_default) {
                desc << " (optional)";
            }
            desc << "\n";
        }
    }
    
    if (!function.return_type.type_name.name.empty()) {
        desc << "Returns: " << function.return_type.type_name.name;
    }
    
    return desc.str();
}

bool MCPGenerator::should_include_function(
    const parser::ast::function_definition& function,
    const MCPGenerationOptions& options
) {
    // Check if function is in exclude list
    if (std::find(options.exclude_functions.begin(), options.exclude_functions.end(), 
                  function.name.name) != options.exclude_functions.end()) {
        return false;
    }
    
    // Check if function is public (if private functions are excluded)
    if (!options.include_private_functions && !is_public_function(function)) {
        return false;
    }
    
    // Check if function is a test function (if test functions are excluded)
    if (!options.include_test_functions && is_test_function(function)) {
        return false;
    }
    
    // Check namespace inclusion (if specified)
    if (!options.include_namespaces.empty()) {
        // For now, assume all functions are in global namespace
        // In a full implementation, this would check the function's namespace
        return std::find(options.include_namespaces.begin(), options.include_namespaces.end(), 
                        "global") != options.include_namespaces.end();
    }
    
    return true;
}

bool MCPGenerator::write_mcp_files(
    const MCPServerConfig& config,
    const MCPGenerationOptions& options
) {
    // Create output directory
    if (!create_directory(options.output_directory)) {
        return false;
    }
    
    // Write main MCP configuration
    std::string config_path = options.output_directory + "/mcp-config.json";
    if (!write_file(config_path, config.to_json())) {
        return false;
    }
    
    if (options.generate_server_config) {
        // Write server script
        std::string server_script = generate_mcp_server_script(config);
        std::string script_path = options.output_directory + "/server.js";
        if (!write_file(script_path, server_script)) {
            return false;
        }
        
        // Write package.json
        std::string package_json = generate_mcp_package_json(config);
        std::string package_path = options.output_directory + "/package.json";
        if (!write_file(package_path, package_json)) {
            return false;
        }
        
        // Write README.md
        std::string readme = generate_mcp_readme(config);
        std::string readme_path = options.output_directory + "/README.md";
        if (!write_file(readme_path, readme)) {
            return false;
        }
    }
    
    return true;
}

MCPServerConfig MCPGenerator::generate_incremental_update(
    const MCPServerConfig& existing_config,
    const std::vector<parser::ast::expression>& new_expressions,
    const MCPGenerationOptions& options
) {
    MCPServerConfig updated_config = existing_config;
    
    // Generate new tools
    auto new_tools = generate_tools_from_functions(new_expressions, options);
    
    // Merge with existing tools (replace if same name, add if new)
    for (const auto& new_tool : new_tools) {
        auto it = std::find_if(updated_config.tools.begin(), updated_config.tools.end(),
                              [&new_tool](const MCPTool& existing) {
                                  return existing.name == new_tool.name;
                              });
        
        if (it != updated_config.tools.end()) {
            // Replace existing tool
            *it = new_tool;
        } else {
            // Add new tool
            updated_config.tools.push_back(new_tool);
        }
    }
    
    // Update metadata
    updated_config.metadata["last_updated"] = std::format("{}", std::chrono::system_clock::now());
    updated_config.metadata["total_tools"] = std::to_string(updated_config.tools.size());
    
    return updated_config;
}

std::vector<std::string> MCPGenerator::validate_mcp_config(const MCPServerConfig& config) {
    std::vector<std::string> errors;
    
    if (config.name.empty()) {
        errors.push_back("Server name cannot be empty");
    }
    
    if (config.version.empty()) {
        errors.push_back("Server version cannot be empty");
    }
    
    if (config.tools.empty()) {
        errors.push_back("At least one tool must be defined");
    }
    
    // Validate each tool
    for (const auto& tool : config.tools) {
        if (tool.name.empty()) {
            errors.push_back("Tool name cannot be empty");
        }
        
        if (tool.description.empty()) {
            errors.push_back(std::format("Tool '{}' must have a description", tool.name));
        }
        
        // Check for duplicate tool names
        auto count = std::count_if(config.tools.begin(), config.tools.end(),
                                  [&tool](const MCPTool& other) {
                                      return other.name == tool.name;
                                  });
        if (count > 1) {
            errors.push_back(std::format("Duplicate tool name: '{}'", tool.name));
        }
    }
    
    return errors;
}

// Helper methods implementation
std::string MCPGenerator::escape_json_string(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

std::string MCPGenerator::type_to_json_schema_type(const std::string& meld_type) {
    if (meld_type == "int" || meld_type == "i32" || meld_type == "i64") {
        return "integer";
    } else if (meld_type == "float" || meld_type == "f32" || meld_type == "f64") {
        return "number";
    } else if (meld_type == "string") {
        return "string";
    } else if (meld_type == "bool") {
        return "boolean";
    } else if (meld_type.starts_with("list<") || meld_type.starts_with("array<")) {
        return "array";
    } else if (meld_type.starts_with("map<") || meld_type == "object") {
        return "object";
    } else {
        // Default to string for unknown types
        return "string";
    }
}

std::string MCPGenerator::generate_function_signature(const parser::ast::function_definition& function) {
    std::ostringstream sig;
    sig << "fnc " << function.name.name << "(";
    
    bool first = true;
    for (const auto& param : function.parameters) {
        if (!first) sig << ", ";
        first = false;
        sig << param.name.name << ": " << param.type.type_name.name;
        if (param.has_default) {
            sig << " = <default>";
        }
    }
    
    sig << ")";
    
    if (!function.return_type.type_name.name.empty()) {
        sig << " -> " << function.return_type.type_name.name;
    }
    
    return sig.str();
}

bool MCPGenerator::is_public_function(const parser::ast::function_definition& function) {
    // For now, assume all functions are public unless they start with underscore
    return !function.name.name.starts_with("_");
}

bool MCPGenerator::is_test_function(const parser::ast::function_definition& function) {
    // Check if function has test blocks or name suggests it's a test
    return function.has_tests || 
           function.name.name.starts_with("test_") || 
           function.name.name.ends_with("_test");
}

std::string MCPGenerator::sanitize_tool_name(const std::string& name) {
    // Replace invalid characters with underscores
    std::string sanitized = name;
    std::regex invalid_chars("[^a-zA-Z0-9_-]");
    sanitized = std::regex_replace(sanitized, invalid_chars, "_");
    
    // Ensure it starts with a letter or underscore
    if (!sanitized.empty() && !std::isalpha(sanitized[0]) && sanitized[0] != '_') {
        sanitized = "_" + sanitized;
    }
    
    return sanitized;
}

std::optional<macro::BlueprintMetadata> MCPGenerator::get_blueprint_for_function(
    const parser::ast::function_definition& function
) {
    // Try to get blueprint from registry
    auto& registry = macro::BlueprintRegistry::instance();
    return registry.get_blueprint(function.name.name);
}

bool MCPGenerator::create_directory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool MCPGenerator::write_file(const std::string& path, const std::string& content) {
    try {
        std::ofstream file(path);
        if (!file) return false;
        file << content;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string MCPGenerator::read_existing_config(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file) return "";
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception&) {
        return "";
    }
}

// Utility functions implementation
std::string meld_type_to_json_schema(const std::string& meld_type) {
    MCPGenerator generator;
    return generator.type_to_json_schema_type(meld_type);
}

std::string generate_mcp_server_script(const MCPServerConfig& config) {
    std::ostringstream script;
    script << "#!/usr/bin/env node\n";
    script << "// Auto-generated MCP server for " << config.name << "\n\n";
    script << "const { Server } = require('@modelcontextprotocol/sdk/server/index.js');\n";
    script << "const { StdioServerTransport } = require('@modelcontextprotocol/sdk/server/stdio.js');\n\n";
    script << "const server = new Server({\n";
    script << "  name: '" << config.name << "',\n";
    script << "  version: '" << config.version << "'\n";
    script << "}, {\n";
    script << "  capabilities: {\n";
    script << "    tools: {}\n";
    script << "  }\n";
    script << "});\n\n";
    
    // Add tool handlers
    for (const auto& tool : config.tools) {
        script << "server.setRequestHandler('tools/call', async (request) => {\n";
        script << "  if (request.params.name === '" << tool.name << "') {\n";
        script << "    // TODO: Implement " << tool.name << " handler\n";
        script << "    return {\n";
        script << "      content: [{\n";
        script << "        type: 'text',\n";
        script << "        text: 'Tool " << tool.name << " called with: ' + JSON.stringify(request.params.arguments)\n";
        script << "      }]\n";
        script << "    };\n";
        script << "  }\n";
        script << "});\n\n";
    }
    
    script << "async function main() {\n";
    script << "  const transport = new StdioServerTransport();\n";
    script << "  await server.connect(transport);\n";
    script << "}\n\n";
    script << "main().catch(console.error);\n";
    
    return script.str();
}

std::string generate_mcp_package_json(const MCPServerConfig& config) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << config.name << "\",\n";
    json << "  \"version\": \"" << config.version << "\",\n";
    json << "  \"description\": \"" << config.description << "\",\n";
    json << "  \"main\": \"server.js\",\n";
    json << "  \"scripts\": {\n";
    json << "    \"start\": \"node server.js\"\n";
    json << "  },\n";
    json << "  \"dependencies\": {\n";
    json << "    \"@modelcontextprotocol/sdk\": \"^0.4.0\"\n";
    json << "  },\n";
    json << "  \"keywords\": [\"mcp\", \"meld\", \"ai-tools\"],\n";
    json << "  \"author\": \"Meld Compiler\",\n";
    json << "  \"license\": \"MIT\"\n";
    json << "}\n";
    
    return json.str();
}

std::string generate_mcp_readme(const MCPServerConfig& config) {
    std::ostringstream readme;
    readme << "# " << config.name << "\n\n";
    readme << config.description << "\n\n";
    readme << "## Installation\n\n";
    readme << "```bash\n";
    readme << "npm install\n";
    readme << "```\n\n";
    readme << "## Usage\n\n";
    readme << "```bash\n";
    readme << "npm start\n";
    readme << "```\n\n";
    readme << "## Available Tools\n\n";
    
    for (const auto& tool : config.tools) {
        readme << "### " << tool.name << "\n\n";
        readme << tool.description << "\n\n";
        readme << "**Signature:** `" << tool.function_signature << "`\n\n";
        
        if (!tool.input_schema.empty()) {
            readme << "**Parameters:**\n";
            for (const auto& [param, type] : tool.input_schema) {
                bool is_required = std::find(tool.required_parameters.begin(), 
                                           tool.required_parameters.end(), param) != tool.required_parameters.end();
                readme << "- `" << param << "` (" << type << ")" << (is_required ? " - required" : " - optional") << "\n";
            }
            readme << "\n";
        }
    }
    
    readme << "## Generated by Meld Compiler\n\n";
    readme << "This MCP server was automatically generated from Meld source code.\n";
    
    return readme.str();
}

} // namespace meld::compiler