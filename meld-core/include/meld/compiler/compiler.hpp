#pragma once

#include "meld/parser/parser.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/mms/meta_system.hpp"
#include "meld/compiler/micro_test_engine.hpp"
#include "meld/compiler/contract_checker.hpp"
#include "meld/compiler/mcp_generator.hpp"
#include "meld/compiler/wasm_backend.hpp"
#include "meld/compiler/codegen.hpp"
#include "meld/compiler/effect_checker.hpp"
#include "meld/compiler/ide_integration.hpp"
#include "meld/compiler/annotation_persister.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/compiler/move_tracking_pass.hpp"
#include "meld/compiler/view_access_pass.hpp"
#include "meld/compiler/container_constraint_pass.hpp"
#include "meld/compiler/cycle_detection_pass.hpp"
#include "meld/compiler/advisory_diagnostics_pass.hpp"
#include "meld/compiler/hold_type_inference_pass.hpp"
#include "meld/compiler/backend_lowering_pass.hpp"
#include "meld/verification/provenance_verification.hpp"
#include "meld/parser/provenance_parser.hpp"
#include "meld/provenance/shadow_history.hpp"
#include "meld/runtime/flight_recorder.hpp"
#include "meld/kernel/primitives.hpp"
#include <vector>
#include <string>
#include <memory>

namespace meld::compiler {

// Compilation diagnostic levels
enum class DiagnosticLevel {
    INFO,
    WARNING,
    ERROR
};

// Compilation diagnostic message
struct Diagnostic {
    DiagnosticLevel level;
    std::string message;
    std::string file_path;
    size_t line = 0;
    size_t column = 0;
    std::string source_context;
    std::vector<std::string> suggestions;

    std::string to_json() const {
        std::string lvl = (level == DiagnosticLevel::ERROR) ? "error" : (level == DiagnosticLevel::WARNING) ? "warning" : "info";
        std::string json = "{\"level\": \"" + lvl + "\", ";
        json += "\"message\": \"" + message + "\", ";
        json += "\"file\": \"" + file_path + "\", ";
        json += "\"line\": " + std::to_string(line) + ", ";
        json += "\"column\": " + std::to_string(column) + ", ";
        json += "\"suggestions\": [";
        for (size_t i = 0; i < suggestions.size(); ++i) {
            json += "\"" + suggestions[i] + "\"";
            if (i + 1 < suggestions.size()) json += ", ";
        }
        json += "]}";
        return json;
    }
};

// Compilation options
struct CompilationOptions {
    bool execute_micro_tests = true;
    bool stop_on_test_failure = false;
    bool verbose_test_output = false;
    bool enable_property_tests = true;
    double test_timeout_ms = 5000.0;
    std::string output_path;
    std::vector<std::string> include_paths;
    
    // Trust level enforcement options
    std::optional<double> minimum_trust_level;  // Minimum trust level required (0.0 to 1.0)
    
    // Code generation target
    CodeGenTarget target = CodeGenTarget::CPP;
    
    // MCP generation options
    bool generate_mcp = false;
    MCPGenerationOptions mcp_options;
    
    // JVM generation options
    bool generate_jvm = false;
    std::string jvm_package_name = "meld.generated";
    std::string jvm_output_directory = "./generated";
    bool jvm_use_modern_java = true;
    int jvm_java_version = 17;
    
    // WebAssembly generation options
    bool generate_wasm = false;
    WasmGenerationOptions wasm_options;
    
    // TASK 35.9: IDE integration options
    bool enable_ide_integration = false;
    bool generate_inlay_hints = true;
    bool track_effect_annotations = true;
    
    // TASK 35.10: Annotation persistence options
    bool persist_annotations_on_save = true;
    bool update_annotations_on_change = true;
    
    // TASK 41.3: v2.0 compilation pipeline options
    bool enable_provenance_tracking = true;
    bool enable_flow_validation = true;
    bool enable_provenance_mismatch_detection = true;
    bool enable_shadow_history_storage = false;  // Opt-in for AI-generated code
    bool enable_flight_recorder = false;  // Opt-in for debugging
    
    // Rust-inspired ownership system options
    bool enable_ownership_checking = false;  // Opt-in for ownership analysis
    bool enable_borrow_checking = false;     // Opt-in for borrow checking
    bool strict_ownership_mode = false;      // Strict mode requires all code to use ownership
    bool ownership_inference = true;         // Automatically infer ownership when possible
    
    // Effects annotation strict mode
    bool strict_no_keywords = false;  // Reject keyword-based effect syntax (error instead of warning)
};

// Compilation result
struct CompilationResult {
    bool success = false;
    std::vector<Diagnostic> diagnostics;
    std::vector<FunctionTestResult> test_results;
    size_t total_functions_compiled = 0;
    size_t total_tests_executed = 0;
    size_t total_test_failures = 0;
    double compilation_time_ms = 0.0;
    double test_execution_time_ms = 0.0;
    
