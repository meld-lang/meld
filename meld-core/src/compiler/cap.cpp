#include "meld/compiler/cap.hpp"
#include "meld/parser/parser.hpp"
#include "meld/api/meld_binary.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unordered_set>

namespace meld::compiler::cap {

// EffectInfo implementation
nlohmann::json EffectInfo::to_json() const {
    return nlohmann::json{
        {"function_name", function_name},
        {"declared_effects", declared_effects},
        {"inferred_effects", inferred_effects},
        {"effects_match", effects_match},
        {"effect_annotation", effect_annotation}
    };
}

EffectInfo EffectInfo::from_json(const nlohmann::json& j) {
    EffectInfo info(j["function_name"]);
    info.declared_effects = j.value("declared_effects", std::vector<std::string>{});
    info.inferred_effects = j.value("inferred_effects", std::vector<std::string>{});
    info.effects_match = j.value("effects_match", true);
    info.effect_annotation = j.value("effect_annotation", "");
    return info;
}

// ASGQueryResult implementation
nlohmann::json ASGQueryResult::to_json() const {
    nlohmann::json j = {
        {"query_type", query_type},
        {"pattern", pattern},
        {"matching_nodes", matching_nodes},
        {"node_descriptions", node_descriptions},
        {"query_time_ms", query_time.count()}
    };
    
    j["result_count"] = matching_nodes.size();
    return j;
}

ASGQueryResult ASGQueryResult::from_json(const nlohmann::json& j) {
    ASGQueryResult result(j["query_type"], j["pattern"]);
    result.matching_nodes = j.value("matching_nodes", std::vector<api::NodeId>{});
    result.node_descriptions = j.value("node_descriptions", std::vector<std::string>{});
    result.query_time = std::chrono::milliseconds(j.value("query_time_ms", 0));
    return result;
}

// ExecutionSnapshot implementation
nlohmann::json ExecutionSnapshot::to_json() const {
    nlohmann::json j = {
        {"snapshot_id", snapshot_id},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count()},
        {"function_name", function_name},
        {"function_inputs", function_inputs},
        {"effect_history", effect_history},
        {"call_stack", call_stack},
        {"local_variables", local_variables},
        {"environment_metadata", environment_metadata},
        {"error_message", error_message},
        {"error_type", error_type}
    };
    return j;
}

ExecutionSnapshot ExecutionSnapshot::from_json(const nlohmann::json& j) {
    ExecutionSnapshot snapshot(j["snapshot_id"], j["function_name"]);
    snapshot.timestamp = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(j.value("timestamp", 0))
    );
    snapshot.function_inputs = j.value("function_inputs", std::vector<std::string>{});
    snapshot.effect_history = j.value("effect_history", std::vector<std::string>{});
    snapshot.call_stack = j.value("call_stack", std::vector<std::string>{});
    snapshot.local_variables = j.value("local_variables", std::map<std::string, std::string>{});
    snapshot.environment_metadata = j.value("environment_metadata", std::map<std::string, std::string>{});
    snapshot.error_message = j.value("error_message", "");
    snapshot.error_type = j.value("error_type", "");
    return snapshot;
}

// FlowVisualizationInfo implementation
nlohmann::json FlowVisualizationInfo::to_json() const {
    return nlohmann::json{
        {"flow_name", flow_name},
        {"visualization_format", visualization_format},
        {"visualization_data", visualization_data},
        {"state_names", state_names},
        {"transition_descriptions", transition_descriptions},
        {"metadata", metadata}
    };
}

FlowVisualizationInfo FlowVisualizationInfo::from_json(const nlohmann::json& j) {
    FlowVisualizationInfo info(j["flow_name"], j["visualization_format"]);
    info.visualization_data = j.value("visualization_data", "");
    info.state_names = j.value("state_names", std::vector<std::string>{});
    info.transition_descriptions = j.value("transition_descriptions", std::vector<std::string>{});
    info.metadata = j.value("metadata", std::map<std::string, std::string>{});
    return info;
}

// ProvenanceInfo implementation
nlohmann::json ProvenanceInfo::to_json() const {
    nlohmann::json j = {
        {"node_id", node_id},
        {"origin", origin_type_to_json(origin)},
        {"creation_timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(creation_timestamp.time_since_epoch()).count()},
        {"trust_score", trust_score},
        {"mismatch_warnings", mismatch_warnings}
    };
    
    if (author_email) j["author_email"] = *author_email;
    if (agent_model) j["agent_model"] = *agent_model;
    if (confidence_score) j["confidence_score"] = *confidence_score;
    if (reviewer_email) j["reviewer_email"] = *reviewer_email;
    if (verification_timestamp) {
        j["verification_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            verification_timestamp->time_since_epoch()).count();
    }
    
    return j;
}

ProvenanceInfo ProvenanceInfo::from_json(const nlohmann::json& j) {
    ProvenanceInfo info(j["node_id"], origin_type_from_json(j["origin"]));
    info.creation_timestamp = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(j.value("creation_timestamp", 0))
    );
    info.trust_score = j.value("trust_score", 0.0);
    info.mismatch_warnings = j.value("mismatch_warnings", std::vector<std::string>{});
    
    if (j.contains("author_email")) info.author_email = j["author_email"];
    if (j.contains("agent_model")) info.agent_model = j["agent_model"];
    if (j.contains("confidence_score")) info.confidence_score = j["confidence_score"];
    if (j.contains("reviewer_email")) info.reviewer_email = j["reviewer_email"];
    if (j.contains("verification_timestamp")) {
        info.verification_timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(j["verification_timestamp"])
        );
    }
    
    return info;
}

// Location implementation
nlohmann::json Location::to_json() const {
    return nlohmann::json{
        {"file", file},
        {"line", line},
        {"column", column},
        {"end_line", end_line},
        {"end_column", end_column}
    };
}

Location Location::from_json(const nlohmann::json& j) {
    return Location(
        j.value("file", ""),
        j.value("line", 0),
        j.value("column", 0),
        j.value("end_line", 0),
        j.value("end_column", 0)
    );
}

// FixSuggestion implementation
nlohmann::json FixSuggestion::to_json() const {
    nlohmann::json j = {
        {"type", fix_type_to_json(type)},
        {"description", description},
        {"location", location.to_json()},
        {"replacement_text", replacement_text},
        {"confidence", confidence_to_json(confidence)}
    };
    
    if (!additional_changes.empty()) {
        j["additional_changes"] = additional_changes;
    }
    
    return j;
}

FixSuggestion FixSuggestion::from_json(const nlohmann::json& j) {
    FixSuggestion suggestion(
        fix_type_from_json(j["type"]),
        j["description"],
        Location::from_json(j["location"]),
        j["replacement_text"],
        confidence_from_json(j["confidence"])
    );
    
    if (j.contains("additional_changes")) {
        suggestion.additional_changes = j["additional_changes"];
    }
    
    return suggestion;
}

