#pragma once

#include "meld/compiler/type_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/parser.hpp"
#include "meld/api/semantic_graph.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/ide/flow_visualizer.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

namespace meld::api { class MeldBinary; }
#include <chrono>

namespace meld::compiler::cap {

// Forward declarations
struct FixSuggestion;
struct CompilationMessage;
struct CompilationResult;
struct BatchCompilationResult;
struct IncrementalChange;
struct ASGQueryResult;

// Confidence level for fix suggestions
enum class ConfidenceLevel {
    LOW = 1,      // 0-40% confidence
    MEDIUM = 2,   // 40-70% confidence  
    HIGH = 3,     // 70-90% confidence
    VERY_HIGH = 4 // 90-100% confidence
};

// Message severity levels
enum class MessageSeverity {
    ERROR,
    WARNING,
    INFO,
    HINT
};

// Fix suggestion types
enum class FixType {
    REPLACE_TEXT,     // Replace text at specific location
    INSERT_TEXT,      // Insert text at specific location
    DELETE_TEXT,      // Delete text at specific location
    ADD_IMPORT,       // Add import statement
    RENAME_SYMBOL,    // Rename a symbol
    ADD_TYPE_ANNOTATION, // Add type annotation
    EXTRACT_FUNCTION, // Extract code into function
    INLINE_VARIABLE   // Inline variable usage
};

// Location information for errors and fixes
struct Location {
    std::string file;
    size_t line;
    size_t column;
    size_t end_line;
    size_t end_column;
    
    Location(std::string f = "", size_t l = 0, size_t c = 0, size_t el = 0, size_t ec = 0)
        : file(std::move(f)), line(l), column(c), end_line(el), end_column(ec) {}
    
    nlohmann::json to_json() const;
    static Location from_json(const nlohmann::json& j);
};

// Fix suggestion with confidence score
struct FixSuggestion {
    FixType type;
    std::string description;
    Location location;
    std::string replacement_text;
    ConfidenceLevel confidence;
    std::vector<std::string> additional_changes; // Related changes needed
    
    FixSuggestion(FixType t, std::string desc, Location loc, std::string text, ConfidenceLevel conf)
        : type(t), description(std::move(desc)), location(std::move(loc)), 
          replacement_text(std::move(text)), confidence(conf) {}
    
    nlohmann::json to_json() const;
    static FixSuggestion from_json(const nlohmann::json& j);
};

// Compilation message (error, warning, info, hint)
struct CompilationMessage {
    MessageSeverity severity;
    std::string code;        // Error/warning code (e.g., "E001", "W042")
    std::string message;
    Location location;
    std::string context;     // Additional context information
    std::vector<FixSuggestion> suggestions;
    std::vector<Location> related_locations; // Related locations for multi-location errors
    
    CompilationMessage(MessageSeverity sev, std::string c, std::string msg, Location loc)
        : severity(sev), code(std::move(c)), message(std::move(msg)), location(std::move(loc)) {}
    