    // MCP generation results
    bool mcp_generated = false;
    std::string mcp_output_path;
    size_t mcp_tools_generated = 0;
    
    // WebAssembly generation results
    bool wasm_generated = false;
    std::string wasm_output_path;
    std::vector<std::string> wasm_files_generated;
    
    // TASK 35.9: IDE integration results
    std::vector<InlayHint> inlay_hints;
    std::vector<EffectAnnotationHint> effect_annotations;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    // TASK 35.10: Annotation persistence results
    bool annotations_persisted = false;
    size_t annotations_added = 0;
    size_t annotations_updated = 0;
    std::vector<std::string> annotation_errors;
    
    // TASK 41.3: v2.0 compilation pipeline results
    bool provenance_tracked = false;
    size_t provenance_nodes_tracked = 0;
    bool flow_validated = false;
    size_t flow_definitions_validated = 0;
    std::vector<std::string> flow_validation_warnings;
    bool provenance_mismatches_detected = false;
    size_t provenance_mismatches_found = 0;
    std::vector<std::string> provenance_mismatch_warnings;
    bool shadow_history_stored = false;
    std::string shadow_history_path;
    bool flight_recorder_enabled = false;
    std::string flight_recorder_snapshot_path;
    
    // Rust-inspired ownership system results
    bool ownership_checked = false;
    size_t ownership_violations_found = 0;
    std::vector<std::string> ownership_errors;
    bool borrow_checked = false;
    size_t borrow_violations_found = 0;
    std::vector<std::string> borrow_errors;
};

// Main Meld compiler
class Compiler {
public:
    Compiler();
    ~Compiler();
    
    // Compile a single source file
    CompilationResult compile_file(
        const std::string& file_path,
        const CompilationOptions& options = {}
    );
    
    // Compile source code from string
    CompilationResult compile_source(
        const std::string& source_code,
        const std::string& source_name = "<string>",
        const CompilationOptions& options = {}
    );
    
    // Compile multiple files
    CompilationResult compile_files(
        const std::vector<std::string>& file_paths,
        const CompilationOptions& options = {}
    );
    
