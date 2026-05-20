#include "meld/macro/blueprint.hpp"
#include "meld/kernel/primitives.hpp"
#include <format>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>
#include <regex>
#include <set>
#include <mutex>

namespace meld::macro {

// BlueprintRegistry implementation
void BlueprintRegistry::register_blueprint(const std::string& function_name, 
                                          const BlueprintMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Track history if blueprint already exists (v2.0)
    if (auto it = blueprints_.find(function_name); it != blueprints_.end()) {
        blueprint_history_[function_name].push_back(it->second);
    }
    
    // Set timestamps
    auto now = std::chrono::system_clock::now();
    auto updated_metadata = metadata;
    updated_metadata.updated_at = now;
    if (updated_metadata.created_at == std::chrono::system_clock::time_point{}) {
        updated_metadata.created_at = now;
    }
    
    blueprints_[function_name] = updated_metadata;
    
    // Update AST node mapping if provided (v2.0)
    if (!metadata.ast_node_id.empty()) {
        ast_node_to_function_[metadata.ast_node_id] = function_name;
    }
}

std::optional<BlueprintMetadata> 
BlueprintRegistry::get_blueprint(const std::string& function_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blueprints_.find(function_name);
    if (it != blueprints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, float>> 
BlueprintRegistry::search_by_similarity(const std::vector<float>& query_embedding, 
                                       float threshold, 
                                       size_t max_results) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, float>> results;
    
    for (const auto& [name, metadata] : blueprints_) {
        if (!metadata.embedding.empty()) {
            float similarity = cosine_similarity(query_embedding, metadata.embedding);
            if (similarity >= threshold) {
                results.emplace_back(name, similarity);
            }
        }
    }
    
    // Sort by similarity (descending)
    std::sort(results.begin(), results.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit results
    if (results.size() > max_results) {
        results.resize(max_results);
    }
    
    return results;
}

std::vector<std::string> 
BlueprintRegistry::search_by_tags(const std::vector<std::string>& tags) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    for (const auto& [name, metadata] : blueprints_) {
        // Check if any of the query tags match blueprint tags
        for (const auto& query_tag : tags) {
            if (std::find(metadata.tags.begin(), metadata.tags.end(), query_tag) != metadata.tags.end()) {
                results.push_back(name);
                break;
            }
        }
    }
    
    return results;
}

std::vector<std::string> 
BlueprintRegistry::search_by_text(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    for (const auto& [name, metadata] : blueprints_) {
        // Search in summary
        std::string lower_summary = metadata.summary;
        std::transform(lower_summary.begin(), lower_summary.end(), lower_summary.begin(), ::tolower);
        
        if (lower_summary.find(lower_query) != std::string::npos) {
            results.push_back(name);
            continue;
        }
        
        // Search in rules
        for (const auto& rule : metadata.rules) {
            std::string lower_rule = rule;
            std::transform(lower_rule.begin(), lower_rule.end(), lower_rule.begin(), ::tolower);
            if (lower_rule.find(lower_query) != std::string::npos) {
                results.push_back(name);
                break;
            }
        }
        
        // Search in examples
        for (const auto& example : metadata.examples) {
            std::string lower_example = example.input + " " + example.output;
            std::transform(lower_example.begin(), lower_example.end(), lower_example.begin(), ::tolower);
            if (lower_example.find(lower_query) != std::string::npos) {
                results.push_back(name);
                break;
            }
        }
        
        // Search in tags
        for (const auto& tag : metadata.tags) {
            std::string lower_tag = tag;
            std::transform(lower_tag.begin(), lower_tag.end(), lower_tag.begin(), ::tolower);
            if (lower_tag.find(lower_query) != std::string::npos) {
                results.push_back(name);
                break;
            }
        }
    }
    
    return results;
}

void BlueprintRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    blueprints_.clear();
    blueprint_history_.clear();
    ast_node_to_function_.clear();
}

std::string BlueprintRegistry::export_to_mcp_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"tools\": [\n";
    
    bool first = true;
    for (const auto& [name, metadata] : blueprints_) {
        if (!first) json << ",\n";
        first = false;
        
        json << "    {\n";
        json << "      \"name\": \"" << name << "\",\n";
        json << "      \"description\": \"" << metadata.summary << "\",\n";
        
        // Include specs if present (AI_DX Req 140)
        if (!metadata.specs.empty()) {
            json << "      \"specs\": " << specs_to_json(metadata.specs, name) << ",\n";
        }
        
        json << "      \"inputSchema\": {\n";
        json << "        \"type\": \"object\",\n";
        json << "        \"properties\": {},\n";
        json << "        \"required\": []\n";
        json << "      }\n";
        json << "    }";
    }
    
    json << "\n  ]\n";
    json << "}";
    
    return json.str();
}

// Shadow provenance integration (v2.0)
void BlueprintRegistry::link_to_shadow_history(const std::string& function_name, 
                                              const std::string& shadow_history_id,
                                              const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = blueprints_.find(function_name); it != blueprints_.end()) {
        it->second.shadow_history_id = shadow_history_id;
        if (!conversation_id.empty()) {
            it->second.conversation_id = conversation_id;
        }
        it->second.updated_at = std::chrono::system_clock::now();
    }
}

// AST node linking (v2.0)
void BlueprintRegistry::link_to_ast_node(const std::string& function_name, 
                                        const std::string& ast_node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = blueprints_.find(function_name); it != blueprints_.end()) {
        it->second.ast_node_id = ast_node_id;
        ast_node_to_function_[ast_node_id] = function_name;
        it->second.updated_at = std::chrono::system_clock::now();
    }
}

