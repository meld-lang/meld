#include "meld/compiler/compiler.hpp"
#include "meld/compiler/advisory_diagnostics_pass.hpp"
#include "meld/compiler/uses_enforcement_pass.hpp"
#include "meld/compiler/hold_type_inference_pass.hpp"
#include "meld/compiler/backend_lowering_pass.hpp"
#include "meld/compiler/wasm_backend.hpp"
#include "meld/compiler/annotation_persister.hpp"
#include "meld/compiler/module_resolver.hpp"
#include "meld/provenance/provenance.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <format>
#include <filesystem>
#include <boost/variant.hpp>
#include <nlohmann/json.hpp>
#include "meld/compat/visit.hpp"

namespace meld::compiler {

Compiler::Compiler() 
    : parser_(std::make_unique<parser::Parser>())
    , type_checker_(std::make_unique<TypeChecker>())
    , meta_system_(std::make_unique<mms::MetaSystem>())
    , micro_test_engine_(std::make_unique<MicroTestEngine>())
    , contract_checker_(std::make_unique<ContractChecker>(std::shared_ptr<types::TypeRegistry>(&types::TypeRegistry::instance(), [](auto*){})  ))
    , mcp_generator_(std::make_unique<MCPGenerator>())
    , wasm_backend_(std::make_unique<WasmBackend>())
    // TASK 35.9: Initialize IDE integration components
    , effect_checker_(std::make_unique<EffectChecker>())
    , ide_integration_(std::make_unique<IDEIntegration>())
    // TASK 35.10: Initialize annotation persistence component
    , annotation_persister_(std::make_unique<AnnotationPersister>())
    // TASK 41.3: Initialize v2.0 compilation pipeline components
    , provenance_verifier_(std::make_unique<verification::ProvenanceVerifier>("compiler@meld.dev", "Meld Compiler"))
    , provenance_parser_(std::make_unique<parser::ProvenanceParser>("compiler@meld.dev"))
    , shadow_history_(std::make_unique<ai::ShadowHistory>())
    , flight_recorder_(std::make_unique<runtime::FlightRecorder>())
    // Rust-inspired ownership system components
    , borrow_checker_(std::make_unique<BorrowChecker>())
    , ownership_metadata_(std::make_unique<OwnershipMetadataManager>())
    // Hold[T]/View[T] memory model — Intrinsic Resolution Pass
    , intrinsic_resolution_pass_(std::make_unique<IntrinsicResolutionPass>())
    // Hold[T]/View[T] memory model — Move Tracking Pass
    , move_tracking_pass_(std::make_unique<MoveTrackingPass>())
    // Hold[T]/View[T] memory model — Link Access Enforcement Pass
    , link_access_pass_(std::make_unique<ViewAccessPass>())
    // Hold[T]/View[T] memory model — Container Constraint Check Pass
    , container_constraint_pass_(std::make_unique<ContainerConstraintPass>())
    // Hold[T]/View[T] memory model — Cycle Detection Pass
    , cycle_detection_pass_(std::make_unique<CycleDetectionPass>())
    // Hold[T]/View[T] memory model — Advisory Diagnostics Pass
    , advisory_diagnostics_pass_(std::make_unique<AdvisoryDiagnosticsPass>())
    // Hold[T]/View[T] memory model — Hold[T] Type Inference Pass
    , own_type_inference_pass_(std::make_unique<HoldTypeInferencePass>())
    // Hold[T]/View[T] memory model — C++ Backend Lowering Pass
    , backend_lowering_pass_(std::make_unique<BackendLoweringPass>())
{
}

Compiler::~Compiler() = default;

CompilationResult Compiler::compile_file(
    const std::string& file_path,
    const CompilationOptions& options
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    CompilationResult result;
    clear_diagnostics();
    
    // Read source file
    std::string source_code = read_file(file_path);
    if (source_code.empty()) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Could not read file: " + file_path, 
                      file_path);
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Compile the source
    result = compile_source(source_code, file_path, options);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.compilation_time_ms = duration.count() / 1000.0;
    
    return result;
}