    // Execute micro-tests for a parsed function
    FunctionTestResult execute_function_tests(
        const parser::ast::function_definition& function,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // TASK 35.9: IDE integration methods
    // Analyze source for IDE integration (inlay hints, effect annotations)
    CompilationResult analyze_for_ide(
        const std::string& source_code,
        const std::string& source_name = "<string>",
        const CompilationOptions& options = {}
    );
    
    // Get inlay hints for a source file
    std::vector<InlayHint> get_inlay_hints(const std::string& file_path);
    
    // Get effect annotations for a source file
    std::vector<EffectAnnotationHint> get_effect_annotations(const std::string& file_path);
    
    // Update IDE integration on source change
    void on_source_change(const std::string& file_path, const std::string& new_content);
    
    // TASK 35.10: Annotation persistence methods
    // Persist annotations to source file on save/format
    bool persist_annotations_to_file(const std::string& file_path,
                                    const std::vector<parser::ast::function_definition>& functions,
                                    const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Check if file needs annotation updates
    bool file_needs_annotation_update(const std::string& file_path);
    
    // Update annotations in source file when implementation changes
    bool update_annotations_in_file(const std::string& file_path,
                                   const std::vector<parser::ast::function_definition>& functions,
                                   const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Get last compilation diagnostics
    const std::vector<Diagnostic>& get_diagnostics() const { return last_diagnostics_; }
    
    // Get the intrinsic resolution registry (populated after compilation)
    const IntrinsicResolutionRegistry& get_intrinsic_registry() const { return intrinsic_registry_; }
    
    // Clear diagnostics
    void clear_diagnostics() { last_diagnostics_.clear(); }
    
    // Configuration
    void set_micro_test_config(const MicroTestEngine::Config& config);
    const MicroTestEngine::Config& get_micro_test_config() const;
    
    // MCP generation
    MCPServerConfig generate_mcp_config(
        const std::vector<parser::ast::expression>& expressions,
        const MCPGenerationOptions& options = {}
    );
    
    bool write_mcp_files(
        const MCPServerConfig& config,
        const MCPGenerationOptions& options
    );

private:
    std::unique_ptr<parser::Parser> parser_;
    std::unique_ptr<TypeChecker> type_checker_;
    std::unique_ptr<mms::MetaSystem> meta_system_;
    std::unique_ptr<MicroTestEngine> micro_test_engine_;
    std::unique_ptr<ContractChecker> contract_checker_;
    std::unique_ptr<MCPGenerator> mcp_generator_;
    std::unique_ptr<WasmBackend> wasm_backend_;
    
    // TASK 35.9: IDE integration components
    std::unique_ptr<EffectChecker> effect_checker_;
    std::unique_ptr<IDEIntegration> ide_integration_;
    
    // TASK 35.10: Annotation persistence component
    std::unique_ptr<AnnotationPersister> annotation_persister_;
    
    // TASK 41.3: v2.0 compilation pipeline components
    std::unique_ptr<verification::ProvenanceVerifier> provenance_verifier_;
    std::unique_ptr<parser::ProvenanceParser> provenance_parser_;
    std::unique_ptr<ai::ShadowHistory> shadow_history_;
    std::unique_ptr<runtime::FlightRecorder> flight_recorder_;
    
    // Rust-inspired ownership system components
    std::unique_ptr<BorrowChecker> borrow_checker_;
    std::unique_ptr<OwnershipMetadataManager> ownership_metadata_;
    
    // Hold[T]/View[T] tenancy model — Intrinsic Resolution Pass
    std::unique_ptr<IntrinsicResolutionPass> intrinsic_resolution_pass_;
    IntrinsicResolutionRegistry intrinsic_registry_;
    
    // Hold[T]/View[T] tenancy model — Move Tracking Pass
    std::unique_ptr<MoveTrackingPass> move_tracking_pass_;
    
    // Hold[T]/View[T] tenancy model — View Access Enforcement Pass
    std::unique_ptr<ViewAccessPass> link_access_pass_;
    
    // Hold[T]/View[T] tenancy model — Container Constraint Check Pass
    std::unique_ptr<ContainerConstraintPass> container_constraint_pass_;
    
    // Hold[T]/View[T] tenancy model — Cycle Detection Pass
    std::unique_ptr<CycleDetectionPass> cycle_detection_pass_;
    
    // Hold[T]/View[T] tenancy model — Advisory Diagnostics Pass
    std::unique_ptr<AdvisoryDiagnosticsPass> advisory_diagnostics_pass_;
    
    // Hold[T]/View[T] tenancy model — Hold[T] Type Inference Pass
    std::unique_ptr<HoldTypeInferencePass> own_type_inference_pass_;
    
    // Hold[T]/View[T] tenancy model — C++ Backend Lowering Pass
    std::unique_ptr<BackendLoweringPass> backend_lowering_pass_;
    
    std::vector<Diagnostic> last_diagnostics_;
    
    // Internal compilation steps
    bool parse_source(
        const std::string& source_code,
        const std::string& source_name,
        std::vector<parser::ast::expression>& expressions
    );
    
    bool type_check_expressions(
        const std::vector<parser::ast::expression>& expressions
    );
    
    bool enforce_trust_level(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool execute_micro_tests(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool generate_mcp_output(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool generate_jvm_output(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool generate_wasm_output(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    // TASK 35.9: IDE integration internal methods
    bool analyze_effects_and_generate_hints(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    std::vector<parser::ast::function_definition> extract_function_definitions(
        const std::vector<parser::ast::expression>& expressions
    );
    
    // TASK 35.10: Annotation persistence internal methods
    bool persist_annotations_for_compilation(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    // TASK 41.3: v2.0 compilation pipeline internal methods
    bool track_provenance_for_compilation(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool validate_flow_definitions(
        const std::vector<parser::ast::expression>& expressions,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool detect_provenance_mismatches(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool store_shadow_history(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    bool integrate_flight_recorder(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    // Rust-inspired ownership system internal methods
    bool check_ownership_and_borrowing(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        const CompilationOptions& options,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Intrinsic Resolution Pass
    bool resolve_intrinsic_annotations(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Move Tracking Pass
    bool run_move_tracking(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — View Access Enforcement Pass
    bool run_link_access_enforcement(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Container Constraint Check Pass
    bool run_container_constraint_check(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Cycle Detection Pass
    bool run_cycle_detection(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Advisory Diagnostics Pass
    bool run_advisory_diagnostics(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );

    // @uses static enforcement — pure-by-default effect checking
    void run_uses_enforcement(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — Hold[T] Type Inference Pass
    bool run_own_type_inference(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    // Hold[T]/View[T] tenancy model — C++ Backend Lowering Pass
    bool run_backend_lowering(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_name,
        CompilationResult& result
    );
    
    void add_diagnostic(
        DiagnosticLevel level,
        const std::string& message,
        const std::string& file_path = "",
        size_t line = 0,
        size_t column = 0,
        const std::string& context = ""
    );
    
    void convert_test_failures_to_diagnostics(
        const std::vector<FunctionTestResult>& test_results,
        const std::string& file_path
    );
    
    std::string read_file(const std::string& file_path);
};

} // namespace meld::compiler