// Runtime querying (v2.0)
std::vector<std::string> BlueprintRegistry::query_by_rules(const std::vector<std::string>& rule_patterns) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    for (const auto& [name, metadata] : blueprints_) {
        for (const auto& pattern : rule_patterns) {
            for (const auto& rule : metadata.rules) {
                if (rule.find(pattern) != std::string::npos) {
                    results.push_back(name);
                    goto next_blueprint; // Found match, move to next blueprint
                }
            }
        }
        next_blueprint:;
    }
    
    return results;
}

std::vector<std::string> BlueprintRegistry::query_by_examples(const std::string& input_pattern) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    for (const auto& [name, metadata] : blueprints_) {
        for (const auto& example : metadata.examples) {
            if (example.input.find(input_pattern) != std::string::npos) {
                results.push_back(name);
                break; // Found match, move to next blueprint
            }
        }
    }
    
    return results;
}

std::optional<BlueprintMetadata> BlueprintRegistry::get_by_ast_node_id(const std::string& ast_node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = ast_node_to_function_.find(ast_node_id); it != ast_node_to_function_.end()) {
        return get_blueprint(it->second);
    }
    return std::nullopt;
}

// Blueprint evolution tracking (v2.0)
void BlueprintRegistry::track_blueprint_update(const std::string& function_name, 
                                              const BlueprintMetadata& old_metadata,
                                              const BlueprintMetadata& new_metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    blueprint_history_[function_name].push_back(old_metadata);
    
    auto updated_metadata = new_metadata;
    updated_metadata.updated_at = std::chrono::system_clock::now();
    blueprints_[function_name] = updated_metadata;
}

// Get blueprint history
std::vector<BlueprintMetadata> BlueprintRegistry::get_blueprint_history(const std::string& function_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = blueprint_history_.find(function_name); it != blueprint_history_.end()) {
        return it->second;
    }
    return {};
}

float BlueprintRegistry::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 0.0f;
    }
    
    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

// BlueprintParser implementation
std::expected<BlueprintMetadata, std::string> 
BlueprintParser::parse_blueprint(const kernel::Value& blueprint_ast, 
                                const std::string& function_name,
                                const std::string& file_path,
                                int line_number) const {
    BlueprintMetadata metadata;
    metadata.id = generate_blueprint_id(function_name, file_path);
    metadata.file_path = file_path;
    metadata.line_number = line_number;
    
    // Parse blueprint fields
    auto fields_result = parse_blueprint_fields(blueprint_ast);
    if (!fields_result) {
        return std::unexpected(fields_result.error());
    }
    
    auto fields = *fields_result;
    
    // Extract summary (required)
    if (auto it = fields.find("summary"); it != fields.end()) {
        auto summary_result = extract_string(it->second);
        if (!summary_result) {
            return std::unexpected("Invalid summary field: " + summary_result.error());
        }
        metadata.summary = *summary_result;
    } else {
        return std::unexpected("Blueprint missing required 'summary' field");
    }
    
    // Extract rules (optional)
    if (auto it = fields.find("rules"); it != fields.end()) {
        auto rules_result = extract_string_array(it->second);
        if (!rules_result) {
            return std::unexpected("Invalid rules field: " + rules_result.error());
        }
        metadata.rules = *rules_result;
    }
    
    // Extract examples (optional) - now supports input-output pairs (v2.0)
    if (auto it = fields.find("examples"); it != fields.end()) {
        auto examples_result = extract_example_pairs(it->second);
        if (!examples_result) {
            return std::unexpected("Invalid examples field: " + examples_result.error());
        }
        metadata.examples = *examples_result;
    }
    
    // Extract spec pairs (optional) - executable specs (AI_DX Req 140)
    if (auto it = fields.find("spec"); it != fields.end()) {
        auto specs_result = extract_spec_pairs(it->second);
        if (!specs_result) {
            return std::unexpected("Invalid spec field: " + specs_result.error());
        }
        metadata.specs = *specs_result;
    }
    
    // Extract tags (optional)
    if (auto it = fields.find("tags"); it != fields.end()) {
        auto tags_result = extract_string_array(it->second);
        if (!tags_result) {
            return std::unexpected("Invalid tags field: " + tags_result.error());
        }
        metadata.tags = *tags_result;
    }
    
    // Extract inputs (optional) - v2.1 addition
    if (auto it = fields.find("inputs"); it != fields.end()) {
        auto inputs_result = extract_key_value_map(it->second);
        if (!inputs_result) {
            return std::unexpected("Invalid inputs field: " + inputs_result.error());
        }
        metadata.inputs = *inputs_result;
    }
    
    // Extract outputs (optional) - v2.1 addition
    if (auto it = fields.find("outputs"); it != fields.end()) {
        auto outputs_result = extract_key_value_map(it->second);
        if (!outputs_result) {
            return std::unexpected("Invalid outputs field: " + outputs_result.error());
        }
        metadata.outputs = *outputs_result;
    }
    
    // Extract links (optional) - v2.1 addition
    if (auto it = fields.find("links"); it != fields.end()) {
        auto links_result = extract_string_array(it->second);
        if (!links_result) {
            return std::unexpected("Invalid links field: " + links_result.error());
        }
        metadata.links = *links_result;
    }
    
    // Note: cost and intent fields removed (deprecated in v2.0)
    
    // Extract parent blueprint for inheritance (optional)
    if (auto it = fields.find("extends"); it != fields.end()) {
        auto parent_result = extract_string(it->second);
        if (!parent_result) {
            return std::unexpected("Invalid extends field: " + parent_result.error());
        }
        metadata.parent_blueprint = *parent_result;
    }
    
    // Extract custom fields (excluding standard and deprecated fields)
    for (const auto& [key, value] : fields) {
        if (key != "summary" && key != "rules" && key != "examples" && 
            key != "spec" &&  // AI_DX: executable spec pairs
            key != "tags" && key != "extends" && 
            key != "inputs" && key != "outputs" && key != "links" &&  // v2.1 fields
            key != "cost" && key != "intent") {  // Exclude deprecated fields
            auto custom_value = extract_string(value);
            if (custom_value) {
                metadata.custom_fields[key] = *custom_value;
            }
        }
    }
    
    // Validate the metadata
    auto validation_result = validate_blueprint(metadata);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }
    
    // Generate embedding
    EmbeddingGenerator generator;
    metadata.embedding = generator.generate_embedding(metadata);
    
    return metadata;
}

