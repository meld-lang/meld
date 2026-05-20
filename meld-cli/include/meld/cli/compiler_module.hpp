#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <expected>
#include <cstdint>

namespace meld::cli {

/**
 * Compilation target platforms
 */
enum class CompilationTarget {
    JVM,        // JVM bytecode
    Go,         // Go source code
    Cpp,        // C++17 source code
    WebAssembly // WebAssembly
};

/**
 * Compilation options
 */
struct CompileOptions {
    CompilationTarget target = CompilationTarget::JVM;
    std::filesystem::path output_path;
    bool debug_info = false;
    int optimization_level = 0;
    bool warnings_as_errors = false;
    std::vector<std::filesystem::path> include_paths;
    std::map<std::string, std::string> defines;
};

/**
 * Compilation error information
 */
struct CompilationError {
    std::string message;
    std::string file;
    size_t line = 0;
    size_t column = 0;
    std::string context;
    std::vector<std::string> suggestions;
    
    CompilationError(std::string msg, std::string f = "", size_t l = 0, size_t c = 0)
        : message(std::move(msg)), file(std::move(f)), line(l), column(c) {}
    
    std::string format() const;
};

/**
 * Compilation result
 */
struct CompilationResult {
    bool success = false;
    std::filesystem::path output_file;
    std::vector<CompilationError> errors;
    std::vector<CompilationError> warnings;
    std::string generated_code;
    
    bool has_errors() const { return !errors.empty(); }
    bool has_warnings() const { return !warnings.empty(); }
};

/**
 * Abstract base class for compilation targets
 */
class CompilerTarget {
public:
    virtual ~CompilerTarget() = default;
    
    /**
     * Get the target name
     */
    virtual std::string name() const = 0;
    
    /**
     * Get the file extension for output files
     */
    virtual std::string file_extension() const = 0;
    
    /**
     * Compile Meld source to target format
     */
    virtual std::expected<std::string, CompilationError> 
    compile(const std::string& source_code, const CompileOptions& options) = 0;
    
    /**
     * Get required external tools for this target
     */
    virtual std::vector<std::string> required_tools() const = 0;
    
    /**
     * Check if required tools are available
     */
    virtual bool check_tools_available() const;
    
    /**
     * Post-process generated code (e.g., run javac, go build)
     */
    virtual std::expected<std::filesystem::path, CompilationError>
    post_process(const std::string& generated_code, 
                const std::filesystem::path& output_path,
                const CompileOptions& options) = 0;
};

/**
 * JVM bytecode compilation target
 */
class JvmTarget : public CompilerTarget {
public:
    std::string name() const override { return "jvm"; }
    std::string file_extension() const override { return ".java"; }
    
    std::expected<std::string, CompilationError> 
    compile(const std::string& source_code, const CompileOptions& options) override;
    
    std::vector<std::string> required_tools() const override;
    
    std::expected<std::filesystem::path, CompilationError>
    post_process(const std::string& generated_code, 
                const std::filesystem::path& output_path,
                const CompileOptions& options) override;
};

/**
 * Go source code compilation target
 */
class GoTarget : public CompilerTarget {
public:
    std::string name() const override { return "go"; }
    std::string file_extension() const override { return ".go"; }
    
    std::expected<std::string, CompilationError> 
    compile(const std::string& source_code, const CompileOptions& options) override;
    
    std::vector<std::string> required_tools() const override;
    
    std::expected<std::filesystem::path, CompilationError>
    post_process(const std::string& generated_code, 
                const std::filesystem::path& output_path,
                const CompileOptions& options) override;
};

/**
 * C++17 source code compilation target
 */
class CppTarget : public CompilerTarget {
public:
    std::string name() const override { return "cpp"; }
    std::string file_extension() const override { return ".cpp"; }
    
    std::expected<std::string, CompilationError> 
    compile(const std::string& source_code, const CompileOptions& options) override;
    
    std::vector<std::string> required_tools() const override;
    
    std::expected<std::filesystem::path, CompilationError>
    post_process(const std::string& generated_code, 
                const std::filesystem::path& output_path,
                const CompileOptions& options) override;
};

/**
 * WebAssembly compilation target
 */
class WasmTarget : public CompilerTarget {
public:
    std::string name() const override { return "wasm"; }
    std::string file_extension() const override { return ".wasm"; }
    
    std::expected<std::string, CompilationError> 
    compile(const std::string& source_code, const CompileOptions& options) override;
    
    std::vector<std::string> required_tools() const override;
    
    std::expected<std::filesystem::path, CompilationError>
    post_process(const std::string& generated_code, 
                const std::filesystem::path& output_path,
                const CompileOptions& options) override;
};

/**
 * Compiler module - handles compilation to various targets
 */
class CompilerModule : public BaseCommandHandler {
public:
    CompilerModule();
    ~CompilerModule() override = default;
    
    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
    /**
     * Compile a source file to the specified target
     */
    CompilationResult compile_file(const std::filesystem::path& source_file,
                                  const CompileOptions& options);
    
    /**
     * Compile source code string to the specified target
     */
    CompilationResult compile_source(const std::string& source_code,
                                   const CompileOptions& options);
    
    /**
     * Get available compilation targets
     */
    std::vector<std::string> get_available_targets() const;
    
    /**
     * Parse target string to CompilationTarget enum
     */
    std::expected<CompilationTarget, std::string> parse_target(const std::string& target_str) const;
    
    /**
     * Get default output path for a source file and target
     */
    std::filesystem::path get_default_output_path(const std::filesystem::path& source_file,
                                                 CompilationTarget target) const;

    /**
     * Build a .meld file to bytecode (.meldc)
     * Pipeline: parse → AST-to-IR → BytecodeGenerator → (optional optimize) → serialize → write
     * @param debug_info  Emit DWARF/PDB debug info metadata in the IR (Req 12B)
     */
    CommandResult build_to_bytecode(const std::filesystem::path& source_file,
                                    const std::filesystem::path& output_path,
                                    bool optimize,
                                    bool debug_info = true);

private:
    std::map<CompilationTarget, std::unique_ptr<CompilerTarget>> targets_;
    
    // Helper methods
    CompileOptions parse_compile_options(const CommandArgs& args) const;
    void print_compilation_errors(const CompilationResult& result) const;
    void print_compilation_warnings(const CompilationResult& result) const;
    std::string read_source_file(const std::filesystem::path& file_path) const;
    bool write_output_file(const std::filesystem::path& output_path, 
                          const std::string& content) const;
};

} // namespace meld::cli