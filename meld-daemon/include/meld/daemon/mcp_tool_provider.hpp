#pragma once

#include "meld/daemon/dependency_graph.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/vector_index.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace meld::daemon {

// ============================================================================
// Req 22: MCP Codebase Exploration — result types
// ============================================================================

/// Discovered project/workspace info (Req 22.1).
struct ProjectInfo {
    std::string name;
    std::filesystem::path root;
    size_t file_count{0};
    std::vector<std::string> modules;
};

/// Hierarchical codebase structure node (Req 22.2).
struct CodebaseNode {
    std::string name;
    std::string kind;  // "file", "module", "symbol"
    std::string type_info;
    std::vector<CodebaseNode> children;
};

/// Code artifact exposure result (Req 22.3).
struct CodeArtifact {
    std::string name;
    std::string kind;
    std::string type_info;
    std::string ast_kind;
    std::vector<std::string> effects;
    uint32_t line{0};
    uint32_t column{0};
};

/// Dependency analysis result (Req 22.4).
struct DependencyAnalysis {
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    std::vector<DependencyNode> dependencies;
};

/// Documentation extraction result (Req 22.5).
struct DocumentationEntry {
    std::string symbol_name;
    std::string doc_text;
    std::string kind;  // "comment", "docstring", "annotation"
};

// ============================================================================
// Req 23: MCP Code Semantic Analysis — result types
// ============================================================================

/// Grammar parse result (Req 23.1).
struct ParseResult {
    bool success{false};
    std::string ast_kind;
    std::string detail;
    std::vector<std::string> child_kinds;
};

/// Type analysis result (Req 23.2).
struct TypeAnalysis {
    std::string symbol_name;
    std::string qualified_type;
    bool has_refinement{false};
    bool has_dispatch{false};
    std::vector<std::string> type_params;
    std::vector<std::string> constraints;
};

/// Control flow analysis result (Req 23.3).
struct ControlFlowAnalysis {
    std::string function_name;
    std::vector<std::string> branches;   // "pattern_match", "if_else", "guard"
    std::vector<std::string> constructs; // "map", "filter", "fold", "pipe"
    bool has_pattern_match{false};
    bool has_functional_constructs{false};
};

/// Macro processing result (Req 23.4).
struct MacroProcessing {
    std::string macro_name;
    std::string macro_kind;  // "syntax", "derive", "attribute", "meta"
    std::vector<std::string> parameters;
    std::string expansion_hint;
    bool valid{false};
};

/// Code validation result (Req 23.5).
struct ValidationResult {
    bool valid{true};
    std::vector<Diagnostic> syntax_errors;
    std::vector<Diagnostic> type_errors;
    std::vector<Diagnostic> semantic_errors;
};

// ============================================================================
// Req 24: MCP Code Search and Query — result types
// ============================================================================

/// Symbol search result (Req 24.1).
struct SymbolSearchResult {
    std::string name;
    std::string kind;
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
    std::string usage;  // "definition", "reference", "import"
};

/// Type signature search result (Req 24.2).
struct TypeSignatureResult {
    std::string name;
    std::string type_signature;
    std::filesystem::path file;
    uint32_t line{0};
};

/// Code pattern result (Req 24.3).
struct CodePatternResult {
    std::string pattern_kind;  // "idiom", "construct", "approach"
    std::string description;
    std::filesystem::path file;
    uint32_t line{0};
    std::string matched_code;
};

/// Language feature filter result (Req 24.4).
struct LanguageFeatureResult {
    std::string feature;  // "multiple_dispatch", "refinement_type", "pattern_match", etc.
    std::string symbol_name;
    std::filesystem::path file;
    uint32_t line{0};
    std::string detail;
};

// ============================================================================
// Req 25: MCP Code Generation and Validation — result types
// ============================================================================

/// Generated code snippet with validation (Req 25.1).
struct GeneratedSnippet {
    std::string code;
    bool syntax_valid{false};
    std::vector<std::string> grammar_errors;
};