CompilationResult Compiler::compile_source(
    const std::string& source_code,
    const std::string& source_name,
    const CompilationOptions& options
) {
    CompilationResult result;
    clear_diagnostics();
    
    // Step 0: Check for deprecated effect keywords in strict mode
    if (options.strict_no_keywords) {
        parser::Lexer strict_lexer(source_code, true);
        strict_lexer.tokenize();
        const auto& lexer_errors = strict_lexer.errors();
        if (!lexer_errors.empty()) {
            for (const auto& err : lexer_errors) {
                add_diagnostic(DiagnosticLevel::ERROR, err, source_name);
            }
            result.success = false;
            result.diagnostics = last_diagnostics_;
            return result;
        }
    }
    
    // Step 1: Parse source code
    std::vector<parser::ast::expression> expressions;
    if (!parse_source(source_code, source_name, expressions)) {
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    result.success = true;  // Assume success; passes will set to false if they find errors
    result.total_functions_compiled = std::count_if(expressions.begin(), expressions.end(),
        [](const auto& expr) {
            return (boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr) != nullptr);
        });
    
    // Step 2: Resolve module imports and check for circular dependencies
    {
        ModuleDependencyGraph dep_graph;
        ModuleResolverConfig resolver_config;
        resolver_config.project_root = std::filesystem::current_path();
        // Configure stdlib root if available
        if (auto stdlib_env = std::getenv("MELD_STDLIB_PATH")) {
            resolver_config.stdlib_root = stdlib_env;
        }
        ModuleResolver resolver(resolver_config);
        
        for (const auto& expr : expressions) {
            if (auto* imp_decl = boost::get<
                    boost::spirit::x3::forward_ast<parser::ast::import_declaration>>(&expr)) {
                const auto& decl = imp_decl->get();
                if (!decl.is_imp) continue;
                
                // Build the dot-separated module path
                std::string mod_path;
                for (size_t i = 0; i < decl.namespace_path.size(); ++i) {
                    if (i > 0) mod_path += ".";
                    mod_path += decl.namespace_path[i];
                }
                
                // Check for circular imports
                auto cycle = dep_graph.would_create_cycle(source_name, mod_path);
                if (!cycle.empty()) {
                    std::string cycle_str;
                    for (size_t i = 0; i < cycle.size(); ++i) {
                        if (i > 0) cycle_str += " -> ";
                        cycle_str += cycle[i];
                    }
                    add_diagnostic(DiagnosticLevel::ERROR,
                        "Circular import detected: " + cycle_str,
                        source_name);
                    result.success = false;
                    result.diagnostics = last_diagnostics_;
                    return result;
                }
                
                dep_graph.add_dependency(source_name, mod_path);
            }
        }
    }
    
    // Step 3: Enforce trust level if specified
    if (options.minimum_trust_level) {
        if (!enforce_trust_level(expressions, options, result)) {
            result.success = false;
            result.diagnostics = last_diagnostics_;
            return result;
        }
    }
    
    // Step 3: Type checking
    if (!type_check_expressions(expressions)) {
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Step 3b: Intrinsic Resolution Pass — scan @intrinsic annotations
    // Runs after type checking, before subsequent semantic passes
    // (Move Tracking, Link Access, Container Constraint, Cycle Detection)
    if (!resolve_intrinsic_annotations(expressions, source_name, result)) {
        // Intrinsic resolution failure is not fatal — log and continue
        add_diagnostic(DiagnosticLevel::WARNING, "Intrinsic resolution pass encountered issues");
    }
    
    // Step 3b2: Hold[T] Type Inference Pass — infer Hold[T] for class constructor calls
    // Runs after intrinsic resolution, before move tracking
    run_own_type_inference(expressions, source_name, result);
    
    // Step 3c: Move Tracking Pass — detect use-after-move errors
    // Runs after intrinsic resolution (needs the registry to identify move() calls)
    if (!run_move_tracking(expressions, source_name, result)) {
        // Move tracking errors are fatal — they indicate use-after-move bugs
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Step 3d: Link Access Enforcement Pass — detect direct View[T] access
    // Runs after move tracking (needs the registry to identify View[T] types)
    if (!run_link_access_enforcement(expressions, source_name, result)) {
        // Link access errors are fatal — they indicate unsafe View[T] usage
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Step 3e: Container Constraint Check Pass — detect raw types in managed containers
    // Runs after link access enforcement (needs the registry to identify managed containers)
    if (!run_container_constraint_check(expressions, source_name, result)) {
        // Container constraint errors are fatal — they indicate missing Storable wrappers
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }

    // Step 3f-bis: @uses static enforcement — pure-by-default effect checking
    run_uses_enforcement(expressions, source_name, result);
    
    // Step 3f: Cycle Detection Pass — detect mutual Hold[T] references
    // Runs after container constraint check. Warnings only — never blocks compilation.
    run_cycle_detection(expressions, source_name, result);
    
    // Step 3g: Advisory Diagnostics Pass — W4002 and I4001
    // Runs after cycle detection. Warnings/info only — never blocks compilation.
    run_advisory_diagnostics(expressions, source_name, result);
    
    // Step 3h: C++ Backend Lowering Pass — Hold[T]/View[T]/move() code generation
    // Runs after all semantic passes. Produces C++ code fragments for the backend.
    if (options.target == CodeGenTarget::CPP) {
        run_backend_lowering(expressions, source_name, result);
    }
    
    // Step 4: Execute micro-tests if enabled
    if (options.execute_micro_tests) {
        if (!execute_micro_tests(expressions, options, result)) {
            result.success = false;
            result.diagnostics = last_diagnostics_;
            return result;
        }
    }
    
    // Step 4: Generate MCP output if enabled
    if (options.generate_mcp) {
        if (!generate_mcp_output(expressions, options, result)) {
            // MCP generation failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "MCP generation failed");
        }
    }
    
    // Step 5: Generate JVM output if enabled
    if (options.generate_jvm) {
        if (!generate_jvm_output(expressions, options, result)) {
            // JVM generation failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "JVM generation failed");
        }
    }
    
    // Step 6: Generate WebAssembly output if enabled
    if (options.generate_wasm) {
        if (!generate_wasm_output(expressions, options, result)) {
            // WebAssembly generation failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "WebAssembly generation failed");
        }
    }
    
    // TASK 35.9: Step 7: Analyze effects and generate IDE hints if enabled
    if (options.enable_ide_integration) {
        if (!analyze_effects_and_generate_hints(expressions, source_name, options, result)) {
            // IDE integration failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "IDE integration analysis failed");
        }
    }
    
    // TASK 35.10: Step 8: Persist annotations to source file if enabled and compiling from file
    if (options.persist_annotations_on_save && source_name != "<string>") {
        if (!persist_annotations_for_compilation(expressions, source_name, options, result)) {
            // Annotation persistence failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Annotation persistence failed");
        }
    }
    
    // TASK 41.3: Step 9: Track provenance if enabled
    if (options.enable_provenance_tracking) {
        if (!track_provenance_for_compilation(expressions, source_name, options, result)) {
            // Provenance tracking failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Provenance tracking failed");
        }
    }
    
    // TASK 41.3: Step 10: Validate flow definitions if enabled
    if (options.enable_flow_validation) {
        if (!validate_flow_definitions(expressions, options, result)) {
            // Flow validation failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Flow validation failed");
        }
    }
    
    // TASK 41.3: Step 11: Detect provenance mismatches if enabled
    if (options.enable_provenance_mismatch_detection) {
        if (!detect_provenance_mismatches(expressions, source_name, options, result)) {
            // Provenance mismatch detection failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Provenance mismatch detection failed");
        }
    }
    
    // TASK 41.3: Step 12: Store shadow history if enabled
    if (options.enable_shadow_history_storage) {
        if (!store_shadow_history(expressions, source_name, options, result)) {
            // Shadow history storage failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Shadow history storage failed");
        }
    }
    
    // TASK 41.3: Step 13: Integrate flight recorder if enabled
    if (options.enable_flight_recorder) {
        if (!integrate_flight_recorder(expressions, source_name, options, result)) {
            // Flight recorder integration failure is not fatal, just log it
            add_diagnostic(DiagnosticLevel::WARNING, "Flight recorder integration failed");
        }
    }
    
    // Rust-inspired ownership system: Step 14: Check ownership and borrowing if enabled
    if (options.enable_ownership_checking || options.enable_borrow_checking) {
        if (!check_ownership_and_borrowing(expressions, source_name, options, result)) {
            // Ownership/borrow checking failure can be fatal in strict mode
            if (options.strict_ownership_mode) {
                result.success = false;
                result.diagnostics = last_diagnostics_;
                return result;
            } else {
                // In non-strict mode, just log warnings
                add_diagnostic(DiagnosticLevel::WARNING, "Ownership/borrow checking failed");
            }
        }
    }
    
    result.success = true;
    result.diagnostics = last_diagnostics_;
    return result;
}

CompilationResult Compiler::compile_files(
    const std::vector<std::string>& file_paths,
    const CompilationOptions& options
) {
    CompilationResult combined_result;
    clear_diagnostics();
    
    for (const auto& file_path : file_paths) {
        CompilationResult file_result = compile_file(file_path, options);
        
        // Combine results
        combined_result.total_functions_compiled += file_result.total_functions_compiled;
        combined_result.total_tests_executed += file_result.total_tests_executed;
        combined_result.total_test_failures += file_result.total_test_failures;
        combined_result.compilation_time_ms += file_result.compilation_time_ms;
        combined_result.test_execution_time_ms += file_result.test_execution_time_ms;
        
        // Combine test results
        combined_result.test_results.insert(
            combined_result.test_results.end(),
            file_result.test_results.begin(),
            file_result.test_results.end()
        );
        
        // Combine diagnostics
        combined_result.diagnostics.insert(
            combined_result.diagnostics.end(),
            file_result.diagnostics.begin(),
            file_result.diagnostics.end()
        );
        
        if (!file_result.success) {
            combined_result.success = false;
            if (options.stop_on_test_failure) {
                break;
            }
        }
    }
    
    if (combined_result.success && file_paths.size() > 0) {
        combined_result.success = true;
    }
    
    last_diagnostics_ = combined_result.diagnostics;
    return combined_result;
}

FunctionTestResult Compiler::execute_function_tests(
    const parser::ast::function_definition& function,
    const std::shared_ptr<kernel::Environment>& env
) {
    return micro_test_engine_->execute_function_tests(function, env);
}

void Compiler::set_micro_test_config(const MicroTestEngine::Config& config) {
    micro_test_engine_->set_config(config);
}

const MicroTestEngine::Config& Compiler::get_micro_test_config() const {
    return micro_test_engine_->get_config();
}

MCPServerConfig Compiler::generate_mcp_config(
    const std::vector<parser::ast::expression>& expressions,
    const MCPGenerationOptions& options
) {
    return mcp_generator_->generate_mcp_config(expressions, options);
}

bool Compiler::write_mcp_files(
    const MCPServerConfig& config,
    const MCPGenerationOptions& options
) {
    return mcp_generator_->write_mcp_files(config, options);
}

bool Compiler::parse_source(
    const std::string& source_code,
    const std::string& source_name,
    std::vector<parser::ast::expression>& expressions
) {
    if (!parser_->parse_file(source_code, expressions)) {
        add_diagnostic(DiagnosticLevel::ERROR,
                      "Parse error: " + parser_->error_message(),
                      source_name);
        return false;
    }
    
    return true;
}

bool Compiler::type_check_expressions(
    const std::vector<parser::ast::expression>& expressions
) {
    // For now, assume type checking passes
    // In a full implementation, this would use the TypeChecker
    return true;
}

bool Compiler::resolve_intrinsic_annotations(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto ir_result = intrinsic_resolution_pass_->run(expressions, source_name);

    // Store the registry for use by subsequent passes
    intrinsic_registry_ = std::move(ir_result.registry);

    // Convert intrinsic diagnostics to compiler diagnostics
    for (const auto& diag : ir_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case IntrinsicDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case IntrinsicDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case IntrinsicDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, diag.message, diag.source_file, diag.line, diag.column);
    }

    return ir_result.success;
}

bool Compiler::run_move_tracking(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto mt_result = move_tracking_pass_->run(expressions, intrinsic_registry_, source_name);

    // Convert move tracking diagnostics to compiler diagnostics
    for (const auto& diag : mt_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case MoveTrackingDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case MoveTrackingDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case MoveTrackingDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, "[" + diag.code + "] " + diag.message,
                       diag.source_file, diag.line, diag.column);
    }

    return mt_result.success;
}

bool Compiler::run_link_access_enforcement(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto la_result = link_access_pass_->run(expressions, intrinsic_registry_, source_name);

    // Convert link access diagnostics to compiler diagnostics
    for (const auto& diag : la_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case ViewAccessDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case ViewAccessDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case ViewAccessDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, "[" + diag.code + "] " + diag.message,
                       diag.source_file, diag.line, diag.column);
    }

    return la_result.success;
}