std::expected<std::map<std::string, kernel::Value>, std::string>
BlueprintParser::parse_blueprint_fields(const kernel::Value& blueprint_ast) const {
    std::map<std::string, kernel::Value> fields;
    
    // Expect a map-like structure: { key: value, ... }
    if (!blueprint_ast.is<kernel::Cons>()) {
        return std::unexpected("Blueprint must be a map structure");
    }
    
    // Parse key-value pairs from the AST
    // This is a simplified parser - in a real implementation, this would
    // integrate with the full Meld parser
    auto current = blueprint_ast.as<kernel::Cons>();
    while (current && !current->is_nil()) {
        // Expect each element to be a key-value pair
        if (!current->head().is<kernel::Cons>()) {
            return std::unexpected("Invalid blueprint field format");
        }
        
        auto pair = current->head().as<kernel::Cons>();
        if (!pair || pair->is_nil()) {
            return std::unexpected("Empty blueprint field");
        }
        
        // Extract key (should be a symbol)
        if (!pair->head().is<kernel::Symbol>()) {
            return std::unexpected("Blueprint field key must be a symbol");
        }
        
        auto key_symbol = pair->head().as<kernel::Symbol>();
        std::string key = key_symbol->name();
        
        // Extract value
        if (!pair->tail().is<kernel::Cons>() || pair->tail().as<kernel::Cons>()->is_nil()) {
            return std::unexpected("Blueprint field missing value");
        }
        
        auto value_cons = pair->tail().as<kernel::Cons>();
        kernel::Value value = value_cons->head();
        
        fields[key] = value;
        
        // Move to next field
        if (!current->tail().is<kernel::Cons>()) {
            break;
        }
        current = current->tail().as<kernel::Cons>();
    }
    
    return fields;
}

std::expected<std::string, std::string>
BlueprintParser::extract_string(const kernel::Value& value) const {
    if (value.is<kernel::String>()) {
        return value.as<kernel::String>()->value();
    } else if (value.is<kernel::Symbol>()) {
        return value.as<kernel::Symbol>()->name();
    } else {
        return std::unexpected("Expected string value");
    }
}

std::expected<std::vector<std::string>, std::string>
BlueprintParser::extract_string_array(const kernel::Value& value) const {
    std::vector<std::string> result;
    
    if (!value.is<kernel::Cons>()) {
        return std::unexpected("Expected array value");
    }
    
    auto current = value.as<kernel::Cons>();
    while (current && !current->is_nil()) {
        auto string_result = extract_string(current->head());
        if (!string_result) {
            return std::unexpected("Array contains non-string value: " + string_result.error());
        }
        result.push_back(*string_result);
        
        if (!current->tail().is<kernel::Cons>()) {
            break;
        }
        current = current->tail().as<kernel::Cons>();
    }
    
    return result;
}

std::expected<std::vector<ExamplePair>, std::string>
BlueprintParser::extract_example_pairs(const kernel::Value& value) const {
    std::vector<ExamplePair> result;
    
    if (!value.is<kernel::Cons>()) {
        return std::unexpected("Expected array value for examples");
    }
    
    auto current = value.as<kernel::Cons>();
    while (current && !current->is_nil()) {
        // Each example can be either a string (legacy format) or an object with input/output
        if (current->head().is<kernel::String>() || current->head().is<kernel::Symbol>()) {
            // Legacy format: treat as simple string
            auto string_result = extract_string(current->head());
            if (!string_result) {
                return std::unexpected("Array contains invalid example: " + string_result.error());
            }
            
            ExamplePair pair;
            pair.input = *string_result;
            pair.output = "";  // No output specified in legacy format
            result.push_back(pair);
        } else if (current->head().is<kernel::Cons>()) {
            // New format: object with input/output fields
            auto example_obj = current->head().as<kernel::Cons>();
            auto fields_result = parse_blueprint_fields(current->head());
            if (!fields_result) {
                return std::unexpected("Invalid example object: " + fields_result.error());
            }
            
            auto fields = *fields_result;
            ExamplePair pair;
            
            // Extract input (required)
            if (auto it = fields.find("input"); it != fields.end()) {
                auto input_result = extract_string(it->second);
                if (!input_result) {
                    return std::unexpected("Invalid input field in example: " + input_result.error());
                }
                pair.input = *input_result;
            } else {
                return std::unexpected("Example missing required 'input' field");
            }
            
            // Extract output (required)
            if (auto it = fields.find("output"); it != fields.end()) {
                auto output_result = extract_string(it->second);
                if (!output_result) {
                    return std::unexpected("Invalid output field in example: " + output_result.error());
                }
                pair.output = *output_result;
            } else {
                return std::unexpected("Example missing required 'output' field");
            }
            
            // Extract description (optional)
            if (auto it = fields.find("description"); it != fields.end()) {
                auto desc_result = extract_string(it->second);
                if (desc_result) {
                    pair.description = *desc_result;
                }
            }
            
            result.push_back(pair);
        } else {
            return std::unexpected("Example must be either a string or an object with input/output fields");
        }
        
        if (!current->tail().is<kernel::Cons>()) {
            break;
        }
        current = current->tail().as<kernel::Cons>();
    }
    
    return result;
}