    nlohmann::json to_json() const;
    static CompilationMessage from_json(const nlohmann::json& j);
};

// Effect information for functions
struct EffectInfo {
    std::string function_name;
    std::vector<std::string> declared_effects;
    std::vector<std::string> inferred_effects;
    bool effects_match;
    std::string effect_annotation;
    EffectInfo(std::string name) : function_name(std::move(name)), effects_match(true) {}
    nlohmann::json to_json() const;
    static EffectInfo from_json(const nlohmann::json& j);
};

struct ExecutionSnapshot {
    std::string snapshot_id;
    std::chrono::system_clock::time_point timestamp;
    std::string function_name;
    std::vector<std::string> function_inputs;
    std::vector<std::string> effect_history;
    std::vector<std::string> call_stack;
    std::map<std::string, std::string> local_variables;
    std::map<std::string, std::string> environment_metadata;
    std::string error_message;
    std::string error_type;
    ExecutionSnapshot(std::string id, std::string func_name)
        : snapshot_id(std::move(id))
        , timestamp(std::chrono::system_clock::now())
        , function_name(std::move(func_name)) {}
    nlohmann::json to_json() const;
    static ExecutionSnapshot from_json(const nlohmann::json& j);
};

struct FlowVisualizationInfo {
    std::string flow_name;
    std::string visualization_format;
    std::string visualization_data;
    std::vector<std::string> state_names;
    std::vector<std::string> transition_descriptions;
    std::map<std::string, std::string> metadata;
    FlowVisualizationInfo(std::string name, std::string format)
        : flow_name(std::move(name)), visualization_format(std::move(format)) {}
    nlohmann::json to_json() const;
    static FlowVisualizationInfo from_json(const nlohmann::json& j);
};

struct ProvenanceInfo {
    std::string node_id;
    provenance::OriginType origin;
    std::chrono::system_clock::time_point creation_timestamp;
    std::optional<std::string> author_email;
    std::optional<std::string> agent_model;
    std::optional<double> confidence_score;
    std::optional<std::string> reviewer_email;
    std::optional<std::chrono::system_clock::time_point> verification_timestamp;
    double trust_score;
    std::vector<std::string> mismatch_warnings;
    ProvenanceInfo(std::string id, provenance::OriginType orig)
        : node_id(std::move(id)), origin(orig)
        , creation_timestamp(std::chrono::system_clock::now())
        , trust_score(0.0) {}
    nlohmann::json to_json() const;
    static ProvenanceInfo from_json(const nlohmann::json& j);
};

struct ASGQueryResult {
    std::string query_type;
    std::string pattern;
    std::vector<api::NodeId> matching_nodes;
    std::vector<std::string> node_descriptions;
    std::chrono::milliseconds query_time;
    ASGQueryResult(std::string type, std::string pat) 
        : query_type(std::move(type)), pattern(std::move(pat)), query_time(0) {}
    nlohmann::json to_json() const;
    static ASGQueryResult from_json(const nlohmann::json& j);
};

// Single file compilation result
struct CompilationResult {
    std::string file_path;
    bool success;
    std::vector<CompilationMessage> messages;
    std::optional<std::string> generated_code;
    std::optional<std::string> output_file;
    std::chrono::milliseconds compilation_time;
    
    // Metadata
    size_t lines_of_code;
    size_t ast_node_count;
    std::vector<std::string> dependencies;
    std::vector<EffectInfo> effect_information;
    std::optional<std::string> meld_b_file; // Path to generated MELD-B file
    
    // v2.0 AI-native features
    std::vector<ProvenanceInfo> provenance_information;
    std::vector<FlowVisualizationInfo> flow_visualizations;
    std::optional<ExecutionSnapshot> crash_snapshot; // Only present if compilation crashed
    
    CompilationResult(std::string path) 
        : file_path(std::move(path)), success(false), compilation_time(0), 
          lines_of_code(0), ast_node_count(0) {}
    
    bool has_errors() const;
    bool has_warnings() const;
    std::vector<CompilationMessage> get_errors() const;
    std::vector<CompilationMessage> get_warnings() const;
    
    nlohmann::json to_json() const;
    static CompilationResult from_json(const nlohmann::json& j);
};

// Batch compilation result for multiple files
struct BatchCompilationResult {
    std::vector<CompilationResult> results;
    bool overall_success;
    std::chrono::milliseconds total_time;
    
    // Summary statistics
    size_t total_files;
    size_t successful_files;
    size_t total_errors;
    size_t total_warnings;
    
    BatchCompilationResult() 
        : overall_success(false), total_time(0), total_files(0), 
          successful_files(0), total_errors(0), total_warnings(0) {}
    
    void add_result(CompilationResult result);
    void finalize();
    
    nlohmann::json to_json() const;
    static BatchCompilationResult from_json(const nlohmann::json& j);
};

// Incremental compilation change tracking
struct IncrementalChange {
    enum class ChangeType {
        FILE_ADDED,
        FILE_MODIFIED,
        FILE_DELETED,
        DEPENDENCY_CHANGED
    };
    
    ChangeType type;
    std::string file_path;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> old_content_hash;
    std::optional<std::string> new_content_hash;
    
    IncrementalChange(ChangeType t, std::string path)
        : type(t), file_path(std::move(path)), timestamp(std::chrono::system_clock::now()) {}
    
    nlohmann::json to_json() const;
    static IncrementalChange from_json(const nlohmann::json& j);
};

// Main CAP interface
class CompilerAgentProtocol {
public:
    CompilerAgentProtocol();
    
    // Single file compilation with structured output
    CompilationResult compile_file(const std::filesystem::path& file_path);
    CompilationResult compile_source(const std::string& source_code, 
                                   const std::string& file_name = "<source>");
    