// CompilationMessage implementation
nlohmann::json CompilationMessage::to_json() const {
    nlohmann::json j = {
        {"severity", severity_to_json(severity)},
        {"code", code},
        {"message", message},
        {"location", location.to_json()}
    };
    
    if (!context.empty()) {
        j["context"] = context;
    }
    
    if (!suggestions.empty()) {
        j["suggestions"] = nlohmann::json::array();
        for (const auto& suggestion : suggestions) {
            j["suggestions"].push_back(suggestion.to_json());
        }
    }
    
    if (!related_locations.empty()) {
        j["related_locations"] = nlohmann::json::array();
        for (const auto& loc : related_locations) {
            j["related_locations"].push_back(loc.to_json());
        }
    }
    
    return j;
}

CompilationMessage CompilationMessage::from_json(const nlohmann::json& j) {
    CompilationMessage message(
        severity_from_json(j["severity"]),
        j["code"],
        j["message"],
        Location::from_json(j["location"])
    );
    
    if (j.contains("context")) {
        message.context = j["context"];
    }
    
    if (j.contains("suggestions")) {
        for (const auto& suggestion_json : j["suggestions"]) {
            message.suggestions.push_back(FixSuggestion::from_json(suggestion_json));
        }
    }
    
    if (j.contains("related_locations")) {
        for (const auto& loc_json : j["related_locations"]) {
            message.related_locations.push_back(Location::from_json(loc_json));
        }
    }
    
    return message;
}

// CompilationResult implementation
bool CompilationResult::has_errors() const {
    return std::any_of(messages.begin(), messages.end(),
        [](const CompilationMessage& msg) { return msg.severity == MessageSeverity::ERROR; });
}

bool CompilationResult::has_warnings() const {
    return std::any_of(messages.begin(), messages.end(),
        [](const CompilationMessage& msg) { return msg.severity == MessageSeverity::WARNING; });
}

std::vector<CompilationMessage> CompilationResult::get_errors() const {
    std::vector<CompilationMessage> errors;
    std::copy_if(messages.begin(), messages.end(), std::back_inserter(errors),
        [](const CompilationMessage& msg) { return msg.severity == MessageSeverity::ERROR; });
    return errors;
}

std::vector<CompilationMessage> CompilationResult::get_warnings() const {
    std::vector<CompilationMessage> warnings;
    std::copy_if(messages.begin(), messages.end(), std::back_inserter(warnings),
        [](const CompilationMessage& msg) { return msg.severity == MessageSeverity::WARNING; });
    return warnings;
}

nlohmann::json CompilationResult::to_json() const {
    nlohmann::json j = {
        {"file_path", file_path},
        {"success", success},
        {"compilation_time_ms", compilation_time.count()},
        {"lines_of_code", lines_of_code},
        {"ast_node_count", ast_node_count}
    };
    
    // Messages
    j["messages"] = nlohmann::json::array();
    for (const auto& message : messages) {
        j["messages"].push_back(message.to_json());
    }
    
    // Optional fields
    if (generated_code) {
        j["generated_code"] = *generated_code;
    }
    
    if (output_file) {
        j["output_file"] = *output_file;
    }
    
    if (!dependencies.empty()) {
        j["dependencies"] = dependencies;
    }
    
    // Effect information (Requirement 40.10)
    if (!effect_information.empty()) {
        j["effect_information"] = nlohmann::json::array();
        for (const auto& effect_info : effect_information) {
            j["effect_information"].push_back(effect_info.to_json());
        }
    }
    
    // MELD-B file path (Requirement 40.10)
    if (meld_b_file) {
        j["meld_b_file"] = *meld_b_file;
    }
    
    // v2.0 AI-native features
    
    // Provenance information (Requirement 45)
    if (!provenance_information.empty()) {
        j["provenance_information"] = nlohmann::json::array();
        for (const auto& provenance_info : provenance_information) {
            j["provenance_information"].push_back(provenance_info.to_json());
        }
    }
    
    // Flow visualizations (Requirement 43.9)
    if (!flow_visualizations.empty()) {
        j["flow_visualizations"] = nlohmann::json::array();
        for (const auto& flow_viz : flow_visualizations) {
            j["flow_visualizations"].push_back(flow_viz.to_json());
        }
    }
    
    // Flight recorder snapshot (Requirement 44.9)
    if (crash_snapshot) {
        j["crash_snapshot"] = crash_snapshot->to_json();
    }
    
    // Summary counts
    j["error_count"] = get_errors().size();
    j["warning_count"] = get_warnings().size();
    
    return j;
}

CompilationResult CompilationResult::from_json(const nlohmann::json& j) {
    CompilationResult result(j["file_path"]);
    result.success = j["success"];
    result.compilation_time = std::chrono::milliseconds(j["compilation_time_ms"]);
    result.lines_of_code = j["lines_of_code"];
    result.ast_node_count = j["ast_node_count"];
    
    if (j.contains("messages")) {
        for (const auto& message_json : j["messages"]) {
            result.messages.push_back(CompilationMessage::from_json(message_json));
        }
    }
    
    if (j.contains("generated_code")) {
        result.generated_code = j["generated_code"];
    }
    
    if (j.contains("output_file")) {
        result.output_file = j["output_file"];
    }
    
    if (j.contains("dependencies")) {
        result.dependencies = j["dependencies"];
    }
    
    if (j.contains("effect_information")) {
        for (const auto& effect_json : j["effect_information"]) {
            result.effect_information.push_back(EffectInfo::from_json(effect_json));
        }
    }
    
    if (j.contains("meld_b_file")) {
        result.meld_b_file = j["meld_b_file"];
    }
    
    return result;
}

// BatchCompilationResult implementation
void BatchCompilationResult::add_result(CompilationResult result) {
    total_files++;
    if (result.success) {
        successful_files++;
    }
    
    total_errors += result.get_errors().size();
    total_warnings += result.get_warnings().size();
    
    results.push_back(std::move(result));
}

void BatchCompilationResult::finalize() {
    overall_success = (total_errors == 0);
    
    // Calculate total time
    total_time = std::chrono::milliseconds(0);
    for (const auto& result : results) {
        total_time += result.compilation_time;
    }
}

nlohmann::json BatchCompilationResult::to_json() const {
    nlohmann::json j = {
        {"overall_success", overall_success},
        {"total_time_ms", total_time.count()},
        {"total_files", total_files},
        {"successful_files", successful_files},
        {"total_errors", total_errors},
        {"total_warnings", total_warnings}
    };
    
    j["results"] = nlohmann::json::array();
    for (const auto& result : results) {
        j["results"].push_back(result.to_json());
    }
    
    return j;
}

BatchCompilationResult BatchCompilationResult::from_json(const nlohmann::json& j) {
    BatchCompilationResult batch_result;
    batch_result.overall_success = j["overall_success"];
    batch_result.total_time = std::chrono::milliseconds(j["total_time_ms"]);
    batch_result.total_files = j["total_files"];
    batch_result.successful_files = j["successful_files"];
    batch_result.total_errors = j["total_errors"];
    batch_result.total_warnings = j["total_warnings"];
    
    if (j.contains("results")) {
        for (const auto& result_json : j["results"]) {
            batch_result.results.push_back(CompilationResult::from_json(result_json));
        }
    }
    
    return batch_result;
}

// IncrementalChange implementation
nlohmann::json IncrementalChange::to_json() const {
    nlohmann::json j = {
        {"type", static_cast<int>(type)},
        {"file_path", file_path},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count()}
    };
    
    if (old_content_hash) {
        j["old_content_hash"] = *old_content_hash;
    }
    
    if (new_content_hash) {
        j["new_content_hash"] = *new_content_hash;
    }
    
    return j;
}