std::expected<std::map<std::string, std::string>, std::string>
BlueprintParser::extract_key_value_map(const kernel::Value& value) const {
    std::map<std::string, std::string> result;
    
    if (!value.is<kernel::Cons>()) {
        return std::unexpected("Expected object value for key-value map");
    }
    
    auto current = value.as<kernel::Cons>();
    while (current && !current->is_nil()) {
        // Each element should be a key-value pair
        if (!current->head().is<kernel::Cons>()) {
            return std::unexpected("Key-value map element must be a pair");
        }
        
        auto pair = current->head().as<kernel::Cons>();
        if (!pair || pair->is_nil()) {
            return std::unexpected("Empty key-value pair");
        }
        
        // Extract key (should be a symbol or string)
        auto key_result = extract_string(pair->head());
        if (!key_result) {
            return std::unexpected("Key-value pair key must be a string: " + key_result.error());
        }
        std::string key = *key_result;
        
        // Extract value
        if (!pair->tail().is<kernel::Cons>() || pair->tail().as<kernel::Cons>()->is_nil()) {
            return std::unexpected("Key-value pair missing value");
        }
        
        auto value_cons = pair->tail().as<kernel::Cons>();
        auto value_result = extract_string(value_cons->head());
        if (!value_result) {
            return std::unexpected("Key-value pair value must be a string: " + value_result.error());
        }
        
        result[key] = *value_result;
        
        // Move to next pair
        if (!current->tail().is<kernel::Cons>()) {
            break;
        }
        current = current->tail().as<kernel::Cons>();
    }
    
    return result;
}

std::expected<std::vector<SpecPair>, std::string>
BlueprintParser::extract_spec_pairs(const kernel::Value& value) const {
    std::vector<SpecPair> result;
    
    if (!value.is<kernel::Cons>()) {
        return std::unexpected("Expected array value for spec pairs");
    }
    
    auto current = value.as<kernel::Cons>();
    while (current && !current->is_nil()) {
        // Each spec must be an object with action and result fields
        if (!current->head().is<kernel::Cons>()) {
            return std::unexpected("Spec pair must be an object with action/result fields");
        }
        
        auto fields_result = parse_blueprint_fields(current->head());
        if (!fields_result) {
            return std::unexpected("Invalid spec pair: " + fields_result.error());
        }
        
        auto fields = *fields_result;
        SpecPair pair;
        
        // Extract action (required)
        if (auto it = fields.find("action"); it != fields.end()) {
            auto action_result = extract_string(it->second);
            if (!action_result) {
                return std::unexpected("Invalid action field in spec: " + action_result.error());
            }
            pair.action = *action_result;
        } else {
            return std::unexpected("Spec pair missing required 'action' field");
        }
        
        // Extract result (required)
        if (auto it = fields.find("result"); it != fields.end()) {
            auto result_val = extract_string(it->second);
            if (!result_val) {
                return std::unexpected("Invalid result field in spec: " + result_val.error());
            }
            pair.result = *result_val;
        } else {
            return std::unexpected("Spec pair missing required 'result' field");
        }
        
        // Extract declared effects (optional) — @uses(...) on this spec
        if (auto it = fields.find("uses"); it != fields.end()) {
            auto effects_result = extract_string_array(it->second);
            if (!effects_result) {
                return std::unexpected("Invalid uses field in spec: " + effects_result.error());
            }
            pair.declared_effects = *effects_result;
        }
        
        // Status defaults to Unverified — set by SpecVerificationPass during --release
        pair.status = SpecPair::Status::Unverified;
        
        result.push_back(pair);
        
        if (!current->tail().is<kernel::Cons>()) {
            break;
        }
        current = current->tail().as<kernel::Cons>();
    }
    
    return result;
}