    // Batch compilation
    BatchCompilationResult compile_batch(const std::vector<std::filesystem::path>& files);
    
    // Incremental compilation
    BatchCompilationResult compile_incremental(const std::vector<IncrementalChange>& changes);
    
    // MELD-B support (Requirement 40.10)
    CompilationResult compile_to_meld_b(const std::filesystem::path& file_path, 
                                       const std::filesystem::path& output_path = "");
    std::unique_ptr<api::MeldBinary> get_meld_binary(const CompilationResult& result);
    
    // ASG queries (Requirement 40.10)
    ASGQueryResult query_asg(const std::string& pattern, const std::string& query_type = "symbol");
    ASGQueryResult query_by_effect(const std::string& effect_name);
    ASGQueryResult query_usage(const std::string& symbol);
    std::vector<ASGQueryResult> batch_query_asg(const std::vector<std::pair<std::string, std::string>>& queries);
    
    // Effect analysis
    std::vector<EffectInfo> analyze_effects(const std::filesystem::path& file_path);
    EffectInfo get_function_effects(const std::string& function_name);
    
    // v2.0 AI-native features
    
    // Provenance analysis (Requirement 45)
    std::vector<ProvenanceInfo> analyze_provenance(const std::filesystem::path& file_path);
    ProvenanceInfo get_node_provenance(const std::string& node_id);
    std::vector<ProvenanceInfo> query_by_trust_level(double min_trust_level);
    std::vector<ProvenanceInfo> query_by_origin(provenance::OriginType origin);
    
    // Flow visualization (Requirement 43.9)
    std::vector<FlowVisualizationInfo> extract_flow_visualizations(const std::filesystem::path& file_path);
    FlowVisualizationInfo generate_flow_visualization(const std::string& flow_name, 
                                                     ide::ExportFormat format = ide::ExportFormat::JSON);
    
    // Flight recorder integration (Requirement 44.9)
    void set_enable_crash_snapshots(bool enable) { enable_crash_snapshots_ = enable; }
    std::optional<ExecutionSnapshot> get_last_crash_snapshot() const;
    void attach_crash_snapshot(CompilationResult& result, const ExecutionSnapshot& snapshot);
    
    // Configuration
    void set_include_suggestions(bool enable) { include_suggestions_ = enable; }
    void set_max_suggestions_per_error(size_t max) { max_suggestions_per_error_ = max; }
    void set_confidence_threshold(ConfidenceLevel threshold) { confidence_threshold_ = threshold; }
    void set_generate_meld_b(bool enable) { generate_meld_b_ = enable; }
    void set_include_effect_info(bool enable) { include_effect_info_ = enable; }
    void set_enable_asg_queries(bool enable) { enable_asg_queries_ = enable; }
    
    // v2.0 configuration
    void set_include_provenance_info(bool enable) { include_provenance_info_ = enable; }
    void set_include_flow_visualizations(bool enable) { include_flow_visualizations_ = enable; }
    void set_min_trust_level(double min_trust) { min_trust_level_ = min_trust; }
    
    // JSON serialization helpers
    static std::string to_json_string(const CompilationResult& result, bool pretty = false);
    static std::string to_json_string(const BatchCompilationResult& result, bool pretty = false);
    static std::string to_json_string(const ASGQueryResult& result, bool pretty = false);
    static std::string to_json_string(const EffectInfo& effect_info, bool pretty = false);
    
    // v2.0 JSON serialization helpers
    static std::string to_json_string(const ProvenanceInfo& provenance_info, bool pretty = false);
    static std::string to_json_string(const FlowVisualizationInfo& flow_info, bool pretty = false);
    static std::string to_json_string(const ExecutionSnapshot& snapshot, bool pretty = false);
    
private:
    // Core compilation logic
    CompilationResult compile_internal(const std::string& source_code, const std::string& file_name);
    
    // Error processing and suggestion generation
    std::vector<CompilationMessage> process_type_errors(const std::vector<TypeError>& errors, 
                                                       const std::string& file_name);
    std::vector<FixSuggestion> generate_suggestions(const TypeError& error);
    
    // Specific suggestion generators
    std::vector<FixSuggestion> suggest_undefined_variable_fixes(const TypeError& error);
    std::vector<FixSuggestion> suggest_type_mismatch_fixes(const TypeError& error);
    std::vector<FixSuggestion> suggest_missing_import_fixes(const TypeError& error);
    std::vector<FixSuggestion> suggest_syntax_error_fixes(const TypeError& error);
    