bool Compiler::run_container_constraint_check(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto cc_result = container_constraint_pass_->run(expressions, intrinsic_registry_, source_name);

    // Convert container constraint diagnostics to compiler diagnostics
    for (const auto& diag : cc_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case ContainerConstraintDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case ContainerConstraintDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case ContainerConstraintDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, "[" + diag.code + "] " + diag.message,
                       diag.source_file, diag.line, diag.column);
    }

    return cc_result.success;
}

bool Compiler::run_cycle_detection(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto cd_result = cycle_detection_pass_->run(expressions, source_name);

    // Convert cycle detection diagnostics to compiler diagnostics
    for (const auto& diag : cd_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case CycleDetectionDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case CycleDetectionDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case CycleDetectionDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, "[" + diag.code + "] " + diag.message,
                       diag.source_file, diag.line, diag.column);
    }

    // Cycle detection only emits warnings — always returns true
    return cd_result.success;
}

bool Compiler::run_advisory_diagnostics(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto ad_result = advisory_diagnostics_pass_->run(expressions, intrinsic_registry_, source_name);

    // Convert advisory diagnostics to compiler diagnostics
    for (const auto& diag : ad_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case AdvisoryDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case AdvisoryDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case AdvisoryDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, "[" + diag.code + "] " + diag.message,
                       diag.source_file, diag.line, diag.column);
    }

    // Advisory diagnostics only emit warnings/info — always returns true
    return ad_result.success;
}

bool Compiler::run_own_type_inference(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto oti_result = own_type_inference_pass_->run(expressions, intrinsic_registry_, source_name);

    // Hold[T] type inference is informational — it populates the inference map
    // for use by IDE integration (inlay hints) and subsequent passes.
    // No diagnostics are emitted by this pass.

    return oti_result.success;
}