IncrementalChange IncrementalChange::from_json(const nlohmann::json& j) {
    IncrementalChange change(
        static_cast<IncrementalChange::ChangeType>(j["type"]),
        j["file_path"]
    );
    
    auto timestamp_ms = j["timestamp"];
    change.timestamp = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
    
    if (j.contains("old_content_hash")) {
        change.old_content_hash = j["old_content_hash"];
    }
    
    if (j.contains("new_content_hash")) {
        change.new_content_hash = j["new_content_hash"];
    }
    
    return change;
}

// CompilerAgentProtocol implementation
CompilerAgentProtocol::CompilerAgentProtocol()
    : include_suggestions_(true)
    , max_suggestions_per_error_(3)
    , confidence_threshold_(ConfidenceLevel::MEDIUM)
    , generate_meld_b_(false)
    , include_effect_info_(true)
    , enable_asg_queries_(true)
    , include_provenance_info_(false)
    , include_flow_visualizations_(false)
    , enable_crash_snapshots_(false)
    , min_trust_level_(0.5)
    , type_checker_(std::make_unique<TypeChecker>())
    , parser_(std::make_unique<parser::Parser>()) {
}

CompilationResult CompilerAgentProtocol::compile_file(const std::filesystem::path& file_path) {
    try {
        std::string source_code = read_file_content(file_path);
        return compile_internal(source_code, file_path.string());
    } catch (const std::exception& e) {
        CompilationResult result(file_path.string());
        result.messages.emplace_back(
            MessageSeverity::ERROR,
            "E001",
            "Failed to read file: " + std::string(e.what()),
            Location(file_path.string(), 1, 1, 1, 1)
        );
        return result;
    }
}

CompilationResult CompilerAgentProtocol::compile_source(const std::string& source_code, 
                                                       const std::string& file_name) {
    return compile_internal(source_code, file_name);
}

BatchCompilationResult CompilerAgentProtocol::compile_batch(const std::vector<std::filesystem::path>& files) {
    BatchCompilationResult batch_result;
    
    for (const auto& file : files) {
        auto result = compile_file(file);
        batch_result.add_result(std::move(result));
    }
    
    batch_result.finalize();
    return batch_result;
}

BatchCompilationResult CompilerAgentProtocol::compile_incremental(const std::vector<IncrementalChange>& changes) {
    BatchCompilationResult batch_result;
    
    // For incremental compilation, we only compile modified and added files
    for (const auto& change : changes) {
        if (change.type == IncrementalChange::ChangeType::FILE_MODIFIED ||
            change.type == IncrementalChange::ChangeType::FILE_ADDED) {
            
            auto result = compile_file(change.file_path);
            batch_result.add_result(std::move(result));
        }
    }
    
    batch_result.finalize();
    return batch_result;
}

std::string CompilerAgentProtocol::to_json_string(const CompilationResult& result, bool pretty) {
    auto json = result.to_json();
    return pretty ? json.dump(2) : json.dump();
}

std::string CompilerAgentProtocol::to_json_string(const BatchCompilationResult& result, bool pretty) {
    auto json = result.to_json();
    return pretty ? json.dump(2) : json.dump();
}

std::string CompilerAgentProtocol::to_json_string(const ASGQueryResult& result, bool pretty) {
    auto json = result.to_json();
    return pretty ? json.dump(2) : json.dump();
}

std::string CompilerAgentProtocol::to_json_string(const EffectInfo& effect_info, bool pretty) {
    auto json = effect_info.to_json();
    return pretty ? json.dump(2) : json.dump();
}

// v2.0 JSON serialization helpers
std::string CompilerAgentProtocol::to_json_string(const ProvenanceInfo& provenance_info, bool pretty) {
    auto json = provenance_info.to_json();
    return pretty ? json.dump(2) : json.dump();
}

std::string CompilerAgentProtocol::to_json_string(const FlowVisualizationInfo& flow_info, bool pretty) {
    auto json = flow_info.to_json();
    return pretty ? json.dump(2) : json.dump();
}

std::string CompilerAgentProtocol::to_json_string(const ExecutionSnapshot& snapshot, bool pretty) {
    auto json = snapshot.to_json();
    return pretty ? json.dump(2) : json.dump();
}

// MELD-B support (Requirement 40.10)
CompilationResult CompilerAgentProtocol::compile_to_meld_b(const std::filesystem::path& file_path, 
                                                          const std::filesystem::path& output_path) {
    auto result = compile_file(file_path);
    
    if (result.success && current_graph_) {
        try {
            std::string meld_b_path = output_path.empty() ? 
                generate_meld_b_path(file_path.string()) : output_path.string();
            
            save_meld_binary(current_graph_, meld_b_path);
            result.meld_b_file = meld_b_path;
            
            // Add success message
            result.messages.emplace_back(
                MessageSeverity::INFO,
                "I001",
                "Generated MELD-B file: " + meld_b_path,
                Location(file_path.string(), 1, 1, 1, 1)
            );
        } catch (const std::exception& e) {
            result.messages.emplace_back(
                MessageSeverity::WARNING,
                "W002",
                "Failed to generate MELD-B file: " + std::string(e.what()),
                Location(file_path.string(), 1, 1, 1, 1)
            );
        }
    }
    
    return result;
}

std::unique_ptr<api::MeldBinary> CompilerAgentProtocol::get_meld_binary(const CompilationResult& result) {
    if (!result.meld_b_file) {
        return nullptr;
    }
    
    return api::MeldBinary::load(*result.meld_b_file);
}

// ASG queries (Requirement 40.10)
ASGQueryResult CompilerAgentProtocol::query_asg(const std::string& pattern, const std::string& query_type) {
    if (!enable_asg_queries_ || !current_graph_) {
        ASGQueryResult result(query_type, pattern);
        result.node_descriptions.push_back("ASG queries not enabled or no graph available");
        return result;
    }
    
    return execute_asg_query(pattern, query_type, current_graph_);
}