    // Confidence scoring
    ConfidenceLevel calculate_confidence(const FixSuggestion& suggestion, const TypeError& error);
    
    // Effect analysis helpers
    std::vector<EffectInfo> extract_effect_info(const std::vector<parser::ast::expression>& ast, 
                                               const std::string& file_name);
    EffectInfo analyze_function_effects(const parser::ast::expression& function_ast);
    std::vector<std::string> infer_effects_from_body(const parser::ast::expression& body);
    
    // ASG query helpers
    ASGQueryResult execute_asg_query(const std::string& pattern, const std::string& query_type,
                                    std::shared_ptr<api::SemanticGraph> graph);
    std::string describe_node(api::NodeId node_id, std::shared_ptr<api::SemanticGraph> graph);
    
    // MELD-B helpers
    std::string generate_meld_b_path(const std::string& source_file);
    void save_meld_binary(std::shared_ptr<api::SemanticGraph> graph, const std::string& output_path);
    
    // v2.0 helper methods
    
    // Provenance analysis helpers
    std::vector<ProvenanceInfo> extract_provenance_info(const std::vector<parser::ast::expression>& ast,
                                                       const std::string& file_name);
    ProvenanceInfo analyze_node_provenance(const parser::ast::expression& node_ast);
    std::vector<std::string> detect_provenance_mismatches(const parser::ast::expression& node_ast);
    
    // Flow visualization helpers
    std::vector<FlowVisualizationInfo> extract_flow_definitions(const std::vector<parser::ast::expression>& ast);
    FlowVisualizationInfo create_flow_visualization(const parser::ast::flow_definition& flow_def,
                                                   ide::ExportFormat format);
    
    // Flight recorder helpers
    ExecutionSnapshot create_crash_snapshot(const std::string& function_name,
                                          const std::string& error_message,
                                          const std::string& error_type);
    void capture_execution_context(ExecutionSnapshot& snapshot);
    
    // Flow visualization format generators
    std::string generate_mermaid_flow(const std::string& flow_name);
    std::string generate_dot_flow(const std::string& flow_name);
    std::string generate_json_flow(const std::string& flow_name);
    std::string generate_svg_flow(const std::string& flow_name);
    std::string generate_plantuml_flow(const std::string& flow_name);
    
    // AST utility methods
    std::string ast_to_string(const parser::ast::expression& expr);
    
    // Utility methods
    std::string generate_error_code(const TypeError& error);
    Location error_to_location(const TypeError& error);
    std::string read_file_content(const std::filesystem::path& file_path);
    size_t count_lines(const std::string& content);
    size_t count_ast_nodes(const std::vector<parser::ast::expression>& ast);
    
    // Configuration
    bool include_suggestions_;
    size_t max_suggestions_per_error_;
    ConfidenceLevel confidence_threshold_;
    bool generate_meld_b_;
    bool include_effect_info_;
    bool enable_asg_queries_;
    
    // v2.0 configuration
    bool include_provenance_info_;
    bool include_flow_visualizations_;
    bool enable_crash_snapshots_;
    double min_trust_level_;
    
    // Dependencies
    std::unique_ptr<TypeChecker> type_checker_;
    std::unique_ptr<parser::Parser> parser_;
    std::shared_ptr<api::SemanticGraph> current_graph_; // For ASG queries
    
    // v2.0 dependencies
    std::optional<ExecutionSnapshot> last_crash_snapshot_;
};

// Utility functions for JSON conversion
nlohmann::json confidence_to_json(ConfidenceLevel confidence);
ConfidenceLevel confidence_from_json(const nlohmann::json& j);

nlohmann::json severity_to_json(MessageSeverity severity);
MessageSeverity severity_from_json(const nlohmann::json& j);

nlohmann::json fix_type_to_json(FixType type);
FixType fix_type_from_json(const nlohmann::json& j);

// v2.0 utility functions
nlohmann::json origin_type_to_json(provenance::OriginType origin);
provenance::OriginType origin_type_from_json(const nlohmann::json& j);

nlohmann::json export_format_to_json(ide::ExportFormat format);
ide::ExportFormat export_format_from_json(const nlohmann::json& j);

} // namespace meld::compiler::cap