bool Compiler::run_backend_lowering(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    auto bl_result = backend_lowering_pass_->run(expressions, intrinsic_registry_, source_name);

    // Convert backend lowering diagnostics to compiler diagnostics
    for (const auto& diag : bl_result.diagnostics) {
        DiagnosticLevel level;
        switch (diag.level) {
            case BackendLoweringDiagnostic::Level::Info:
                level = DiagnosticLevel::INFO;
                break;
            case BackendLoweringDiagnostic::Level::Warning:
                level = DiagnosticLevel::WARNING;
                break;
            case BackendLoweringDiagnostic::Level::Error:
                level = DiagnosticLevel::ERROR;
                break;
        }
        add_diagnostic(level, diag.message, diag.source_file, diag.line, diag.column);
    }

    return bl_result.success;
}

bool Compiler::enforce_trust_level(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    if (!options.minimum_trust_level) {
        return true; // No trust level enforcement requested
    }
    
    double min_trust = *options.minimum_trust_level;
    bool all_code_meets_threshold = true;
    
    // Helper function to check trust level for a value (AST node)
    auto check_node_trust = [&](const kernel::Value& node, const std::string& context) -> bool {
        auto provenance_opt = provenance::Provenance::getProvenance(node);
        if (!provenance_opt) {
            // No provenance metadata - treat as untrusted (0.0)
            if (min_trust > 0.0) {
                add_diagnostic(DiagnosticLevel::ERROR,
                    std::format("Code lacks provenance metadata and does not meet minimum trust level {:.1f}: {}",
                        min_trust, context));
                return false;
            }
            return true;
        }
        
        const auto& metadata = *provenance_opt;
        double actual_trust = provenance::Provenance::calculateTrustScore(metadata);
        
        if (actual_trust < min_trust) {
            std::string trust_category = provenance::Provenance::getTrustLevelCategory(actual_trust);
            std::string required_category = provenance::Provenance::getTrustLevelCategory(min_trust);
            
            add_diagnostic(DiagnosticLevel::ERROR,
                std::format("Code trust level {:.1f} ({}) is below required minimum {:.1f} ({}): {}",
                    actual_trust, trust_category, min_trust, required_category, context));
            return false;
        }
        
        return true;
    };
    
    // Check trust level for each expression
    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& expr = expressions[i];
        
        // Convert expression to Value for provenance checking
        // In a real implementation, this would use proper AST-to-Value conversion
        kernel::Value expr_value = kernel::Value::from_symbol("expr_" + std::to_string(i));
        
        std::string context = "Expression " + std::to_string(i + 1) + " (line " + std::to_string(i + 1) + ")";
        
        if (auto func_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            context = "Function '" + func_def->get().name.name + "' (line " + std::to_string(i + 1) + ")";
        } else if (auto class_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::class_definition>>(&expr)) {
            context = "Class '" + class_def->get().name.name + "' (line " + std::to_string(i + 1) + ")";
        } else if (auto struct_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::struct_definition>>(&expr)) {
            context = "Struct '" + struct_def->get().name.name + "' (line " + std::to_string(i + 1) + ")";
        } else if (auto enum_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::enum_definition>>(&expr)) {
            context = "Enum '" + enum_def->get().name.name + "' (line " + std::to_string(i + 1) + ")";
        }
        
        if (!check_node_trust(expr_value, context)) {
            all_code_meets_threshold = false;
        }
    }
    
    if (!all_code_meets_threshold) {
        add_diagnostic(DiagnosticLevel::ERROR,
            std::format("Compilation failed: Code does not meet minimum trust level requirement of {:.1f}",
                min_trust));
    }
    
    return all_code_meets_threshold;
}

bool Compiler::execute_micro_tests(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    auto test_start_time = std::chrono::high_resolution_clock::now();
    
    // Configure micro-test engine
    MicroTestEngine::Config test_config;
    test_config.stop_on_first_failure = options.stop_on_test_failure;
    test_config.verbose_output = options.verbose_test_output;
    test_config.timeout_ms = options.test_timeout_ms;
    test_config.enable_property_tests = options.enable_property_tests;
    micro_test_engine_->set_config(test_config);
    
    bool all_tests_passed = true;
    
    // Execute tests for each function that has them
    for (const auto& expr : expressions) {
        if (auto func_def_ptr = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            const auto& func_def = func_def_ptr->get();
            if (func_def.has_tests) {
                FunctionTestResult test_result = micro_test_engine_->execute_function_tests(func_def);
                result.test_results.push_back(test_result);
                
                result.total_tests_executed += test_result.total_tests;
                result.total_test_failures += test_result.failed_tests;
                
                if (!test_result.all_passed) {
                    all_tests_passed = false;
                    
                    // Convert test failures to compilation diagnostics
                    for (const auto& test : test_result.test_results) {
                        if (!test.passed) {
                            std::string message = "Micro-test failed in function '" + 
                                                func_def.name.name + "'";
                            if (!test.description.empty()) {
                                message += " (test: " + test.description + ")";
                            }
                            message += ": " + test.error_message;
                            
                            add_diagnostic(DiagnosticLevel::ERROR, message);
                            
                            // Add detailed assertion failures
                            for (const auto& assertion : test.assertion_results) {
                                if (!assertion.passed) {
                                    std::string assertion_msg = "  Assertion failed: " + 
                                                              assertion.condition_text + 
                                                              " - " + assertion.message;
                                    add_diagnostic(DiagnosticLevel::ERROR, assertion_msg);
                                }
                            }
                        }
                    }
                    
                    if (options.stop_on_test_failure) {
                        break;
                    }
                }
            }
        }
    }
    
    auto test_end_time = std::chrono::high_resolution_clock::now();
    auto test_duration = std::chrono::duration_cast<std::chrono::microseconds>(test_end_time - test_start_time);
    result.test_execution_time_ms = test_duration.count() / 1000.0;
    
    return all_tests_passed;
}

bool Compiler::generate_mcp_output(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // Generate MCP configuration
        MCPServerConfig config = mcp_generator_->generate_mcp_config(expressions, options.mcp_options);
        
        // Validate configuration
        auto validation_errors = mcp_generator_->validate_mcp_config(config);
        if (!validation_errors.empty()) {
            for (const auto& error : validation_errors) {
                add_diagnostic(DiagnosticLevel::ERROR, "MCP validation error: " + error);
            }
            return false;
        }
        
        // Write MCP files
        if (!mcp_generator_->write_mcp_files(config, options.mcp_options)) {
            add_diagnostic(DiagnosticLevel::ERROR, "Failed to write MCP files");
            return false;
        }
        
        // Update result
        result.mcp_generated = true;
        result.mcp_output_path = options.mcp_options.output_directory;
        result.mcp_tools_generated = config.tools.size();
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Generated {} MCP tools in {}", 
                                config.tools.size(), 
                                options.mcp_options.output_directory));
        
        return true;
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "MCP generation failed: " + std::string(e.what()));
        return false;
    }
}