std::expected<void, std::string>
BlueprintParser::validate_blueprint(const BlueprintMetadata& metadata) const {
    if (metadata.summary.empty()) {
        return std::unexpected("Blueprint summary cannot be empty");
    }
    
    if (metadata.summary.length() > 500) {
        return std::unexpected("Blueprint summary too long (max 500 characters)");
    }
    
    // Validate rules
    for (const auto& rule : metadata.rules) {
        if (rule.empty()) {
            return std::unexpected("Blueprint rule cannot be empty");
        }
        if (rule.length() > 200) {
            return std::unexpected("Blueprint rule too long (max 200 characters)");
        }
    }
    
    // Validate examples
    for (const auto& example : metadata.examples) {
        if (example.input.empty()) {
            return std::unexpected("Blueprint example input cannot be empty");
        }
        if (example.input.length() > 300) {
            return std::unexpected("Blueprint example input too long (max 300 characters)");
        }
        if (example.output.length() > 300) {
            return std::unexpected("Blueprint example output too long (max 300 characters)");
        }
        if (example.description && example.description->length() > 200) {
            return std::unexpected("Blueprint example description too long (max 200 characters)");
        }
    }
    
    // Validate spec pairs (AI_DX Req 140)
    for (const auto& spec : metadata.specs) {
        if (spec.action.empty()) {
            return std::unexpected("Blueprint spec action cannot be empty");
        }
        if (spec.result.empty()) {
            return std::unexpected("Blueprint spec result cannot be empty");
        }
        if (spec.action.length() > 500) {
            return std::unexpected("Blueprint spec action too long (max 500 characters)");
        }
        if (spec.result.length() > 500) {
            return std::unexpected("Blueprint spec result too long (max 500 characters)");
        }
    }
    
    // Validate inputs
    for (const auto& [key, value] : metadata.inputs) {
        if (key.empty()) {
            return std::unexpected("Blueprint input key cannot be empty");
        }
        if (value.empty()) {
            return std::unexpected("Blueprint input description cannot be empty");
        }
        if (key.length() > 50) {
            return std::unexpected("Blueprint input key too long (max 50 characters)");
        }
        if (value.length() > 200) {
            return std::unexpected("Blueprint input description too long (max 200 characters)");
        }
    }
    
    // Validate outputs
    for (const auto& [key, value] : metadata.outputs) {
        if (key.empty()) {
            return std::unexpected("Blueprint output key cannot be empty");
        }
        if (value.empty()) {
            return std::unexpected("Blueprint output description cannot be empty");
        }
        if (key.length() > 50) {
            return std::unexpected("Blueprint output key too long (max 50 characters)");
        }
        if (value.length() > 200) {
            return std::unexpected("Blueprint output description too long (max 200 characters)");
        }
    }
    
    // Validate links
    for (const auto& link : metadata.links) {
        if (link.empty()) {
            return std::unexpected("Blueprint link cannot be empty");
        }
        if (link.length() > 500) {
            return std::unexpected("Blueprint link too long (max 500 characters)");
        }
    }
    
    return {};
}

std::string BlueprintParser::generate_blueprint_id(const std::string& function_name, 
                                                   const std::string& file_path) const {
    // Generate a unique ID based on function name and file path
    std::hash<std::string> hasher;
    auto hash_value = hasher(function_name + "|" + file_path);
    return std::format("blueprint_{:x}", hash_value);
}

// EmbeddingGenerator implementation
std::vector<float> EmbeddingGenerator::generate_embedding(const BlueprintMetadata& metadata) const {
    // Combine all text fields
    std::ostringstream combined_text;
    combined_text << metadata.summary;
    
    for (const auto& rule : metadata.rules) {
        combined_text << " " << rule;
    }
    
    for (const auto& example : metadata.examples) {
        combined_text << " " << example.input;
        if (!example.output.empty()) {
            combined_text << " " << example.output;
        }
        if (example.description) {
            combined_text << " " << *example.description;
        }
    }
    
    for (const auto& tag : metadata.tags) {
        combined_text << " " << tag;
    }
    
    // Include spec action descriptions in embedding (AI_DX Req 140)
    for (const auto& spec : metadata.specs) {
        combined_text << " " << spec.action;
        combined_text << " " << spec.result;
        for (const auto& effect : spec.declared_effects) {
            combined_text << " " << effect;
        }
    }
    
    // Include new v2.1 fields in embedding
    for (const auto& [key, value] : metadata.inputs) {
        combined_text << " " << key << " " << value;
    }
    
    for (const auto& [key, value] : metadata.outputs) {
        combined_text << " " << key << " " << value;
    }
    
    for (const auto& link : metadata.links) {
        combined_text << " " << link;
    }
    
    return generate_text_embedding(combined_text.str());
}

std::vector<float> EmbeddingGenerator::generate_text_embedding(const std::string& text) const {
    // Simple hash-based embedding for now
    // In a real implementation, this would use a proper ML model
    return hash_embedding(text);
}

std::vector<float> EmbeddingGenerator::combine_embeddings(const std::vector<std::vector<float>>& embeddings) const {
    if (embeddings.empty()) {
        return {};
    }
    
    size_t dim = embeddings[0].size();
    std::vector<float> result(dim, 0.0f);
    
    for (const auto& embedding : embeddings) {
        if (embedding.size() != dim) {
            continue; // Skip mismatched dimensions
        }
        
        for (size_t i = 0; i < dim; ++i) {
            result[i] += embedding[i];
        }
    }
    
    // Average the embeddings
    float count = static_cast<float>(embeddings.size());
    for (float& val : result) {
        val /= count;
    }
    
    normalize_embedding(result);
    return result;
}

std::vector<float> EmbeddingGenerator::hash_embedding(const std::string& text) const {
    // Simple hash-based embedding (128 dimensions)
    constexpr size_t EMBEDDING_DIM = 128;
    std::vector<float> embedding(EMBEDDING_DIM, 0.0f);
    
    // Use multiple hash functions to create different dimensions
    std::hash<std::string> hasher;
    
    for (size_t i = 0; i < EMBEDDING_DIM; ++i) {
        std::string modified_text = text + std::to_string(i);
        auto hash_val = hasher(modified_text);
        
        // Convert hash to float in range [-1, 1]
        embedding[i] = static_cast<float>(static_cast<int32_t>(hash_val)) / 
                      static_cast<float>(std::numeric_limits<int32_t>::max());
    }
    
    normalize_embedding(embedding);
    return embedding;
}