/// Generated type definition (Req 25.2).
struct GeneratedTypeDef {
    std::string code;
    std::string type_name;
    bool type_safe{false};
    std::vector<std::string> constraint_errors;
};

/// Generated function implementation (Req 25.3).
struct GeneratedFunction {
    std::string code;
    std::string function_name;
    std::string signature;
    bool dispatch_compatible{false};
    std::vector<std::string> dispatch_errors;
};

/// Generated module (Req 25.4).
struct GeneratedModule {
    std::string code;
    std::string module_name;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    bool import_export_consistent{false};
    std::vector<std::string> dependency_errors;
};

/// Formatted code result (Req 25.5).
struct FormattedCode {
    std::string code;
    bool formatting_applied{false};
    std::vector<std::string> style_warnings;
};

// ============================================================================
// Req 26: MCP Code Transformation — result types
// ============================================================================

/// Symbol rename result (Req 26.1).
struct RenameResult {
    std::string old_name;
    std::string new_name;
    size_t references_updated{0};
    bool semantically_correct{false};
    std::vector<std::string> errors;
};

/// Function extraction result (Req 26.2).
struct ExtractedFunction {
    std::string function_name;
    std::string signature;
    std::string body;
    bool scoping_correct{false};
    bool dispatch_compatible{false};
    std::vector<std::string> errors;
};

/// Module reorganization result (Req 26.3).
struct ReorganizedModule {
    std::string module_name;
    std::vector<std::string> updated_imports;
    std::vector<std::string> updated_exports;
    bool dependencies_consistent{false};
    std::vector<std::string> errors;
};

/// Design pattern application result (Req 26.4).
struct PatternApplication {
    std::string pattern_name;
    std::string transformed_code;
    bool behavior_preserved{false};
    bool meld_idiomatic{false};
    std::vector<std::string> errors;
};

/// Code optimization result (Req 26.5).
struct OptimizationResult {
    std::string original_code;
    std::string optimized_code;
    std::string optimization_kind;
    bool functionally_equivalent{false};
    std::vector<std::string> suggestions;
};

// ============================================================================
// Req 27: MCP Project Metadata — result types
// ============================================================================

/// Project configuration (Req 27.1).
struct ProjectConfig {
    std::string project_name;
    std::vector<std::string> build_settings;
    std::vector<std::string> dependencies;
    std::vector<std::string> compilation_options;
};

/// Project structure (Req 27.2).
struct ProjectStructure {
    std::string root_dir;
    std::vector<std::string> directories;
    std::vector<std::string> module_hierarchy;
    std::vector<std::pair<std::string, std::string>> file_relationships;
};

/// Build artifact info (Req 27.3).
struct BuildArtifact {
    std::string name;
    std::string kind;  // "object", "library", "binary", "intermediate"
    std::filesystem::path path;
    size_t size_bytes{0};
};

/// Version control info (Req 27.4).
struct VersionControlInfo {
    std::string current_branch;
    std::string head_commit;
    std::vector<std::string> recent_commits;
    std::vector<std::string> modified_files;
};

/// Test suite info (Req 27.5).
struct TestSuiteInfo {
    std::string suite_name;
    std::vector<std::string> test_files;
    std::vector<std::string> test_cases;
    double coverage_percent{0.0};
};

// ============================================================================
// Req 28: MCP Development Tool Integration — result types
// ============================================================================

/// Build system integration result (Req 28.1).
struct BuildSystemInfo {
    std::string build_system;  // "bazel"
    std::vector<std::string> targets;
    std::vector<std::string> rules;
};

/// Compiler output (Req 28.2).
struct CompilerOutput {
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
    std::string human_summary;
    std::string agent_context;
};

/// Test execution result (Req 28.4).
struct TestExecutionResult {
    std::string test_name;
    bool passed{false};
    std::string output;
    double duration_ms{0.0};
    double coverage_percent{0.0};
};

/// Documentation generation result (Req 28.5).
struct DocumentationOutput {
    std::string symbol_name;
    std::string api_doc;
    std::vector<std::string> code_examples;
    bool up_to_date{false};
};