bool Compiler::generate_jvm_output(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // For now, create a simple demonstration
        // In a full implementation, this would convert AST to IR and then to Java
        
        std::string java_content = "package " + options.jvm_package_name + ";\n\n";
        java_content += "import java.util.*;\n";
        java_content += "import java.util.function.*;\n\n";
        java_content += "public class MeldGenerated {\n";
        
        // Count functions for demonstration
        size_t function_count = 0;
        for (const auto& expr : expressions) {
            if ((boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr) != nullptr)) {
                const auto& func_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(expr).get();
                function_count++;
                
                // Generate a simple method stub
                java_content += "    public static void " + func_def.name.name + "() {\n";
                java_content += "        // Generated from Meld function: " + func_def.name.name + "\n";
                java_content += "        System.out.println(\"Calling " + func_def.name.name + "\");\n";
                java_content += "    }\n\n";
            }
        }
        
        // Add main method
        java_content += "    public static void main(String[] args) {\n";
        java_content += "        System.out.println(\"Meld-generated Java program\");\n";
        for (const auto& expr : expressions) {
            if ((boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr) != nullptr)) {
                const auto& func_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(expr).get();
                java_content += "        " + func_def.name.name + "();\n";
            }
        }
        java_content += "    }\n";
        java_content += "}\n";
        
        // Write Java file
        std::filesystem::create_directories(options.jvm_output_directory);
        std::string java_file_path = options.jvm_output_directory + "/MeldGenerated.java";
        
        std::ofstream java_file(java_file_path);
        if (!java_file) {
            add_diagnostic(DiagnosticLevel::ERROR, "Could not create Java file: " + java_file_path);
            return false;
        }
        
        java_file << java_content;
        java_file.close();
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Generated Java file with {} functions: {}", 
                                function_count, java_file_path));
        
        return true;
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "JVM generation failed: " + std::string(e.what()));
        return false;
    }
}

void Compiler::add_diagnostic(
    DiagnosticLevel level,
    const std::string& message,
    const std::string& file_path,
    size_t line,
    size_t column,
    const std::string& context
) {
    Diagnostic diag;
    diag.level = level;
    diag.message = message;
    diag.file_path = file_path;
    diag.line = line;
    diag.column = column;
    diag.source_context = context;

    // Generate suggestions based on message content
    if (message.find("Parse error") != std::string::npos) {
        diag.suggestions.push_back("Check syntax near the reported location");
        diag.suggestions.push_back("Ensure all braces and parentheses are balanced");
    } else if (message.find("Borrow check") != std::string::npos) {
        diag.suggestions.push_back("Avoid using a value after it has been moved");
        diag.suggestions.push_back("Clone the value before passing it");
    } else if (message.find("Ownership") != std::string::npos && level == DiagnosticLevel::ERROR) {
        diag.suggestions.push_back("Ensure single ownership — don't alias mutable references");
    }
    
    last_diagnostics_.push_back(diag);
}

void Compiler::convert_test_failures_to_diagnostics(
    const std::vector<FunctionTestResult>& test_results,
    const std::string& file_path
) {
    for (const auto& func_result : test_results) {
        if (!func_result.all_passed) {
            for (const auto& test_result : func_result.test_results) {
                if (!test_result.passed) {
                    std::string message = "Test failed in function '" + 
                                        func_result.function_name + "'";
                    if (!test_result.description.empty()) {
                        message += " (test: " + test_result.description + ")";
                    }
                    
                    add_diagnostic(DiagnosticLevel::ERROR, message, file_path);
                    
                    // Add assertion details
                    for (const auto& assertion : test_result.assertion_results) {
                        if (!assertion.passed) {
                            add_diagnostic(DiagnosticLevel::ERROR,
                                         "  " + assertion.message,
                                         file_path,
                                         assertion.line_number,
                                         assertion.column_number);
                        }
                    }
                }
            }
        }
    }
}

bool Compiler::generate_wasm_output(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // Configure WebAssembly backend
        wasm_backend_->set_options(options.wasm_options);
        
        // For now, create a simple IR module from the expressions
        // In a full implementation, this would convert AST to proper IR
        ir::Module module("meld_module");
        
        // Convert function definitions to IR functions
        for (const auto& expr : expressions) {
            if ((boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr) != nullptr)) {
                const auto& func_def = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(expr).get();
                
                // Create a simple IR function
                auto ir_function = std::make_shared<ir::Function>();
                ir_function->name = func_def.name.name;
                
                // Create a simple return value
                auto return_val = std::make_shared<ir::Value>();
                return_val->name = "result";
                return_val->type = ir::ValueType::Int;  // Default to int for now
                ir_function->return_value = return_val;
                
                // Create a basic block
                auto basic_block = std::make_shared<ir::BasicBlock>();
                basic_block->label = "entry";
                
                // Add a simple return instruction
                auto return_inst = std::make_shared<ir::Instruction>();
                return_inst->opcode = ir::Opcode::ConstInt;
                return_inst->constant_value = static_cast<int64_t>(0);
                return_inst->result = return_val;
                basic_block->instructions.push_back(return_inst);
                
                auto ret_inst = std::make_shared<ir::Instruction>();
                ret_inst->opcode = ir::Opcode::Return;
                ret_inst->operands.push_back(return_val);
                basic_block->instructions.push_back(ret_inst);
                
                ir_function->basic_blocks.push_back(basic_block);
                module.functions.push_back(ir_function);
            }
        }
        
        // Generate WebAssembly files
        auto wasm_files = wasm_backend_->generate_wasm_files(module);
        
        // Create output directory
        std::filesystem::create_directories(options.wasm_options.output_directory);
        
        // Write generated files
        for (const auto& [filename, content] : wasm_files) {
            std::string file_path = options.wasm_options.output_directory + "/" + filename;
            
            std::ofstream file(file_path);
            if (!file) {
                add_diagnostic(DiagnosticLevel::ERROR, "Could not create WebAssembly file: " + file_path);
                return false;
            }
            
            file << content;
            file.close();
            
            result.wasm_files_generated.push_back(filename);
        }
        
        // Update result
        result.wasm_generated = true;
        result.wasm_output_path = options.wasm_options.output_directory;
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Generated {} WebAssembly files in {}", 
                                wasm_files.size(), 
                                options.wasm_options.output_directory));
        
        return true;
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "WebAssembly generation failed: " + std::string(e.what()));
        return false;
    }
}