void EmbeddingGenerator::normalize_embedding(std::vector<float>& embedding) const {
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    
    if (norm > 0.0f) {
        norm = std::sqrt(norm);
        for (float& val : embedding) {
            val /= norm;
        }
    }
}

// BlueprintInheritance implementation
std::expected<BlueprintMetadata, std::string>
BlueprintInheritance::resolve_inheritance(const BlueprintMetadata& child, 
                                         const BlueprintRegistry& registry) const {
    if (!child.parent_blueprint) {
        return child; // No inheritance
    }
    
    // Check for circular inheritance
    if (has_circular_inheritance(child.id, registry)) {
        return std::unexpected("Circular blueprint inheritance detected");
    }
    
    // Get parent blueprint
    auto parent_opt = registry.get_blueprint(*child.parent_blueprint);
    if (!parent_opt) {
        return std::unexpected(std::format("Parent blueprint '{}' not found", *child.parent_blueprint));
    }
    
    // Recursively resolve parent's inheritance
    auto resolved_parent_result = resolve_inheritance(*parent_opt, registry);
    if (!resolved_parent_result) {
        return std::unexpected(resolved_parent_result.error());
    }
    
    // Merge parent and child
    return merge_blueprints(*resolved_parent_result, child);
}

BlueprintMetadata BlueprintInheritance::merge_blueprints(const BlueprintMetadata& parent,
                                                        const BlueprintMetadata& child) const {
    BlueprintMetadata merged = child;
    
    // Child summary overrides parent (if present)
    if (merged.summary.empty() && !parent.summary.empty()) {
        merged.summary = parent.summary;
    }
    
    // Merge rules (parent rules first, then child rules)
    std::vector<std::string> merged_rules = parent.rules;
    merged_rules.insert(merged_rules.end(), child.rules.begin(), child.rules.end());
    merged.rules = merged_rules;
    
    // Merge examples (parent examples first, then child examples)
    std::vector<ExamplePair> merged_examples = parent.examples;
    merged_examples.insert(merged_examples.end(), child.examples.begin(), child.examples.end());
    merged.examples = merged_examples;
    
    // Merge spec pairs (parent specs first, then child specs) — AI_DX Req 140
    std::vector<SpecPair> merged_specs = parent.specs;
    merged_specs.insert(merged_specs.end(), child.specs.begin(), child.specs.end());
    merged.specs = merged_specs;
    
    // Merge tags (union of parent and child tags)
    std::vector<std::string> merged_tags = parent.tags;
    for (const auto& tag : child.tags) {
        if (std::find(merged_tags.begin(), merged_tags.end(), tag) == merged_tags.end()) {
            merged_tags.push_back(tag);
        }
    }
    merged.tags = merged_tags;
    
    // Merge inputs (child overrides parent for same keys)
    for (const auto& [key, value] : parent.inputs) {
        if (merged.inputs.find(key) == merged.inputs.end()) {
            merged.inputs[key] = value;
        }
    }
    
    // Merge outputs (child overrides parent for same keys)
    for (const auto& [key, value] : parent.outputs) {
        if (merged.outputs.find(key) == merged.outputs.end()) {
            merged.outputs[key] = value;
        }
    }
    
    // Merge links (parent links first, then child links, avoiding duplicates)
    std::vector<std::string> merged_links = parent.links;
    for (const auto& link : child.links) {
        if (std::find(merged_links.begin(), merged_links.end(), link) == merged_links.end()) {
            merged_links.push_back(link);
        }
    }
    merged.links = merged_links;
    
    // Note: cost field removed (deprecated in v2.0)
    
    // Merge custom fields (child overrides parent)
    for (const auto& [key, value] : parent.custom_fields) {
        if (merged.custom_fields.find(key) == merged.custom_fields.end()) {
            merged.custom_fields[key] = value;
        }
    }
    
    // Regenerate embedding for merged blueprint
    EmbeddingGenerator generator;
    merged.embedding = generator.generate_embedding(merged);
    
    return merged;
}

bool BlueprintInheritance::has_circular_inheritance(const std::string& blueprint_id,
                                                   const BlueprintRegistry& registry) const {
    std::set<std::string> visited;
    std::string current_id = blueprint_id;
    
    while (!current_id.empty()) {
        if (visited.find(current_id) != visited.end()) {
            return true; // Circular dependency found
        }
        
        visited.insert(current_id);
        
        // Find blueprint with this ID
        std::string parent_name;
        for (const auto& [name, metadata] : registry.blueprints()) {
            if (metadata.id == current_id) {
                if (metadata.parent_blueprint) {
                    // Find parent ID
                    for (const auto& [parent_name_candidate, parent_metadata] : registry.blueprints()) {
                        if (parent_name_candidate == *metadata.parent_blueprint) {
                            current_id = parent_metadata.id;
                            break;
                        }
                    }
                } else {
                    current_id.clear(); // No parent
                }
                break;
            }
        }
    }
    
    return false;
}