ASGQueryResult CompilerAgentProtocol::query_by_effect(const std::string& effect_name) {
    ASGQueryResult result("effect", effect_name);
    
    if (!enable_asg_queries_ || !current_graph_) {
        result.node_descriptions.push_back("ASG queries not enabled or no graph available");
        return result;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Find functions that use the specified effect
        auto function_nodes = current_graph_->getNodesByType(api::NodeType::FUNCTION_DECLARATION);
        
        for (const auto& node : function_nodes) {
            // Check if function uses the effect (simplified implementation)
            auto node_data = std::string("node_" + std::to_string(node->getId()));
            if (node_data.find(effect_name) != std::string::npos) {
                result.matching_nodes.push_back(node->getId());
                result.node_descriptions.push_back(describe_node(node->getId(), current_graph_));
            }
        }
    } catch (const std::exception& e) {
        result.node_descriptions.push_back("Query error: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.query_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

ASGQueryResult CompilerAgentProtocol::query_usage(const std::string& symbol) {
    ASGQueryResult result("usage", symbol);
    
    if (!enable_asg_queries_ || !current_graph_) {
        result.node_descriptions.push_back("ASG queries not enabled or no graph available");
        return result;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        auto usage_edges = api::SemanticGraphAPI::findUsage(*current_graph_, symbol);
        
        for (const auto& edge : usage_edges) {
            result.matching_nodes.push_back(edge.node_id);
            result.node_descriptions.push_back(
                "Usage at " + edge.location.file_path + ":" + std::to_string(edge.location.line) + ": " + edge.context
            );
        }
    } catch (const std::exception& e) {
        result.node_descriptions.push_back("Query error: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.query_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

std::vector<ASGQueryResult> CompilerAgentProtocol::batch_query_asg(
    const std::vector<std::pair<std::string, std::string>>& queries) {
    
    std::vector<ASGQueryResult> results;
    results.reserve(queries.size());
    
    for (const auto& [pattern, query_type] : queries) {
        results.push_back(query_asg(pattern, query_type));
    }
    
    return results;
}

// Effect analysis
std::vector<EffectInfo> CompilerAgentProtocol::analyze_effects(const std::filesystem::path& file_path) {
    try {
        std::string source_code = read_file_content(file_path);
        
        std::vector<parser::ast::expression> ast;
        bool parse_success = parser_->parse_file(source_code, ast);
        
        if (!parse_success) {
            return {}; // Return empty if parsing fails
        }
        
        return extract_effect_info(ast, file_path.string());
    } catch (const std::exception&) {
        return {};
    }
}

EffectInfo CompilerAgentProtocol::get_function_effects(const std::string& function_name) {
    EffectInfo info(function_name);
    
    if (!current_graph_) {
        return info;
    }
    
    // Find function in current graph and analyze its effects
    auto function_nodes = current_graph_->getNodesByType(api::NodeType::FUNCTION_DECLARATION);
    
    for (const auto& node : function_nodes) {
        auto node_data = std::string("node_" + std::to_string(node->getId()));
        if (node_data.find(function_name) != std::string::npos) {
            // Extract effect information from node (simplified)
            info.inferred_effects = {"FileSystem", "Network"}; // Placeholder
            info.effect_annotation = "@uses(FileSystem, Network)";
            break;
        }
    }
    
    return info;
}

// Private implementation methods
CompilationResult CompilerAgentProtocol::compile_internal(const std::string& source_code, 
                                                         const std::string& file_name) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    CompilationResult result(file_name);
    result.lines_of_code = count_lines(source_code);
    
    try {
        // Parse the source code
        std::vector<parser::ast::expression> ast;
        bool parse_success = parser_->parse_file(source_code, ast);
        
        if (!parse_success) {
            // Parse error
            result.messages.emplace_back(
                MessageSeverity::ERROR,
                "E002",
                "Parse error: " + parser_->error_message(),
                Location(file_name, 1, 1, 1, 1)
            );
            
            if (include_suggestions_) {
                // Add basic syntax error suggestions
                TypeError parse_error("Parse error: " + parser_->error_message(), file_name, 1, 1);
                auto suggestions = suggest_syntax_error_fixes(parse_error);
                result.messages.back().suggestions = std::move(suggestions);
            }
        } else {
            // Count AST nodes
            result.ast_node_count = count_ast_nodes(ast);
            
            // Build semantic graph for ASG queries (Requirement 40.10)
            if (enable_asg_queries_) {
                try {
                    current_graph_ = api::SemanticGraphAPI::fromAST(ast[0], file_name);
                } catch (const std::exception& e) {
                    result.messages.emplace_back(
                        MessageSeverity::WARNING,
                        "W003",
                        "Failed to build semantic graph: " + std::string(e.what()),
                        Location(file_name, 1, 1, 1, 1)
                    );
                }
            }
            
            // Extract effect information (Requirement 40.10)
            if (include_effect_info_) {
                try {
                    result.effect_information = extract_effect_info(ast, file_name);
                } catch (const std::exception& e) {
                    result.messages.emplace_back(
                        MessageSeverity::WARNING,
                        "W004",
                        "Failed to extract effect information: " + std::string(e.what()),
                        Location(file_name, 1, 1, 1, 1)
                    );
                }
            }
            
            // Extract provenance information (Requirement 45 - v2.0)
            if (include_provenance_info_) {
                try {
                    result.provenance_information = extract_provenance_info(ast, file_name);
                } catch (const std::exception& e) {
                    result.messages.emplace_back(
                        MessageSeverity::WARNING,
                        "W006",
                        "Failed to extract provenance information: " + std::string(e.what()),
                        Location(file_name, 1, 1, 1, 1)
                    );
                }
            }
            
            // Extract flow visualizations (Requirement 43.9 - v2.0)
            if (include_flow_visualizations_) {
                try {
                    result.flow_visualizations = extract_flow_definitions(ast);
                } catch (const std::exception& e) {
                    result.messages.emplace_back(
                        MessageSeverity::WARNING,
                        "W007",
                        "Failed to extract flow visualizations: " + std::string(e.what()),
                        Location(file_name, 1, 1, 1, 1)
                    );
                }
            }
            
            // Generate MELD-B file if requested (Requirement 40.10)
            if (generate_meld_b_ && current_graph_) {
                try {
                    std::string meld_b_path = generate_meld_b_path(file_name);
                    save_meld_binary(current_graph_, meld_b_path);
                    result.meld_b_file = meld_b_path;
                } catch (const std::exception& e) {
                    result.messages.emplace_back(
                        MessageSeverity::WARNING,
                        "W005",
                        "Failed to generate MELD-B file: " + std::string(e.what()),
                        Location(file_name, 1, 1, 1, 1)
                    );
                }
            }
            
            // Type check the AST
            auto type_result = type_checker_->check_program(ast);
            
            if (!type_result) {
                // Type checking errors
                auto error_messages = process_type_errors(type_result.error(), file_name);
                result.messages.insert(result.messages.end(), 
                                     error_messages.begin(), error_messages.end());
            } else {
                // Success
                result.success = true;
                
                // Add any warnings from type checker
                for (const auto& warning : type_checker_->errors()) {
                    if (warning.message.find("warning") != std::string::npos) {
                        result.messages.emplace_back(
                            MessageSeverity::WARNING,
                            "W001",
                            warning.message,
                            error_to_location(warning)
                        );
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        result.messages.emplace_back(
            MessageSeverity::ERROR,
            "E003",
            "Internal compiler error: " + std::string(e.what()),
            Location(file_name, 1, 1, 1, 1)
        );
        
        // Create crash snapshot if enabled (Requirement 44.9 - v2.0)
        if (enable_crash_snapshots_) {
            try {
                auto crash_snapshot = create_crash_snapshot(
                    "compile_internal",
                    "Internal compiler error: " + std::string(e.what()),
                    "std::exception"
                );
                result.crash_snapshot = crash_snapshot;
                last_crash_snapshot_ = crash_snapshot;
                
                result.messages.emplace_back(
                    MessageSeverity::INFO,
                    "I002",
                    "Crash snapshot captured: " + crash_snapshot.snapshot_id,
                    Location(file_name, 1, 1, 1, 1)
                );
            } catch (const std::exception& snapshot_error) {
                result.messages.emplace_back(
                    MessageSeverity::WARNING,
                    "W008",
                    "Failed to create crash snapshot: " + std::string(snapshot_error.what()),
                    Location(file_name, 1, 1, 1, 1)
                );
            }
        }
    }
    
    // Calculate compilation time
    auto end_time = std::chrono::high_resolution_clock::now();
    result.compilation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

std::vector<CompilationMessage> CompilerAgentProtocol::process_type_errors(
    const std::vector<TypeError>& errors, const std::string& file_name) {
    
    std::vector<CompilationMessage> messages;
    
    for (const auto& error : errors) {
        CompilationMessage message(
            MessageSeverity::ERROR,
            generate_error_code(error),
            error.message,
            error_to_location(error)
        );
        
        message.context = error.context;
        
        if (include_suggestions_) {
            auto suggestions = generate_suggestions(error);
            
            // Filter by confidence threshold and limit count
            std::vector<FixSuggestion> filtered_suggestions;
            for (const auto& suggestion : suggestions) {
                if (suggestion.confidence >= confidence_threshold_ &&
                    filtered_suggestions.size() < max_suggestions_per_error_) {
                    filtered_suggestions.push_back(suggestion);
                }
            }
            
            message.suggestions = std::move(filtered_suggestions);
        }
        
        messages.push_back(std::move(message));
    }
    
    return messages;
}

std::vector<FixSuggestion> CompilerAgentProtocol::generate_suggestions(const TypeError& error) {
    std::vector<FixSuggestion> suggestions;
    
    // Analyze error message to determine suggestion type
    if (error.message.find("Undefined variable") != std::string::npos) {
        auto var_suggestions = suggest_undefined_variable_fixes(error);
        suggestions.insert(suggestions.end(), var_suggestions.begin(), var_suggestions.end());
    }
    
    if (error.message.find("type") != std::string::npos && 
        error.message.find("mismatch") != std::string::npos) {
        auto type_suggestions = suggest_type_mismatch_fixes(error);
        suggestions.insert(suggestions.end(), type_suggestions.begin(), type_suggestions.end());
    }
    
    if (error.message.find("Unknown type") != std::string::npos) {
        auto import_suggestions = suggest_missing_import_fixes(error);
        suggestions.insert(suggestions.end(), import_suggestions.begin(), import_suggestions.end());
    }
    
    // Sort by confidence (highest first)
    std::sort(suggestions.begin(), suggestions.end(),
        [](const FixSuggestion& a, const FixSuggestion& b) {
            return static_cast<int>(a.confidence) > static_cast<int>(b.confidence);
        });
    
    return suggestions;
}

std::vector<FixSuggestion> CompilerAgentProtocol::suggest_undefined_variable_fixes(const TypeError& error) {
    std::vector<FixSuggestion> suggestions;
    
    // Extract variable name from error message
    std::regex var_regex(R"(Undefined variable '([^']+)')");
    std::smatch match;
    
    if (std::regex_search(error.message, match, var_regex)) {
        std::string var_name = match[1].str();
        Location loc = error_to_location(error);
        
        // Suggest adding variable declaration
        suggestions.emplace_back(
            FixType::INSERT_TEXT,
            "Add variable declaration",
            Location(loc.file, loc.line, 1, loc.line, 1),
            "val " + var_name + " = /* TODO: provide value */\n",
            ConfidenceLevel::HIGH
        );
        
        // Suggest common typos (if variable name is similar to common names)
        std::vector<std::string> common_vars = {"value", "result", "data", "item", "index", "count"};
        for (const auto& common_var : common_vars) {
            if (std::abs(static_cast<int>(var_name.length()) - static_cast<int>(common_var.length())) <= 2) {
                suggestions.emplace_back(
                    FixType::REPLACE_TEXT,
                    "Did you mean '" + common_var + "'?",
                    loc,
                    common_var,
                    ConfidenceLevel::MEDIUM
                );
            }
        }
    }
    
    return suggestions;
}

std::vector<FixSuggestion> CompilerAgentProtocol::suggest_type_mismatch_fixes(const TypeError& error) {
    std::vector<FixSuggestion> suggestions;
    
    Location loc = error_to_location(error);
    
    // Suggest adding type annotation
    suggestions.emplace_back(
        FixType::ADD_TYPE_ANNOTATION,
        "Add explicit type annotation",
        loc,
        ": /* TODO: specify type */",
        ConfidenceLevel::MEDIUM
    );
    
    // Suggest type conversion
    if (error.message.find("int") != std::string::npos && 
        error.message.find("string") != std::string::npos) {
        suggestions.emplace_back(
            FixType::REPLACE_TEXT,
            "Convert to string",
            loc,
            ".toString()",
            ConfidenceLevel::HIGH
        );
    }
    
    return suggestions;
}

std::vector<FixSuggestion> CompilerAgentProtocol::suggest_missing_import_fixes(const TypeError& error) {
    std::vector<FixSuggestion> suggestions;
    
    // Extract type name from error message
    std::regex type_regex(R"(Unknown type '([^']+)')");
    std::smatch match;
    
    if (std::regex_search(error.message, match, type_regex)) {
        std::string type_name = match[1].str();
        
        // Suggest common imports
        std::unordered_set<std::string> common_types = {
            "List", "Map", "Set", "Array", "String", "Int", "Float", "Bool"
        };
        
        if (common_types.count(type_name)) {
            suggestions.emplace_back(
                FixType::ADD_IMPORT,
                "Import " + type_name + " from standard library",
                Location(error.file, 1, 1, 1, 1),
                "import std.collections." + type_name,
                ConfidenceLevel::HIGH
            );
        }
    }
    
    return suggestions;
}

std::vector<FixSuggestion> CompilerAgentProtocol::suggest_syntax_error_fixes(const TypeError& error) {
    std::vector<FixSuggestion> suggestions;
    
    Location loc = error_to_location(error);
    
    // Common syntax error fixes
    if (error.message.find("expected") != std::string::npos) {
        if (error.message.find("';'") != std::string::npos) {
            suggestions.emplace_back(
                FixType::INSERT_TEXT,
                "Add missing semicolon",
                loc,
                ";",
                ConfidenceLevel::HIGH
            );
        }
        
        if (error.message.find("'}'") != std::string::npos) {
            suggestions.emplace_back(
                FixType::INSERT_TEXT,
                "Add missing closing brace",
                loc,
                "}",
                ConfidenceLevel::HIGH
            );
        }
        
        if (error.message.find("')'") != std::string::npos) {
            suggestions.emplace_back(
                FixType::INSERT_TEXT,
                "Add missing closing parenthesis",
                loc,
                ")",
                ConfidenceLevel::HIGH
            );
        }
    }
    
    return suggestions;
}

ConfidenceLevel CompilerAgentProtocol::calculate_confidence(const FixSuggestion& suggestion, 
                                                          const TypeError& error) {
    // This is a simplified confidence calculation
    // In a real implementation, this would use more sophisticated heuristics
    
    if (suggestion.type == FixType::INSERT_TEXT && 
        (suggestion.replacement_text == ";" || 
         suggestion.replacement_text == "}" || 
         suggestion.replacement_text == ")")) {
        return ConfidenceLevel::VERY_HIGH;
    }
    
    if (suggestion.type == FixType::ADD_IMPORT) {
        return ConfidenceLevel::HIGH;
    }
    
    return ConfidenceLevel::MEDIUM;
}

std::string CompilerAgentProtocol::generate_error_code(const TypeError& error) {
    // Generate error codes based on error type
    if (error.message.find("Undefined variable") != std::string::npos) {
        return "E101";
    }
    if (error.message.find("type") != std::string::npos && 
        error.message.find("mismatch") != std::string::npos) {
        return "E102";
    }
    if (error.message.find("Unknown type") != std::string::npos) {
        return "E103";
    }
    if (error.message.find("Parse error") != std::string::npos) {
        return "E002";
    }
    
    return "E999"; // Generic error
}

Location CompilerAgentProtocol::error_to_location(const TypeError& error) {
    return Location(
        error.file,
        error.line,
        error.column,
        error.line,
        error.column + 1 // Assume single character for now
    );
}

std::string CompilerAgentProtocol::read_file_content(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + file_path.string());
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

size_t CompilerAgentProtocol::count_lines(const std::string& content) {
    return std::count(content.begin(), content.end(), '\n') + 1;
}

size_t CompilerAgentProtocol::count_ast_nodes(const std::vector<parser::ast::expression>& ast) {
    // Simplified node counting - in a real implementation this would traverse the AST
    return ast.size();
}

// Utility functions
nlohmann::json confidence_to_json(ConfidenceLevel confidence) {
    return static_cast<int>(confidence);
}

ConfidenceLevel confidence_from_json(const nlohmann::json& j) {
    return static_cast<ConfidenceLevel>(j.get<int>());
}

nlohmann::json severity_to_json(MessageSeverity severity) {
    switch (severity) {
        case MessageSeverity::ERROR: return "error";
        case MessageSeverity::WARNING: return "warning";
        case MessageSeverity::INFO: return "info";
        case MessageSeverity::HINT: return "hint";
        default: return "unknown";
    }
}

MessageSeverity severity_from_json(const nlohmann::json& j) {
    std::string severity_str = j.get<std::string>();
    if (severity_str == "error") return MessageSeverity::ERROR;
    if (severity_str == "warning") return MessageSeverity::WARNING;
    if (severity_str == "info") return MessageSeverity::INFO;
    if (severity_str == "hint") return MessageSeverity::HINT;
    return MessageSeverity::ERROR; // Default
}

nlohmann::json fix_type_to_json(FixType type) {
    switch (type) {
        case FixType::REPLACE_TEXT: return "replace_text";
        case FixType::INSERT_TEXT: return "insert_text";
        case FixType::DELETE_TEXT: return "delete_text";
        case FixType::ADD_IMPORT: return "add_import";
        case FixType::RENAME_SYMBOL: return "rename_symbol";
        case FixType::ADD_TYPE_ANNOTATION: return "add_type_annotation";
        case FixType::EXTRACT_FUNCTION: return "extract_function";
        case FixType::INLINE_VARIABLE: return "inline_variable";
        default: return "unknown";
    }
}

FixType fix_type_from_json(const nlohmann::json& j) {
    std::string type_str = j.get<std::string>();
    if (type_str == "replace_text") return FixType::REPLACE_TEXT;
    if (type_str == "insert_text") return FixType::INSERT_TEXT;
    if (type_str == "delete_text") return FixType::DELETE_TEXT;
    if (type_str == "add_import") return FixType::ADD_IMPORT;
    if (type_str == "rename_symbol") return FixType::RENAME_SYMBOL;
    if (type_str == "add_type_annotation") return FixType::ADD_TYPE_ANNOTATION;
    if (type_str == "extract_function") return FixType::EXTRACT_FUNCTION;
    if (type_str == "inline_variable") return FixType::INLINE_VARIABLE;
    return FixType::REPLACE_TEXT; // Default
}

// Helper method implementations

// Effect analysis helpers
std::vector<EffectInfo> CompilerAgentProtocol::extract_effect_info(const std::vector<parser::ast::expression>& ast, 
                                                                   const std::string& file_name) {
    std::vector<EffectInfo> effect_infos;
    
    for (const auto& expr : ast) {
        try {
            auto info = analyze_function_effects(expr);
            if (!info.function_name.empty()) {
                effect_infos.push_back(std::move(info));
            }
        } catch (const std::exception&) {
            // Skip functions that can't be analyzed
        }
    }
    
    return effect_infos;
}

EffectInfo CompilerAgentProtocol::analyze_function_effects(const parser::ast::expression& function_ast) {
    // Simplified implementation - in practice, this would traverse the AST
    // to find function declarations and analyze their effect usage
    
    EffectInfo info("example_function"); // Placeholder
    
    // Mock effect analysis
    info.inferred_effects = {"FileSystem", "Network"};
    info.declared_effects = {"FileSystem"};
    info.effects_match = false; // Mismatch detected
    info.effect_annotation = "@uses(FileSystem)";
    
    return info;
}

std::vector<std::string> CompilerAgentProtocol::infer_effects_from_body(const parser::ast::expression& body) {
    std::vector<std::string> effects;
    
    // Simplified implementation - would analyze function calls in body
    // to determine which effects are performed
    
    // Mock inference
    effects.push_back("FileSystem");
    effects.push_back("Network");
    
    return effects;
}

// ASG query helpers
ASGQueryResult CompilerAgentProtocol::execute_asg_query(const std::string& pattern, const std::string& query_type,
                                                       std::shared_ptr<api::SemanticGraph> graph) {
    ASGQueryResult result(query_type, pattern);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (query_type == "symbol") {
            // Find symbols matching pattern
            auto usage_edges = api::SemanticGraphAPI::findUsage(*graph, pattern);
            for (const auto& edge : usage_edges) {
                result.matching_nodes.push_back(edge.node_id);
                result.node_descriptions.push_back(
                    "Symbol usage: " + edge.context
                );
            }
        } else if (query_type == "function") {
            // Find function declarations
            auto function_nodes = api::SemanticGraphAPI::findFunctions(*graph);
            for (auto node_id : function_nodes) {
                result.matching_nodes.push_back(node_id);
                result.node_descriptions.push_back(describe_node(node_id, graph));
            }
        } else if (query_type == "variable") {
            // Find variable declarations
            auto variable_nodes = api::SemanticGraphAPI::findVariables(*graph);
            for (auto node_id : variable_nodes) {
                result.matching_nodes.push_back(node_id);
                result.node_descriptions.push_back(describe_node(node_id, graph));
            }
        }
    } catch (const std::exception& e) {
        result.node_descriptions.push_back("Query error: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.query_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

std::string CompilerAgentProtocol::describe_node(api::NodeId node_id, std::shared_ptr<api::SemanticGraph> graph) {
    try {
        auto node = graph->getNode(node_id);
        if (node) {
            return "Node " + std::to_string(node_id) + ": " + std::string("node_" + std::to_string(node->getId()));
        }
    } catch (const std::exception&) {
        // Fall through to default
    }
    
    return "Node " + std::to_string(node_id) + ": <unknown>";
}

// MELD-B helpers
std::string CompilerAgentProtocol::generate_meld_b_path(const std::string& source_file) {
    std::filesystem::path source_path(source_file);
    auto output_path = source_path;
    output_path.replace_extension(".mldb");
    return output_path.string();
}

void CompilerAgentProtocol::save_meld_binary(std::shared_ptr<api::SemanticGraph> graph, const std::string& output_path) {
    auto meld_binary = api::MeldBinary::fromSemanticGraph(graph);
    if (meld_binary) {
        meld_binary->save(output_path);
    } else {
        throw std::runtime_error("Failed to create MELD-B from semantic graph");
    }
}

// v2.0 utility functions
nlohmann::json origin_type_to_json(provenance::OriginType origin) {
    switch (origin) {
        case provenance::OriginType::Human: return "human";
        case provenance::OriginType::Agent: return "agent";
        case provenance::OriginType::Verified: return "verified";
        default: return "unknown";
    }
}

provenance::OriginType origin_type_from_json(const nlohmann::json& j) {
    std::string origin_str = j.get<std::string>();
    if (origin_str == "human") return provenance::OriginType::Human;
    if (origin_str == "agent") return provenance::OriginType::Agent;
    if (origin_str == "verified") return provenance::OriginType::Verified;
    return provenance::OriginType::Human; // Default
}

nlohmann::json export_format_to_json(ide::ExportFormat format) {
    switch (format) {
        case ide::ExportFormat::MERMAID: return "mermaid";
        case ide::ExportFormat::DOT: return "dot";
        case ide::ExportFormat::JSON: return "json";
        case ide::ExportFormat::SVG: return "svg";
        case ide::ExportFormat::PLANTUML: return "plantuml";
        default: return "json";
    }
}

ide::ExportFormat export_format_from_json(const nlohmann::json& j) {
    std::string format_str = j.get<std::string>();
    if (format_str == "mermaid") return ide::ExportFormat::MERMAID;
    if (format_str == "dot") return ide::ExportFormat::DOT;
    if (format_str == "json") return ide::ExportFormat::JSON;
    if (format_str == "svg") return ide::ExportFormat::SVG;
    if (format_str == "plantuml") return ide::ExportFormat::PLANTUML;
    return ide::ExportFormat::JSON; // Default
}

// v2.0 AI-native feature implementations

// Provenance analysis methods
std::vector<ProvenanceInfo> CompilerAgentProtocol::analyze_provenance(const std::filesystem::path& file_path) {
    try {
        std::string source_code = read_file_content(file_path);
        
        std::vector<parser::ast::expression> ast;
        bool parse_success = parser_->parse_file(source_code, ast);
        
        if (!parse_success) {
            return {}; // Return empty if parsing fails
        }
        
        return extract_provenance_info(ast, file_path.string());
    } catch (const std::exception&) {
        return {};
    }
}

ProvenanceInfo CompilerAgentProtocol::get_node_provenance(const std::string& node_id) {
    ProvenanceInfo info(node_id, provenance::OriginType::Human);
    
    if (!current_graph_) {
        return info;
    }
    
    try {
        // Find node in current graph and extract its provenance metadata
        auto node = current_graph_->getNode(std::stoull(node_id));
        if (node) {
            // Extract provenance information from node metadata (simplified)
            info.origin = provenance::OriginType::Agent; // Placeholder
            info.trust_score = 0.8; // Placeholder
            info.author_email = "ai-agent@example.com";
            info.agent_model = "gpt-4";
            info.confidence_score = 0.85;
        }
    } catch (const std::exception&) {
        // Use default values
    }
    
    return info;
}

std::vector<ProvenanceInfo> CompilerAgentProtocol::query_by_trust_level(double min_trust_level) {
    std::vector<ProvenanceInfo> results;
    
    if (!current_graph_) {
        return results;
    }
    
    try {
        // Query all nodes and filter by trust level
        auto all_nodes = current_graph_->getAllNodes();
        
        for (const auto& node : all_nodes) {
            auto provenance_info = get_node_provenance(std::to_string(node->getId()));
            if (provenance_info.trust_score >= min_trust_level) {
                results.push_back(std::move(provenance_info));
            }
        }
    } catch (const std::exception&) {
        // Return empty results on error
    }
    
    return results;
}

std::vector<ProvenanceInfo> CompilerAgentProtocol::query_by_origin(provenance::OriginType origin) {
    std::vector<ProvenanceInfo> results;
    
    if (!current_graph_) {
        return results;
    }
    
    try {
        // Query all nodes and filter by origin type
        auto all_nodes = current_graph_->getAllNodes();
        
        for (const auto& node : all_nodes) {
            auto provenance_info = get_node_provenance(std::to_string(node->getId()));
            if (provenance_info.origin == origin) {
                results.push_back(std::move(provenance_info));
            }
        }
    } catch (const std::exception&) {
        // Return empty results on error
    }
    
    return results;
}

// Flow visualization methods
std::vector<FlowVisualizationInfo> CompilerAgentProtocol::extract_flow_visualizations(const std::filesystem::path& file_path) {
    try {
        std::string source_code = read_file_content(file_path);
        
        std::vector<parser::ast::expression> ast;
        bool parse_success = parser_->parse_file(source_code, ast);
        
        if (!parse_success) {
            return {}; // Return empty if parsing fails
        }
        
        return extract_flow_definitions(ast);
    } catch (const std::exception&) {
        return {};
    }
}

FlowVisualizationInfo CompilerAgentProtocol::generate_flow_visualization(const std::string& flow_name, 
                                                                        ide::ExportFormat format) {
    std::string format_str = export_format_to_json(format);
    FlowVisualizationInfo info(flow_name, format_str);
    
    try {
        // Generate visualization data based on format
        if (format == ide::ExportFormat::MERMAID) {
            info.visualization_data = generate_mermaid_flow(flow_name);
        } else if (format == ide::ExportFormat::DOT) {
            info.visualization_data = generate_dot_flow(flow_name);
        } else if (format == ide::ExportFormat::JSON) {
            info.visualization_data = generate_json_flow(flow_name);
        } else if (format == ide::ExportFormat::SVG) {
            info.visualization_data = generate_svg_flow(flow_name);
        } else if (format == ide::ExportFormat::PLANTUML) {
            info.visualization_data = generate_plantuml_flow(flow_name);
        }
        
        // Add metadata
        info.metadata["generated_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        info.metadata["format"] = format_str;
        
    } catch (const std::exception& e) {
        info.visualization_data = "Error generating visualization: " + std::string(e.what());
    }
    
    return info;
}

// Flight recorder methods
std::optional<ExecutionSnapshot> CompilerAgentProtocol::get_last_crash_snapshot() const {
    return last_crash_snapshot_;
}

void CompilerAgentProtocol::attach_crash_snapshot(CompilationResult& result, const ExecutionSnapshot& snapshot) {
    result.crash_snapshot = snapshot;
    last_crash_snapshot_ = snapshot;
}

// Private helper method implementations

// Provenance analysis helpers
std::vector<ProvenanceInfo> CompilerAgentProtocol::extract_provenance_info(const std::vector<parser::ast::expression>& ast, 
                                                                          const std::string& file_name) {
    std::vector<ProvenanceInfo> provenance_infos;
    
    for (size_t i = 0; i < ast.size(); ++i) {
        try {
            auto info = analyze_node_provenance(ast[i]);
            if (!info.node_id.empty()) {
                provenance_infos.push_back(std::move(info));
            }
        } catch (const std::exception&) {
            // Skip nodes that can't be analyzed
        }
    }
    
    return provenance_infos;
}

ProvenanceInfo CompilerAgentProtocol::analyze_node_provenance(const parser::ast::expression& node_ast) {
    // Generate a unique node ID (simplified implementation)
    std::string node_id = "node_" + std::to_string(std::hash<std::string>{}(ast_to_string(node_ast)));
    
    ProvenanceInfo info(node_id, provenance::OriginType::Human);
    
    // Mock provenance analysis - in practice, this would extract metadata from the AST
    info.trust_score = 0.9;
    info.author_email = "developer@example.com";
    
    // Detect potential mismatches
    info.mismatch_warnings = detect_provenance_mismatches(node_ast);
    
    return info;
}

std::vector<std::string> CompilerAgentProtocol::detect_provenance_mismatches(const parser::ast::expression& node_ast) {
    std::vector<std::string> warnings;
    
    // Simplified mismatch detection - in practice, this would analyze:
    // - Blueprint vs code consistency
    // - Trust level requirements
    // - Verification expiry
    
    // Mock warning generation
    std::string node_str = ast_to_string(node_ast);
    if (node_str.find("@blueprint") != std::string::npos && 
        node_str.find("TODO") != std::string::npos) {
        warnings.push_back("Blueprint annotation present but implementation contains TODO");
    }
    
    return warnings;
}

// Flow visualization helpers
std::vector<FlowVisualizationInfo> CompilerAgentProtocol::extract_flow_definitions(const std::vector<parser::ast::expression>& ast) {
    std::vector<FlowVisualizationInfo> flow_infos;
    
    for (const auto& expr : ast) {
        try {
            // Look for flow definitions in the AST
            std::string expr_str = ast_to_string(expr);
            if (expr_str.find("flow") != std::string::npos) {
                // Extract flow name (simplified)
                std::string flow_name = "extracted_flow_" + std::to_string(flow_infos.size());
                
                FlowVisualizationInfo info(flow_name, "json");
                info.visualization_data = generate_json_flow(flow_name);
                info.state_names = {"start", "processing", "end"};
                info.transition_descriptions = {"start -> processing", "processing -> end"};
                
                flow_infos.push_back(std::move(info));
            }
        } catch (const std::exception&) {
            // Skip expressions that can't be analyzed
        }
    }
    
    return flow_infos;
}

FlowVisualizationInfo CompilerAgentProtocol::create_flow_visualization(const parser::ast::flow_definition& flow_def,
                                                                      ide::ExportFormat format) {
    std::string format_str = export_format_to_json(format);
    FlowVisualizationInfo info("flow_from_ast", format_str);
    
    try {
        // Extract flow information from AST (simplified)
        info.state_names = {"initial", "active", "final"};
        info.transition_descriptions = {"initial -> active", "active -> final"};
        
        // Generate visualization based on format
        if (format == ide::ExportFormat::MERMAID) {
            info.visualization_data = generate_mermaid_flow("flow_from_ast");
        } else if (format == ide::ExportFormat::JSON) {
            info.visualization_data = generate_json_flow("flow_from_ast");
        }
        // Add other formats as needed
        
    } catch (const std::exception& e) {
        info.visualization_data = "Error creating visualization: " + std::string(e.what());
    }
    
    return info;
}

// Flight recorder helpers
ExecutionSnapshot CompilerAgentProtocol::create_crash_snapshot(const std::string& function_name,
                                                              const std::string& error_message,
                                                              const std::string& error_type) {
    std::string snapshot_id = "crash_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    
    ExecutionSnapshot snapshot(snapshot_id, function_name);
    snapshot.error_message = error_message;
    snapshot.error_type = error_type;
    
    // Capture execution context
    capture_execution_context(snapshot);
    
    return snapshot;
}

void CompilerAgentProtocol::capture_execution_context(ExecutionSnapshot& snapshot) {
    // Mock execution context capture - in practice, this would:
    // - Capture call stack
    // - Capture local variables
    // - Capture effect history
    // - Capture environment metadata
    
    snapshot.call_stack = {
        "main()",
        "compile_file()",
        "compile_internal()",
        snapshot.function_name + "()"
    };
    
    snapshot.local_variables = {
        {"file_path", "/path/to/source.meld"},
        {"line_number", "42"},
        {"column", "15"}
    };
    
    snapshot.effect_history = {
        "FileSystem.read(/path/to/source.meld)",
        "Parser.parse(source_code)",
        "TypeChecker.check(ast)"
    };
    
    snapshot.environment_metadata = {
        {"compiler_version", "2.0.0"},
        {"target_platform", "cpp"},
        {"optimization_level", "debug"}
    };
}

// Flow visualization format generators (simplified implementations)
std::string CompilerAgentProtocol::generate_mermaid_flow(const std::string& flow_name) {
    return "flowchart TD\n"
           "    A[Start] --> B[Processing]\n"
           "    B --> C[End]\n"
           "    style A fill:#e1f5fe\n"
           "    style C fill:#c8e6c9";
}

std::string CompilerAgentProtocol::generate_dot_flow(const std::string& flow_name) {
    return "digraph " + flow_name + " {\n"
           "    rankdir=TD;\n"
           "    start -> processing;\n"
           "    processing -> end;\n"
           "}";
}

std::string CompilerAgentProtocol::generate_json_flow(const std::string& flow_name) {
    return "{\n"
           "  \"name\": \"" + flow_name + "\",\n"
           "  \"nodes\": [\n"
           "    {\"id\": \"start\", \"label\": \"Start\", \"type\": \"initial\"},\n"
           "    {\"id\": \"processing\", \"label\": \"Processing\", \"type\": \"state\"},\n"
           "    {\"id\": \"end\", \"label\": \"End\", \"type\": \"final\"}\n"
           "  ],\n"
           "  \"edges\": [\n"
           "    {\"from\": \"start\", \"to\": \"processing\"},\n"
           "    {\"from\": \"processing\", \"to\": \"end\"}\n"
           "  ]\n"
           "}";
}

std::string CompilerAgentProtocol::generate_svg_flow(const std::string& flow_name) {
    return "<svg width=\"200\" height=\"150\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           "  <rect x=\"10\" y=\"10\" width=\"60\" height=\"30\" fill=\"lightblue\" stroke=\"black\"/>\n"
           "  <text x=\"40\" y=\"30\" text-anchor=\"middle\">Start</text>\n"
           "  <rect x=\"10\" y=\"60\" width=\"60\" height=\"30\" fill=\"lightgreen\" stroke=\"black\"/>\n"
           "  <text x=\"40\" y=\"80\" text-anchor=\"middle\">End</text>\n"
           "  <line x1=\"40\" y1=\"40\" x2=\"40\" y2=\"60\" stroke=\"black\" marker-end=\"url(#arrowhead)\"/>\n"
           "</svg>";
}

std::string CompilerAgentProtocol::generate_plantuml_flow(const std::string& flow_name) {
    return "@startuml\n"
           "start\n"
           ":Processing;\n"
           "stop\n"
           "@enduml";
}

// AST utility methods
std::string CompilerAgentProtocol::ast_to_string(const parser::ast::expression& expr) {
    // Simplified AST to string conversion
    // In a real implementation, this would use a proper AST visitor
    return "ast_node_" + std::to_string(reinterpret_cast<uintptr_t>(&expr));
}

} // namespace meld::compiler::cap