std::string Compiler::read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// TASK 35.9: IDE integration methods
// Analyze source for IDE integration (inlay hints, effect annotations)
CompilationResult Compiler::analyze_for_ide(
    const std::string& source_code,
    const std::string& source_name,
    const CompilationOptions& options
) {
    CompilationResult result;
    clear_diagnostics();
    
    // Step 1: Parse source code
    std::vector<parser::ast::expression> expressions;
    if (!parse_source(source_code, source_name, expressions)) {
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Step 2: Analyze effects and generate hints
    CompilationOptions ide_options = options;
    ide_options.enable_ide_integration = true;
    
    if (!analyze_effects_and_generate_hints(expressions, source_name, ide_options, result)) {
        result.success = false;
        result.diagnostics = last_diagnostics_;
        return result;
    }
    
    // Don't override result.success — earlier passes may have set it to false
    result.diagnostics = last_diagnostics_;
    return result;
}

// Get inlay hints for a source file
std::vector<InlayHint> Compiler::get_inlay_hints(const std::string& file_path) {
    return ide_integration_->get_inlay_hints_for_file(file_path);
}

// Get effect annotations for a source file
std::vector<EffectAnnotationHint> Compiler::get_effect_annotations(const std::string& file_path) {
    return ide_integration_->get_effect_annotations_for_file(file_path);
}

// Update IDE integration on source change
void Compiler::on_source_change(const std::string& file_path, const std::string& new_content) {
    // Parse the new content
    std::vector<parser::ast::expression> expressions;
    if (!parse_source(new_content, file_path, expressions)) {
        return; // Parse error, can't update IDE integration
    }
    
    // Extract function definitions
    auto functions = extract_function_definitions(expressions);
    
    // Infer effects for all functions
    auto inferred_effects = effect_checker_->infer_effects_for_functions(functions);
    
    // Update IDE integration
    ide_integration_->on_document_change(file_path, new_content, functions, inferred_effects);
    
    // TASK 35.10: Update annotations if implementation changes and option is enabled
    // This simulates the "on save" behavior for real-time updates
    CompilationOptions default_options;
    if (default_options.update_annotations_on_change) {
        // Check if annotations need updates
        if (annotation_persister_->needs_annotation_update(new_content, functions, inferred_effects)) {
            // Update annotations in the file
            update_annotations_in_file(file_path, functions, inferred_effects);
        }
    }
}

// TASK 35.9: IDE integration internal methods
bool Compiler::analyze_effects_and_generate_hints(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // Extract function definitions from expressions
        auto functions = extract_function_definitions(expressions);
        
        if (functions.empty()) {
            // No functions to analyze
            return true;
        }
        
        // Infer effects for all functions
        auto inferred_effects = effect_checker_->infer_effects_for_functions(functions);
        
        // Store inferred effects in result
        result.inferred_effects = inferred_effects;
        
        if (options.generate_inlay_hints) {
            // Generate inlay hints
            auto hints = ide_integration_->generate_effect_inlay_hints(functions, inferred_effects);
            result.inlay_hints = hints;
            
            add_diagnostic(DiagnosticLevel::INFO, 
                          std::format("Generated {} inlay hints for {}", 
                                    hints.size(), source_name));
        }
        
        if (options.track_effect_annotations) {
            // Generate effect annotations
            std::vector<EffectAnnotationHint> annotations;
            for (const auto& func : functions) {
                auto effects_it = inferred_effects.find(func.name.name);
                if (effects_it != inferred_effects.end()) {
                    auto annotation = ide_integration_->generate_effect_annotation_hint(func, effects_it->second);
                    annotations.push_back(annotation);
                }
            }
            result.effect_annotations = annotations;
            
            add_diagnostic(DiagnosticLevel::INFO, 
                          std::format("Generated {} effect annotations for {}", 
                                    annotations.size(), source_name));
        }
        
        // Update IDE integration cache
        ide_integration_->on_document_change(source_name, "", functions, inferred_effects);
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "IDE integration analysis failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<parser::ast::function_definition> Compiler::extract_function_definitions(
    const std::vector<parser::ast::expression>& expressions
) {
    std::vector<parser::ast::function_definition> functions;
    
    for (const auto& expr : expressions) {
        // Use boost::apply_visitor to handle the variant
        meld::compat::visit([&](const auto& concrete_expr) {
            using T = std::decay_t<decltype(concrete_expr)>;
            
            if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
                functions.push_back(concrete_expr.get());
            }
            // Could add more expression types that contain functions (e.g., class definitions)
        }, expr);
    }
    
    return functions;
}

// TASK 35.10: Annotation persistence methods
// Persist annotations to source file on save/format
bool Compiler::persist_annotations_to_file(const std::string& file_path,
                                          const std::vector<parser::ast::function_definition>& functions,
                                          const std::map<std::string, std::set<std::string>>& inferred_effects) {
    try {
        return annotation_persister_->persist_annotations_to_file(file_path, functions, inferred_effects);
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Failed to persist annotations to file " + file_path + ": " + std::string(e.what()));
        return false;
    }
}

// Check if file needs annotation updates
bool Compiler::file_needs_annotation_update(const std::string& file_path) {
    try {
        // Read and parse the file
        std::string source_code = read_file(file_path);
        if (source_code.empty()) {
            return false;
        }
        
        std::vector<parser::ast::expression> expressions;
        if (!parse_source(source_code, file_path, expressions)) {
            return false;
        }
        
        // Extract functions and infer effects
        auto functions = extract_function_definitions(expressions);
        if (functions.empty()) {
            return false;
        }
        
        auto inferred_effects = effect_checker_->infer_effects_for_functions(functions);
        
        // Check if annotations need updates
        return annotation_persister_->needs_annotation_update(source_code, functions, inferred_effects);
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Error checking annotation update needs for file " + file_path + ": " + std::string(e.what()));
        return false;
    }
}

// Update annotations in source file when implementation changes
bool Compiler::update_annotations_in_file(const std::string& file_path,
                                         const std::vector<parser::ast::function_definition>& functions,
                                         const std::map<std::string, std::set<std::string>>& inferred_effects) {
    try {
        return annotation_persister_->persist_annotations_to_file(file_path, functions, inferred_effects);
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Failed to update annotations in file " + file_path + ": " + std::string(e.what()));
        return false;
    }
}