// BlueprintIDE implementation
std::string BlueprintIDE::generate_hover_info(const BlueprintMetadata& metadata) const {
    std::ostringstream info;
    
    info << "**Blueprint: " << metadata.summary << "**\n\n";
    
    if (!metadata.rules.empty()) {
        info << "**Rules:**\n";
        for (const auto& rule : metadata.rules) {
            info << "- " << rule << "\n";
        }
        info << "\n";
    }
    
    if (!metadata.examples.empty()) {
        info << "**Examples:**\n";
        for (const auto& example : metadata.examples) {
            if (!example.output.empty()) {
                info << "- Input: `" << example.input << "`\n";
                info << "  Output: `" << example.output << "`\n";
                if (example.description) {
                    info << "  " << *example.description << "\n";
                }
            } else {
                // Legacy format
                info << "- `" << example.input << "`\n";
            }
        }
        info << "\n";
    }
    
    if (!metadata.inputs.empty()) {
        info << "**Inputs:**\n";
        for (const auto& [key, value] : metadata.inputs) {
            info << "- `" << key << "`: " << value << "\n";
        }
        info << "\n";
    }
    
    if (!metadata.outputs.empty()) {
        info << "**Outputs:**\n";
        for (const auto& [key, value] : metadata.outputs) {
            info << "- `" << key << "`: " << value << "\n";
        }
        info << "\n";
    }
    
    if (!metadata.links.empty()) {
        info << "**Links:**\n";
        for (const auto& link : metadata.links) {
            info << "- " << link << "\n";
        }
        info << "\n";
    }
    
    if (!metadata.tags.empty()) {
        info << "**Tags:** ";
        for (size_t i = 0; i < metadata.tags.size(); ++i) {
            if (i > 0) info << ", ";
            info << "`" << metadata.tags[i] << "`";
        }
        info << "\n\n";
    }
    
    // Note: cost field removed (deprecated in v2.0)
    
    return info.str();
}

std::vector<std::string> BlueprintIDE::generate_completions(const std::string& prefix,
                                                           const BlueprintRegistry& registry) const {
    std::vector<std::string> completions;
    
    // Search by text to find matching blueprints
    auto matches = registry.search_by_text(prefix);
    
    for (const auto& match : matches) {
        auto blueprint_opt = registry.get_blueprint(match);
        if (blueprint_opt) {
            // Create completion item with function name and summary
            std::string completion = match + " - " + blueprint_opt->summary;
            completions.push_back(completion);
        }
    }
    
    return completions;
}

std::string BlueprintIDE::generate_signature_help(const BlueprintMetadata& metadata) const {
    std::ostringstream help;
    
    help << metadata.summary;
    
    if (!metadata.rules.empty()) {
        help << "\n\nConstraints:";
        for (const auto& rule : metadata.rules) {
            help << "\n- " << rule;
        }
    }
    
    return help.str();
}

std::string BlueprintIDE::export_for_lsp() const {
    // Export blueprint data in LSP-compatible format
    // This would be used by the Language Server Protocol implementation
    return "{}"; // Placeholder
}

// Create the @blueprint macro
std::shared_ptr<Macro> create_blueprint_macro() {
    auto transformer = [](const kernel::Value& ast_node, MacroExpander& expander) 
        -> std::expected<kernel::Value, std::string> {
        
        // Extract macro arguments
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        auto args = *args_result;
        if (args.size() != 2) {
            return std::unexpected("@blueprint macro expects 2 arguments: blueprint_data and function_definition");
        }
        
        kernel::Value blueprint_data = args[0];
        kernel::Value function_def = args[1];
        
        // Extract function name
        auto function_name_result = extract_function_name(function_def);
        if (!function_name_result) {
            return std::unexpected("Failed to extract function name: " + function_name_result.error());
        }
        
        std::string function_name = *function_name_result;
        
        // Parse blueprint metadata
        BlueprintParser parser;
        auto metadata_result = parser.parse_blueprint(blueprint_data, function_name);
        if (!metadata_result) {
            return std::unexpected("Failed to parse blueprint: " + metadata_result.error());
        }
        
        auto metadata = *metadata_result;
        
        // Resolve inheritance if needed
        if (metadata.parent_blueprint) {
            BlueprintInheritance inheritance;
            auto resolved_result = inheritance.resolve_inheritance(metadata, BlueprintRegistry::instance());
            if (!resolved_result) {
                return std::unexpected("Failed to resolve blueprint inheritance: " + resolved_result.error());
            }
            metadata = *resolved_result;
        }
        
        // Register blueprint in registry
        BlueprintRegistry::instance().register_blueprint(function_name, metadata);
        
        // Return the original function definition (blueprint is metadata only)
        return function_def;
    };
    
    return make_macro("blueprint", {"blueprint_data", "function_def"}, transformer);
}

// Register blueprint-related macros
void register_blueprint_macros() {
    auto& registry = MacroRegistry::instance();
    registry.register_macro(create_blueprint_macro());
}

// Helper functions
std::expected<std::string, std::string>
extract_function_name(const kernel::Value& function_ast) {
    // Simplified function name extraction
    // In a real implementation, this would integrate with the full parser
    
    if (!function_ast.is<kernel::Cons>()) {
        return std::unexpected("Function definition must be a list");
    }
    
    auto func_list = function_ast.as<kernel::Cons>();
    if (!func_list || func_list->is_nil()) {
        return std::unexpected("Empty function definition");
    }
    
    // Expect: (fnc function_name ...)
    if (!func_list->head().is<kernel::Symbol>()) {
        return std::unexpected("Function definition must start with 'fnc'");
    }
    
    auto fnc_symbol = func_list->head().as<kernel::Symbol>();
    if (fnc_symbol->name() != "fnc") {
        return std::unexpected("Expected 'fnc' keyword");
    }
    
    // Get function name (second element)
    if (!func_list->tail().is<kernel::Cons>()) {
        return std::unexpected("Function definition missing name");
    }
    
    auto name_cons = func_list->tail().as<kernel::Cons>();
    if (!name_cons || name_cons->is_nil()) {
        return std::unexpected("Function definition missing name");
    }
    
    if (!name_cons->head().is<kernel::Symbol>()) {
        return std::unexpected("Function name must be a symbol");
    }
    
    auto name_symbol = name_cons->head().as<kernel::Symbol>();
    return name_symbol->name();
}