// ============================================================================
// McpToolProvider — unified MCP tool provider for Req 22–28
// ============================================================================

/// Provides MCP tool functionality for codebase exploration (Req 22),
/// semantic analysis (Req 23), code search/query (Req 24),
/// code generation/validation (Req 25), code transformation (Req 26),
/// project metadata (Req 27), and tool integration (Req 28).
/// Reads from the shared SemanticModel, DependencyGraph, and VectorIndex.
class McpToolProvider {
public:
    McpToolProvider(const SemanticModel& model,
                    const DependencyGraph& dep_graph,
                    const VectorIndex& vector_index);
    ~McpToolProvider() = default;

    // --- Req 22: Codebase Exploration ---

    /// Discover available projects and workspaces (Req 22.1).
    std::vector<ProjectInfo> discover_projects() const;

    /// Get hierarchical codebase structure (Req 22.2).
    CodebaseNode get_codebase_structure() const;

    /// Expose code artifacts (AST, symbols, types) for a file (Req 22.3).
    std::vector<CodeArtifact> get_code_artifacts(
        const std::filesystem::path& file) const;

    /// Analyze dependencies for a file (Req 22.4).
    DependencyAnalysis analyze_dependencies(
        const std::filesystem::path& file) const;

    /// Extract documentation from a file (Req 22.5).
    std::vector<DocumentationEntry> extract_documentation(
        const std::filesystem::path& file) const;

    // --- Req 23: Code Semantic Analysis ---

    /// Parse code and provide AST info (Req 23.1).
    ParseResult parse_code(const std::filesystem::path& file,
                           const std::string& node_name) const;

    /// Analyze types for a symbol (Req 23.2).
    TypeAnalysis analyze_type(const std::filesystem::path& file,
                              const std::string& symbol_name) const;

    /// Analyze control flow for a function (Req 23.3).
    ControlFlowAnalysis analyze_control_flow(
        const std::filesystem::path& file,
        const std::string& function_name) const;

    /// Process a macro definition (Req 23.4).
    MacroProcessing process_macro(const std::filesystem::path& file,
                                  const std::string& macro_name) const;

    /// Validate code correctness for a file (Req 23.5).
    ValidationResult validate_code(const std::filesystem::path& file) const;

    // --- Req 24: Code Search and Query ---

    /// Search for a symbol by name across the codebase (Req 24.1).
    std::vector<SymbolSearchResult> search_symbol(
        const std::string& symbol_name) const;

    /// Search by type signature pattern (Req 24.2).
    std::vector<TypeSignatureResult> search_type_signature(
        const std::string& type_pattern) const;

    /// Identify code patterns (Req 24.3).
    std::vector<CodePatternResult> find_code_patterns(
        const std::string& pattern) const;

    /// Filter by language feature (Req 24.4).
    std::vector<LanguageFeatureResult> filter_by_feature(
        const std::string& feature) const;

    // --- Req 25: Code Generation and Validation ---

    /// Generate and validate a code snippet (Req 25.1).
    GeneratedSnippet generate_snippet(const std::string& description,
                                      const std::string& context) const;

    /// Generate a type definition (Req 25.2).
    GeneratedTypeDef generate_type_definition(
        const std::string& type_name,
        const std::vector<std::string>& fields,
        const std::vector<std::string>& constraints) const;

    /// Generate a function implementation (Req 25.3).
    GeneratedFunction generate_function(
        const std::string& function_name,
        const std::string& signature,
        const std::vector<std::string>& existing_overloads) const;

    /// Generate a complete module (Req 25.4).
    GeneratedModule generate_module(
        const std::string& module_name,
        const std::vector<std::string>& imports,
        const std::vector<std::string>& symbols) const;

    /// Format generated code (Req 25.5).
    FormattedCode format_code(const std::string& code) const;

    // --- Req 26: Code Transformation ---

    /// Rename a symbol across the codebase (Req 26.1).
    RenameResult rename_symbol(const std::filesystem::path& file,
                               const std::string& old_name,
                               const std::string& new_name) const;