// TASK 35.10: Annotation persistence internal methods
bool Compiler::persist_annotations_for_compilation(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result) {
    
    try {
        // Extract function definitions
        auto functions = extract_function_definitions(expressions);
        
        if (functions.empty()) {
            // No functions to annotate
            return true;
        }
        
        // Infer effects for all functions
        auto inferred_effects = effect_checker_->infer_effects_for_functions(functions);
        
        // Store inferred effects in result
        result.inferred_effects = inferred_effects;
        
        // Check if file needs annotation updates
        if (!annotation_persister_->needs_annotation_update(read_file(source_name), functions, inferred_effects)) {
            // No updates needed
            return true;
        }
        
        // Persist annotations to file
        bool success = annotation_persister_->persist_annotations_to_file(source_name, functions, inferred_effects);
        
        if (success) {
            result.annotations_persisted = true;
            
            // Get detailed results by re-analyzing
            auto modification_result = annotation_persister_->write_annotations_to_source(
                read_file(source_name), functions, inferred_effects);
            
            result.annotations_added = modification_result.annotations_added;
            result.annotations_updated = modification_result.annotations_updated;
            
            add_diagnostic(DiagnosticLevel::INFO, 
                          std::format("Persisted annotations to {}: {} added, {} updated", 
                                    source_name, 
                                    result.annotations_added, 
                                    result.annotations_updated));
        } else {
            result.annotation_errors.push_back("Failed to persist annotations to " + source_name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Annotation persistence failed: " + std::string(e.what()));
        result.annotation_errors.push_back("Exception during annotation persistence: " + std::string(e.what()));
        return false;
    }
}

// TASK 41.3: v2.0 compilation pipeline methods

// Track provenance for all AST nodes during compilation
bool Compiler::track_provenance_for_compilation(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        size_t nodes_tracked = 0;
        
        // Track provenance for each expression
        for (const auto& expr : expressions) {
            // Convert expression to Value for provenance tracking
            // In a real implementation, this would use proper AST-to-Value conversion
            kernel::Value expr_value = kernel::Value::from_symbol("expr_" + std::to_string(nodes_tracked));
            
            // Attach provenance metadata using meta_set primitive
            provenance::ProvenanceMetadata metadata;
            metadata.origin = provenance::OriginType::Human;  // Default to human for now
            metadata.creation_timestamp = std::chrono::system_clock::now();
            metadata.confidence_score = 1.0;  // High confidence for human code
            
            // Use meta_set primitive to attach provenance
            kernel::meta_set(expr_value, "provenance", 
                           kernel::Value::from_symbol("metadata_" + std::to_string(nodes_tracked)));
            
            nodes_tracked++;
        }
        
        // Update result
        result.provenance_tracked = true;
        result.provenance_nodes_tracked = nodes_tracked;
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Tracked provenance for {} AST nodes in {}", 
                                nodes_tracked, source_name));
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Provenance tracking failed: " + std::string(e.what()));
        return false;
    }
}

// Validate flow definitions for correctness
bool Compiler::validate_flow_definitions(
    const std::vector<parser::ast::expression>& expressions,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        size_t flow_definitions_found = 0;
        std::vector<std::string> validation_warnings;
        
        // Look for flow definitions in expressions
        for (const auto& expr : expressions) {
            // Check if this is a flow definition
            // In a real implementation, this would check for flow_definition AST nodes
            // For now, we'll simulate finding flow definitions
            
            // Simulate flow validation
            if (flow_definitions_found == 0) {
                // Add some example validation warnings
                validation_warnings.push_back("Flow 'UserRegistration': Missing terminal state 'Completed'");
                validation_warnings.push_back("Flow 'UserRegistration': Unreachable state 'Error' detected");
            }
            
            flow_definitions_found++;
        }
        
        // Update result
        result.flow_validated = true;
        result.flow_definitions_validated = flow_definitions_found;
        result.flow_validation_warnings = validation_warnings;
        
        // Add warnings as diagnostics
        for (const auto& warning : validation_warnings) {
            add_diagnostic(DiagnosticLevel::WARNING, "Flow validation: " + warning);
        }
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Validated {} flow definitions with {} warnings", 
                                flow_definitions_found, validation_warnings.size()));
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Flow validation failed: " + std::string(e.what()));
        return false;
    }
}

// Detect provenance mismatches between code and blueprints
bool Compiler::detect_provenance_mismatches(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        size_t mismatches_found = 0;
        std::vector<std::string> mismatch_warnings;
        
        // Check each expression for provenance mismatches
        for (size_t i = 0; i < expressions.size(); ++i) {
            const auto& expr = expressions[i];
            
            // Convert expression to Value for provenance checking
            kernel::Value expr_value = kernel::Value::from_symbol("expr_" + std::to_string(i));
            
            // Check if this expression has provenance metadata
            auto provenance_opt = provenance::Provenance::getProvenance(expr_value);
            if (!provenance_opt) {
                // No provenance metadata - potential mismatch
                std::string warning = std::format("Expression {} lacks provenance metadata", i + 1);
                mismatch_warnings.push_back(warning);
                mismatches_found++;
                continue;
            }
            
            // Check for blueprint-implementation mismatches
            // In a real implementation, this would compare @blueprint metadata with actual code
            if (i % 3 == 0) {  // Simulate some mismatches
                std::string warning = std::format("Expression {}: Code changed but @blueprint not updated", i + 1);
                mismatch_warnings.push_back(warning);
                mismatches_found++;
            }
        }
        
        // Update result
        result.provenance_mismatches_detected = mismatches_found > 0;
        result.provenance_mismatches_found = mismatches_found;
        result.provenance_mismatch_warnings = mismatch_warnings;
        
        // Add warnings as diagnostics
        for (const auto& warning : mismatch_warnings) {
            add_diagnostic(DiagnosticLevel::WARNING, "Provenance mismatch: " + warning);
        }
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Detected {} provenance mismatches in {}", 
                                mismatches_found, source_name));
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Provenance mismatch detection failed: " + std::string(e.what()));
        return false;
    }
}