bool has_blueprint_annotation(const kernel::Value& function_ast) {
    // Check if function has @blueprint annotation
    // This would be implemented based on how annotations are stored in the AST
    return false; // Placeholder
}

std::expected<kernel::Value, std::string>
extract_blueprint_annotation(const kernel::Value& function_ast) {
    // Extract @blueprint annotation from function AST
    // This would be implemented based on how annotations are stored in the AST
    return std::unexpected("Not implemented"); // Placeholder
}

std::string generate_mcp_tool(const std::string& function_name,
                             const BlueprintMetadata& metadata) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"name\": \"" << function_name << "\",\n";
    json << "  \"description\": \"" << metadata.summary << "\",\n";
    json << "  \"inputSchema\": {\n";
    json << "    \"type\": \"object\",\n";
    json << "    \"properties\": {";
    
    // Include inputs in the schema
    bool first_prop = true;
    for (const auto& [key, value] : metadata.inputs) {
        if (!first_prop) json << ",";
        first_prop = false;
        json << "\n      \"" << key << "\": {\n";
        json << "        \"type\": \"string\",\n";
        json << "        \"description\": \"" << value << "\"\n";
        json << "      }";
    }
    
    json << "\n    },\n";
    json << "    \"required\": []";  // Could be enhanced to mark required inputs
    json << "\n  }";
    
    if (!metadata.outputs.empty()) {
        json << ",\n  \"outputSchema\": {\n";
        json << "    \"type\": \"object\",\n";
        json << "    \"properties\": {";
        
        bool first_output = true;
        for (const auto& [key, value] : metadata.outputs) {
            if (!first_output) json << ",";
            first_output = false;
            json << "\n      \"" << key << "\": {\n";
            json << "        \"type\": \"string\",\n";
            json << "        \"description\": \"" << value << "\"\n";
            json << "      }";
        }
        
        json << "\n    }\n";
        json << "  }";
    }
    
    if (!metadata.examples.empty()) {
        json << ",\n  \"examples\": [\n";
        for (size_t i = 0; i < metadata.examples.size(); ++i) {
            if (i > 0) json << ",\n";
            json << "    {\n";
            json << "      \"input\": \"" << metadata.examples[i].input << "\",\n";
            json << "      \"output\": \"" << metadata.examples[i].output << "\"";
            if (metadata.examples[i].description) {
                json << ",\n      \"description\": \"" << *metadata.examples[i].description << "\"";
            }
            json << "\n    }";
        }
        json << "\n  ]";
    }
    
    // Include specs in MCP tool JSON (AI_DX Req 140)
    if (!metadata.specs.empty()) {
        json << ",\n  \"specs\": " << specs_to_json(metadata.specs, function_name);
    }
    
    if (!metadata.tags.empty()) {
        json << ",\n  \"tags\": [\n";
        for (size_t i = 0; i < metadata.tags.size(); ++i) {
            if (i > 0) json << ",\n";
            json << "    \"" << metadata.tags[i] << "\"";
        }
        json << "\n  ]";
    }
    
    if (!metadata.links.empty()) {
        json << ",\n  \"links\": [\n";
        for (size_t i = 0; i < metadata.links.size(); ++i) {
            if (i > 0) json << ",\n";
            json << "    \"" << metadata.links[i] << "\"";
        }
        json << "\n  ]";
    }
    
    json << "\n}";
    
    return json.str();
}

// Serialize spec pairs to structured JSON (AI_DX Req 140)
std::string specs_to_json(const std::vector<SpecPair>& specs,
                          const std::string& symbol_name) {
    std::ostringstream json;
    
    json << "[\n";
    for (size_t i = 0; i < specs.size(); ++i) {
        if (i > 0) json << ",\n";
        json << "    {\n";
        json << "      \"action\": \"" << specs[i].action << "\",\n";
        json << "      \"result\": \"" << specs[i].result << "\",\n";
        
        // Declared effects
        json << "      \"declared_effects\": [";
        for (size_t j = 0; j < specs[i].declared_effects.size(); ++j) {
            if (j > 0) json << ", ";
            json << "\"" << specs[i].declared_effects[j] << "\"";
        }
        json << "],\n";
        
        // Verification status
        const char* status_str = "unverified";
        if (specs[i].status == SpecPair::Status::Verified) {
            status_str = "verified";
        } else if (specs[i].status == SpecPair::Status::Failed) {
            status_str = "failed";
        }
        json << "      \"status\": \"" << status_str << "\"";
        
        // Failure details (only when failed)
        if (specs[i].status == SpecPair::Status::Failed) {
            if (specs[i].actual_value) {
                json << ",\n      \"actual_value\": \"" << *specs[i].actual_value << "\"";
            }
            if (specs[i].failure_message) {
                json << ",\n      \"failure_message\": \"" << *specs[i].failure_message << "\"";
            }
        }
        
        // Include symbol name if provided
        if (!symbol_name.empty()) {
            json << ",\n      \"symbol\": \"" << symbol_name << "\"";
        }
        
        json << "\n    }";
    }
    json << "\n  ]";
    
    return json.str();
}

} // namespace meld::macro