    /// Extract a function from code (Req 26.2).
    ExtractedFunction extract_function(
        const std::filesystem::path& file,
        const std::string& source_function,
        const std::string& new_function_name,
        uint32_t start_line, uint32_t end_line) const;

    /// Reorganize a module (Req 26.3).
    ReorganizedModule reorganize_module(
        const std::filesystem::path& file,
        const std::vector<std::string>& new_imports,
        const std::vector<std::string>& new_exports) const;

    /// Apply a design pattern (Req 26.4).
    PatternApplication apply_pattern(
        const std::filesystem::path& file,
        const std::string& pattern_name) const;

    /// Optimize code (Req 26.5).
    OptimizationResult optimize_code(
        const std::filesystem::path& file,
        const std::string& function_name) const;

    // --- Req 27: Project Metadata ---

    /// Access project configuration (Req 27.1).
    ProjectConfig get_project_config() const;

    /// Examine project structure (Req 27.2).
    ProjectStructure get_project_structure() const;

    /// Analyze build artifacts (Req 27.3).
    std::vector<BuildArtifact> get_build_artifacts() const;

    /// Access version control information (Req 27.4).
    VersionControlInfo get_version_control_info() const;

    /// Examine test suites (Req 27.5).
    std::vector<TestSuiteInfo> get_test_suites() const;

    // --- Req 28: Development Tool Integration ---

    /// Integrate with build system (Req 28.1).
    BuildSystemInfo get_build_system_info() const;

    /// Access compiler outputs (Req 28.2).
    CompilerOutput get_compiler_output(
        const std::filesystem::path& file) const;

    /// Execute tests and get results (Req 28.4).
    std::vector<TestExecutionResult> run_tests(
        const std::string& test_pattern) const;

    /// Generate documentation (Req 28.5).
    DocumentationOutput generate_documentation(
        const std::filesystem::path& file,
        const std::string& symbol_name) const;

private:
    const SemanticModel& model_;
    const DependencyGraph& dep_graph_;
    const VectorIndex& vector_index_;

    /// Recursively collect artifacts from an AST node.
    void collect_artifacts(const std::shared_ptr<ASTNode>& node,
                           const std::filesystem::path& file,
                           std::vector<CodeArtifact>& out) const;

    /// Recursively build codebase structure from an AST node.
    CodebaseNode build_structure_node(const std::shared_ptr<ASTNode>& node) const;

    /// Find a named node in a file's AST.
    std::shared_ptr<ASTNode> find_node(const std::filesystem::path& file,
                                       const std::string& name) const;

    /// Recursive node search.
    std::shared_ptr<ASTNode> find_node_recursive(
        const std::shared_ptr<ASTNode>& root,
        const std::string& name) const;

    /// Classify AST kind to a feature category.
    static std::string classify_feature(const std::string& ast_kind);

    /// Check if a type string matches a pattern (substring match).
    static bool type_matches_pattern(const std::string& type_sig,
                                     const std::string& pattern);

    /// Case-insensitive substring match.
    static bool icontains(const std::string& haystack, const std::string& needle);

    /// Classify a node's usage type.
    static std::string classify_usage(const std::string& ast_kind);

    /// Extract documentation from AST node metadata.
    static std::string extract_doc_text(const std::shared_ptr<ASTNode>& node);

    /// Detect control flow constructs in children.
    void detect_control_flow(const std::shared_ptr<ASTNode>& node,
                             ControlFlowAnalysis& result) const;

    /// Validate syntax of a code string (simplified grammar check).
    static bool validate_syntax(const std::string& code,
                                std::vector<std::string>& errors);

    /// Check if a name is a valid Meld identifier.
    static bool is_valid_identifier(const std::string& name);

    /// Apply Meld formatting conventions to code.
    static std::string apply_formatting(const std::string& code);

    /// Count references to a symbol across all files.
    size_t count_references(const std::string& symbol_name) const;
};

}  // namespace meld::daemon