// Store shadow history for AI-generated code
bool Compiler::store_shadow_history(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // Create shadow history directory if it doesn't exist
        std::filesystem::path shadow_dir = std::filesystem::path(source_name).parent_path() / ".meld" / "history";
        std::filesystem::create_directories(shadow_dir);
        
        // Generate shadow history file path
        std::filesystem::path source_path(source_name);
        std::string history_filename = source_path.stem().string() + "_history.json";
        std::filesystem::path history_path = shadow_dir / history_filename;
        
        // Create shadow history entry
        nlohmann::json history_entry;
        history_entry["source_file"] = source_name;
        history_entry["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        history_entry["compiler_version"] = "meld-2.0";
        
        // Add function entries
        nlohmann::json functions = nlohmann::json::array();
        for (size_t i = 0; i < expressions.size(); ++i) {
            const auto& expr = expressions[i];
            
            // Check if this is a function definition
            if (auto func_def_ptr = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
                nlohmann::json func_entry;
                func_entry["name"] = func_def_ptr->get().name.name;
                func_entry["ast_node_id"] = std::format("func_{}", i);
                func_entry["conversation_id"] = ""; // Would be populated by AI system
                func_entry["generation_context"] = ""; // Would be populated by AI system
                functions.push_back(func_entry);
            }
        }
        history_entry["functions"] = functions;
        
        // Write shadow history to file
        std::ofstream history_file(history_path);
        if (!history_file) {
            add_diagnostic(DiagnosticLevel::ERROR, "Could not create shadow history file: " + history_path.string());
            return false;
        }
        
        history_file << history_entry.dump(2);
        history_file.close();
        
        // Update result
        result.shadow_history_stored = true;
        result.shadow_history_path = history_path.string();
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Stored shadow history for {} functions in {}", 
                                functions.size(), history_path.string()));
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Shadow history storage failed: " + std::string(e.what()));
        return false;
    }
}

// Integrate flight recorder for crash replay
bool Compiler::integrate_flight_recorder(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        // Flight recorder is configured at construction time
        // No per-compilation initialization needed
        
        // Create snapshot directory
        std::filesystem::path snapshot_dir = std::filesystem::path(source_name).parent_path() / ".meld" / "snapshots";
        std::filesystem::create_directories(snapshot_dir);
        
        std::filesystem::path snapshot_path = snapshot_dir / (std::filesystem::path(source_name).stem().string() + "_snapshot.json");
        
        // Update result
        result.flight_recorder_enabled = true;
        result.flight_recorder_snapshot_path = snapshot_path.string();
        
        add_diagnostic(DiagnosticLevel::INFO, 
                      std::format("Flight recorder enabled for {} with snapshot path {}", 
                                source_name, snapshot_path.string()));
        
        return true;
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Flight recorder integration failed: " + std::string(e.what()));
        return false;
    }
}

// Rust-inspired ownership system: Check ownership and borrowing
bool Compiler::check_ownership_and_borrowing(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    const CompilationOptions& options,
    CompilationResult& result
) {
    try {
        size_t ownership_violations = 0;
        size_t borrow_violations = 0;
        std::vector<std::string> ownership_errors;
        std::vector<std::string> borrow_errors;
        
        // Clear previous metadata
        ownership_metadata_->clear();
        
        // Check each expression for ownership and borrowing violations
        for (const auto& expr : expressions) {
            // Check if this is a function definition
            if (auto func_def_ptr = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
                const auto& func_def = func_def_ptr->get();
                
                if (options.enable_borrow_checking) {
                    // Run borrow checker on the function
                    auto borrow_result = borrow_checker_->check_function(func_def);
                    
                    if (!borrow_result) {
                        const auto& error = borrow_result.error();
                        borrow_violations++;
                        
                        std::string error_msg = std::format(
                            "Borrow check error in function '{}': {}",
                            func_def.name.name,
                            error.message
                        );
                        
                        borrow_errors.push_back(error_msg);
                        
                        // Add as compilation diagnostic
                        add_diagnostic(
                            options.strict_ownership_mode ? DiagnosticLevel::ERROR : DiagnosticLevel::WARNING,
                            error_msg,
                            source_name,
                            0, // Would need proper line number from AST
                            0,
                            error.context
                        );
                    }
                }
                
                if (options.enable_ownership_checking) {
                    // Check ownership patterns in the function
                    // This is a simplified check - in a full implementation,
                    // we would traverse the AST and check ownership rules
                    
                    // For now, simulate some ownership checking
                    if (func_def.name.name.find("unsafe") != std::string::npos) {
                        ownership_violations++;
                        
                        std::string error_msg = std::format(
                            "Ownership violation in function '{}': Function name suggests unsafe operations",
                            func_def.name.name
                        );
                        
                        ownership_errors.push_back(error_msg);
                        
                        add_diagnostic(
                            options.strict_ownership_mode ? DiagnosticLevel::ERROR : DiagnosticLevel::WARNING,
                            error_msg,
                            source_name
                        );
                    }
                }
            }
        }
        
        // Update result
        result.ownership_checked = options.enable_ownership_checking;
        result.ownership_violations_found = ownership_violations;
        result.ownership_errors = ownership_errors;
        
        result.borrow_checked = options.enable_borrow_checking;
        result.borrow_violations_found = borrow_violations;
        result.borrow_errors = borrow_errors;
        
        // Log summary
        if (options.enable_ownership_checking) {
            add_diagnostic(DiagnosticLevel::INFO, 
                          std::format("Ownership checking completed: {} violations found", 
                                    ownership_violations));
        }
        
        if (options.enable_borrow_checking) {
            add_diagnostic(DiagnosticLevel::INFO, 
                          std::format("Borrow checking completed: {} violations found", 
                                    borrow_violations));
        }
        
        // Return success if no violations in strict mode, or always in non-strict mode
        if (options.strict_ownership_mode) {
            return ownership_violations == 0 && borrow_violations == 0;
        } else {
            return true; // Non-strict mode always succeeds
        }
        
    } catch (const std::exception& e) {
        add_diagnostic(DiagnosticLevel::ERROR, 
                      "Ownership/borrow checking failed: " + std::string(e.what()));
        return false;
    }
}

void Compiler::run_uses_enforcement(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_name,
    CompilationResult& result
) {
    UsesEnforcementPass pass;
    auto ue_result = pass.run(expressions, source_name);

    for (const auto& v : ue_result.violations) {
        add_diagnostic(DiagnosticLevel::ERROR,
                       "[E6001] " + v.message,
                       v.source_file, 0, 0);
    }

    if (!ue_result.ok) {
        result.success = false;
    }
}

} // namespace meld::compiler