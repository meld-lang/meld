#include "../include/meld/cli/compiler_module.hpp"
#include "../../meld-core/include/meld/parser/parser.hpp"
#include "../../meld-core/include/meld/compiler/type_checker.hpp"
#include "../../meld-core/include/meld/compiler/ast_to_ir.hpp"
#include "../../meld-core/include/meld/compiler/bytecode_generator.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace meld::cli {

// CompilationError implementation
std::string CompilationError::format() const {
    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":";
    }
    if (line > 0) {
        oss << line << ":" << column << ": ";
    }
    oss << "error: " << message;
    if (!context.empty()) {
        oss << "\n  " << context;
    }
    if (!suggestions.empty()) {
        oss << "\nSuggestions:";
        for (const auto& suggestion : suggestions) {
            oss << "\n  - " << suggestion;
        }
    }
    return oss.str();
}

// CompilerTarget base implementation
bool CompilerTarget::check_tools_available() const {
    auto tools = required_tools();
    for (const auto& tool : tools) {
        // Simple check - try to run the tool with --version
        std::string command = tool + " --version > nul 2>&1";
        int result = std::system(command.c_str());
        if (result != 0) {
            return false;
        }
    }
    return true;
}

// JvmTarget implementation
std::expected<std::string, CompilationError> 
JvmTarget::compile(const std::string& source_code, const CompileOptions& options) {
    try {
        // Parse the Meld source code
        parser::Parser parser;
        std::vector<parser::ast::expression> parsed_expressions;
        bool parse_ok = parser.parse_file(source_code, parsed_expressions);
        
        if (!parse_ok) {
            return std::unexpected(CompilationError(
                "Parse error: " + parser.error_message(),
                "", 0, 0
            ));
        }
        
        // Type check the AST
        compiler::TypeChecker type_checker;
        auto type_result = type_checker.check_program(parsed_expressions);
        
        if (!type_result) {
            std::ostringstream error_msg;
            error_msg << "Type checking failed:";
            for (const auto& error : type_result.error()) {
                error_msg << "\n  " << error.format();
            }
            return std::unexpected(CompilationError(error_msg.str()));
        }
        
        // Generate Java code
        std::ostringstream java_code;
        java_code << "// Generated from Meld source\n";
        java_code << "public class MeldProgram {\n";
        java_code << "    public static void main(String[] args) {\n";
        java_code << "        // TODO: Implement proper code generation\n";
        java_code << "        System.out.println(\"Hello from Meld!\");\n";
        java_code << "    }\n";
        java_code << "}\n";
        
        return java_code.str();
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Compilation failed: " + std::string(e.what())
        ));
    }
}

std::vector<std::string> JvmTarget::required_tools() const {
    return {"javac", "java"};
}

std::expected<std::filesystem::path, CompilationError>
JvmTarget::post_process(const std::string& generated_code, 
                       const std::filesystem::path& output_path,
                       const CompileOptions& options) {
    try {
        // Write Java source file
        std::filesystem::path java_file = output_path;
        java_file.replace_extension(".java");
        
        std::ofstream file(java_file);
        if (!file) {
            return std::unexpected(CompilationError(
                "Failed to write Java source file: " + java_file.string()
            ));
        }
        file << generated_code;
        file.close();
        
        // Compile with javac
        std::string compile_cmd = "javac \"" + java_file.string() + "\"";
        int result = std::system(compile_cmd.c_str());
        
        if (result != 0) {
            return std::unexpected(CompilationError(
                "Java compilation failed with javac"
            ));
        }
        
        // Return path to .class file
        std::filesystem::path class_file = java_file;
        class_file.replace_extension(".class");
        return class_file;
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Post-processing failed: " + std::string(e.what())
        ));
    }
}

// GoTarget implementation
std::expected<std::string, CompilationError> 
GoTarget::compile(const std::string& source_code, const CompileOptions& options) {
    try {
        // Parse the Meld source code
        parser::Parser parser;
        std::vector<parser::ast::expression> parsed_expressions;
        bool parse_ok = parser.parse_file(source_code, parsed_expressions);
        
        if (!parse_ok) {
            return std::unexpected(CompilationError(
                "Parse error: " + parser.error_message(),
                "", 0, 0
            ));
        }
        
        // Type check the AST
        compiler::TypeChecker type_checker;
        auto type_result = type_checker.check_program(parsed_expressions);
        
        if (!type_result) {
            std::ostringstream error_msg;
            error_msg << "Type checking failed:";
            for (const auto& error : type_result.error()) {
                error_msg << "\n  " << error.format();
            }
            return std::unexpected(CompilationError(error_msg.str()));
        }
        
        // Generate Go code
        std::ostringstream go_code;
        go_code << "// Generated from Meld source\n";
        go_code << "package main\n\n";
        go_code << "import \"fmt\"\n\n";
        go_code << "func main() {\n";
        go_code << "    // TODO: Implement proper code generation\n";
        go_code << "    fmt.Println(\"Hello from Meld!\")\n";
        go_code << "}\n";
        
        return go_code.str();
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Compilation failed: " + std::string(e.what())
        ));
    }
}

std::vector<std::string> GoTarget::required_tools() const {
    return {"go"};
}

std::expected<std::filesystem::path, CompilationError>
GoTarget::post_process(const std::string& generated_code, 
                      const std::filesystem::path& output_path,
                      const CompileOptions& options) {
    try {
        // Write Go source file
        std::filesystem::path go_file = output_path;
        go_file.replace_extension(".go");
        
        std::ofstream file(go_file);
        if (!file) {
            return std::unexpected(CompilationError(
                "Failed to write Go source file: " + go_file.string()
            ));
        }
        file << generated_code;
        file.close();
        
        // Compile with go build
        std::filesystem::path exe_file = output_path;
        exe_file.replace_extension(".exe");
        
        std::string compile_cmd = "go build -o \"" + exe_file.string() + "\" \"" + go_file.string() + "\"";
        int result = std::system(compile_cmd.c_str());
        
        if (result != 0) {
            return std::unexpected(CompilationError(
                "Go compilation failed with go build"
            ));
        }
        
        return exe_file;
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Post-processing failed: " + std::string(e.what())
        ));
    }
}

// CppTarget implementation
std::expected<std::string, CompilationError> 
CppTarget::compile(const std::string& source_code, const CompileOptions& options) {
    try {
        // Parse the Meld source code
        parser::Parser parser;
        std::vector<parser::ast::expression> parsed_expressions;
        bool parse_ok = parser.parse_file(source_code, parsed_expressions);
        
        if (!parse_ok) {
            return std::unexpected(CompilationError(
                "Parse error: " + parser.error_message(),
                "", 0, 0
            ));
        }
        
        // Type check the AST
        compiler::TypeChecker type_checker;
        auto type_result = type_checker.check_program(parsed_expressions);
        
        if (!type_result) {
            std::ostringstream error_msg;
            error_msg << "Type checking failed:";
            for (const auto& error : type_result.error()) {
                error_msg << "\n  " << error.format();
            }
            return std::unexpected(CompilationError(error_msg.str()));
        }
        
        // Generate C++ code
        std::ostringstream cpp_code;
        cpp_code << "// Generated from Meld source\n";
        cpp_code << "#include <iostream>\n\n";
        cpp_code << "int main() {\n";
        cpp_code << "    // TODO: Implement proper code generation\n";
        cpp_code << "    std::cout << \"Hello from Meld!\" << std::endl;\n";
        cpp_code << "    return 0;\n";
        cpp_code << "}\n";
        
        return cpp_code.str();
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Compilation failed: " + std::string(e.what())
        ));
    }
}

std::vector<std::string> CppTarget::required_tools() const {
    return {"g++", "clang++"};  // Either g++ or clang++
}

std::expected<std::filesystem::path, CompilationError>
CppTarget::post_process(const std::string& generated_code, 
                       const std::filesystem::path& output_path,
                       const CompileOptions& options) {
    try {
        // Write C++ source file
        std::filesystem::path cpp_file = output_path;
        cpp_file.replace_extension(".cpp");
        
        std::ofstream file(cpp_file);
        if (!file) {
            return std::unexpected(CompilationError(
                "Failed to write C++ source file: " + cpp_file.string()
            ));
        }
        file << generated_code;
        file.close();
        
        // Try to compile with g++ first, then clang++
        std::filesystem::path exe_file = output_path;
        exe_file.replace_extension(".exe");
        
        std::vector<std::string> compilers = {"g++", "clang++"};
        bool compiled = false;
        
        for (const auto& compiler : compilers) {
            std::string compile_cmd = compiler + " -std=c++17 -o \"" + 
                                    exe_file.string() + "\" \"" + cpp_file.string() + "\"";
            int result = std::system(compile_cmd.c_str());
            
            if (result == 0) {
                compiled = true;
                break;
            }
        }
        
        if (!compiled) {
            return std::unexpected(CompilationError(
                "C++ compilation failed with both g++ and clang++"
            ));
        }
        
        return exe_file;
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Post-processing failed: " + std::string(e.what())
        ));
    }
}

// WasmTarget implementation
std::expected<std::string, CompilationError> 
WasmTarget::compile(const std::string& source_code, const CompileOptions& options) {
    try {
        // Parse the Meld source code
        parser::Parser parser;
        std::vector<parser::ast::expression> parsed_expressions;
        bool parse_ok = parser.parse_file(source_code, parsed_expressions);
        
        if (!parse_ok) {
            return std::unexpected(CompilationError(
                "Parse error: " + parser.error_message(),
                "", 0, 0
            ));
        }
        
        // Type check the AST
        compiler::TypeChecker type_checker;
        auto type_result = type_checker.check_program(parsed_expressions);
        
        if (!type_result) {
            std::ostringstream error_msg;
            error_msg << "Type checking failed:";
            for (const auto& error : type_result.error()) {
                error_msg << "\n  " << error.format();
            }
            return std::unexpected(CompilationError(error_msg.str()));
        }
        
        // Generate WebAssembly Text format (WAT)
        std::ostringstream wat_code;
        wat_code << ";; Generated from Meld source\n";
        wat_code << "(module\n";
        wat_code << "  (import \"env\" \"print\" (func $print (param i32)))\n";
        wat_code << "  (func $main\n";
        wat_code << "    ;; TODO: Implement proper code generation\n";
        wat_code << "    i32.const 42\n";
        wat_code << "    call $print\n";
        wat_code << "  )\n";
        wat_code << "  (export \"main\" (func $main))\n";
        wat_code << ")\n";
        
        return wat_code.str();
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Compilation failed: " + std::string(e.what())
        ));
    }
}

std::vector<std::string> WasmTarget::required_tools() const {
    return {"wat2wasm"};
}

std::expected<std::filesystem::path, CompilationError>
WasmTarget::post_process(const std::string& generated_code, 
                        const std::filesystem::path& output_path,
                        const CompileOptions& options) {
    try {
        // Write WAT source file
        std::filesystem::path wat_file = output_path;
        wat_file.replace_extension(".wat");
        
        std::ofstream file(wat_file);
        if (!file) {
            return std::unexpected(CompilationError(
                "Failed to write WAT source file: " + wat_file.string()
            ));
        }
        file << generated_code;
        file.close();
        
        // Compile with wat2wasm
        std::filesystem::path wasm_file = output_path;
        wasm_file.replace_extension(".wasm");
        
        std::string compile_cmd = "wat2wasm \"" + wat_file.string() + "\" -o \"" + wasm_file.string() + "\"";
        int result = std::system(compile_cmd.c_str());
        
        if (result != 0) {
            return std::unexpected(CompilationError(
                "WebAssembly compilation failed with wat2wasm"
            ));
        }
        
        return wasm_file;
        
    } catch (const std::exception& e) {
        return std::unexpected(CompilationError(
            "Post-processing failed: " + std::string(e.what())
        ));
    }
}

// CompilerModule implementation
CompilerModule::CompilerModule() 
    : BaseCommandHandler("compile", "Compile Meld source code to various target platforms") {
    
    // Initialize compilation targets
    targets_[CompilationTarget::JVM] = std::make_unique<JvmTarget>();
    targets_[CompilationTarget::Go] = std::make_unique<GoTarget>();
    targets_[CompilationTarget::Cpp] = std::make_unique<CppTarget>();
    targets_[CompilationTarget::WebAssembly] = std::make_unique<WasmTarget>();
}

CommandResult CompilerModule::execute(const CommandArgs& args) {
    try {
        // Handle "meld build" subcommand — bytecode pipeline
        if (args.command == "build") {
            if (args.positional.empty()) {
                std::cerr << "error: no source file specified\n";
                std::cerr << "Usage: meld build [--optimize] [--output=<path>] <file.meld>" << std::endl;
                return CommandResult::InvalidArguments;
            }

            std::filesystem::path source_file = args.positional[0];

            if (!std::filesystem::exists(source_file)) {
                std::cerr << "error: file not found: " << source_file << std::endl;
                return CommandResult::Error;
            }

            // Warn on non-.meld extension
            if (source_file.extension() != ".meld") {
                std::cerr << "warning: file does not have .meld extension: " << source_file << std::endl;
            }

            bool optimize = args.flags.count("optimize") > 0;

            // Debug info: on by default, --release strips it, --debug is explicit
            bool release_mode = args.flags.count("release") > 0;
            bool debug_flag = args.flags.count("debug") > 0;
            bool debug_info = debug_flag || !release_mode;  // default to debug unless --release

            std::filesystem::path output_path;
            if (args.options.count("output")) {
                output_path = args.options.at("output");
            } else {
                output_path = source_file;
                output_path.replace_extension(".meldc");
            }

            return build_to_bytecode(source_file, output_path, optimize, debug_info);
        }

        // Original "meld compile" path — cross-compilation targets
        // Parse compilation options
        auto options = parse_compile_options(args);
        
        // Get source file from positional arguments
        if (args.positional.empty()) {
            std::cerr << "Error: No source file specified\n";
            std::cerr << get_usage() << std::endl;
            return CommandResult::InvalidArguments;
        }
        
        std::filesystem::path source_file = args.positional[0];
        
        // Check if source file exists
        if (!std::filesystem::exists(source_file)) {
            std::cerr << "Error: Source file does not exist: " << source_file << std::endl;
            return CommandResult::Error;
        }
        
        // Compile the file
        auto result = compile_file(source_file, options);
        
        // Print warnings
        if (result.has_warnings()) {
            print_compilation_warnings(result);
        }
        
        // Print errors and return appropriate result
        if (result.has_errors()) {
            print_compilation_errors(result);
            return CommandResult::Error;
        }
        
        // Success
        std::cout << "Compilation successful: " << result.output_file << std::endl;
        return CommandResult::Success;
        
    } catch (const std::exception& e) {
        std::cerr << "Compilation failed: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

std::string CompilerModule::get_help() const {
    std::ostringstream help;
    help << "Compile Meld source code to various target platforms.\n\n";
    help << get_usage() << "\n\n";
    help << "Options:\n";
    help << "  --target=TARGET    Compilation target (jvm, go, cpp, wasm) [default: jvm]\n";
    help << "  --output=PATH      Output file path\n";
    help << "  --debug            Include debug information (DWARF/PDB)\n";
    help << "  --release          Strip debug info, optimize for release\n";
    help << "  --optimize=LEVEL   Optimization level (0-3) [default: 0]\n";
    help << "  --warnings-as-errors  Treat warnings as errors\n";
    help << "  --include=PATH     Add include path\n";
    help << "  --define=KEY=VALUE Define preprocessor macro\n\n";
    help << "Build subcommand (meld build):\n";
    help << "  --debug            Emit DWARF/PDB debug info (default unless --release)\n";
    help << "  --release          Strip debug info from output\n";
    help << "  --optimize         Apply bytecode optimization passes\n";
    help << "  --output=PATH      Output file path [default: <basename>.meldc]\n\n";
    help << "Available targets:\n";
    for (const auto& target : get_available_targets()) {
        help << "  " << target << "\n";
    }
    return help.str();
}

std::string CompilerModule::get_usage() const {
    return "meld compile [options] <source-file>";
}

std::vector<std::string> CompilerModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    // Complete target names
    if (partial.starts_with("--target=")) {
        std::string target_partial = partial.substr(9);
        for (const auto& target : get_available_targets()) {
            if (target.starts_with(target_partial)) {
                completions.push_back("--target=" + target);
            }
        }
    }
    // Complete option names
    else if (partial.starts_with("--")) {
        std::vector<std::string> options = {
            "--target=", "--output=", "--debug", "--release", "--optimize=", 
            "--warnings-as-errors", "--include=", "--define="
        };
        for (const auto& option : options) {
            if (option.starts_with(partial)) {
                completions.push_back(option);
            }
        }
    }
    // Complete file names
    else {
        // TODO: Implement file completion
        // For now, just return empty
    }
    
    return completions;
}

bool CompilerModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    // --agent-test is a runtime flag, not valid for compilation (Req 19.8)
    if (args.flags.count("agent-test") > 0) {
        error_message = "--agent-test is a runtime flag and cannot be used with meld build";
        return false;
    }

    // Check for required source file
    if (args.positional.empty()) {
        error_message = "No source file specified";
        return false;
    }
    
    // Validate target if specified
    if (args.options.contains("target")) {
        auto target_result = parse_target(args.options.at("target"));
        if (!target_result) {
            error_message = "Invalid target: " + target_result.error();
            return false;
        }
    }
    
    // Validate optimization level if specified
    if (args.options.contains("optimize")) {
        try {
            int level = std::stoi(args.options.at("optimize"));
            if (level < 0 || level > 3) {
                error_message = "Optimization level must be between 0 and 3";
                return false;
            }
        } catch (const std::exception&) {
            error_message = "Invalid optimization level: " + args.options.at("optimize");
            return false;
        }
    }
    
    return true;
}

CompilationResult CompilerModule::compile_file(const std::filesystem::path& source_file,
                                              const CompileOptions& options) {
    CompilationResult result;
    
    try {
        // Read source file
        std::string source_code = read_source_file(source_file);
        
        // Compile source code
        result = compile_source(source_code, options);
        
        // Set output file if not already set
        if (result.success && result.output_file.empty()) {
            result.output_file = get_default_output_path(source_file, options.target);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.emplace_back("Failed to compile file: " + std::string(e.what()));
    }
    
    return result;
}

CompilationResult CompilerModule::compile_source(const std::string& source_code,
                                               const CompileOptions& options) {
    CompilationResult result;
    
    try {
        // Get the appropriate compiler target
        auto target_it = targets_.find(options.target);
        if (target_it == targets_.end()) {
            result.errors.emplace_back("Unsupported compilation target");
            return result;
        }
        
        auto& target = target_it->second;
        
        // Check if required tools are available
        if (!target->check_tools_available()) {
            std::ostringstream error_msg;
            error_msg << "Required tools not available for target '" << target->name() << "': ";
            auto tools = target->required_tools();
            for (size_t i = 0; i < tools.size(); ++i) {
                if (i > 0) error_msg << ", ";
                error_msg << tools[i];
            }
            result.errors.emplace_back(error_msg.str());
            return result;
        }
        
        // Compile to target format
        auto compile_result = target->compile(source_code, options);
        if (!compile_result) {
            result.errors.push_back(compile_result.error());
            return result;
        }
        
        result.generated_code = *compile_result;
        
        // Post-process if output path is specified
        if (!options.output_path.empty()) {
            auto post_result = target->post_process(result.generated_code, options.output_path, options);
            if (!post_result) {
                result.errors.push_back(post_result.error());
                return result;
            }
            result.output_file = *post_result;
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.emplace_back("Compilation failed: " + std::string(e.what()));
    }
    
    return result;
}

std::vector<std::string> CompilerModule::get_available_targets() const {
    return {"jvm", "go", "cpp", "wasm"};
}

std::expected<CompilationTarget, std::string> 
CompilerModule::parse_target(const std::string& target_str) const {
    if (target_str == "jvm") return CompilationTarget::JVM;
    if (target_str == "go") return CompilationTarget::Go;
    if (target_str == "cpp" || target_str == "c++") return CompilationTarget::Cpp;
    if (target_str == "wasm" || target_str == "webassembly") return CompilationTarget::WebAssembly;
    
    return std::unexpected("Unknown target: " + target_str + 
                          ". Available targets: jvm, go, cpp, wasm");
}

std::filesystem::path CompilerModule::get_default_output_path(const std::filesystem::path& source_file,
                                                            CompilationTarget target) const {
    auto target_it = targets_.find(target);
    if (target_it == targets_.end()) {
        return source_file.stem();
    }
    
    std::filesystem::path output = source_file.stem();
    output += target_it->second->file_extension();
    return output;
}

// Private helper methods
CompileOptions CompilerModule::parse_compile_options(const CommandArgs& args) const {
    CompileOptions options;
    
    // Parse target
    if (args.options.contains("target")) {
        auto target_result = parse_target(args.options.at("target"));
        if (target_result) {
            options.target = *target_result;
        }
    }
    
    // Parse output path
    if (args.options.contains("output")) {
        options.output_path = args.options.at("output");
    }
    
    // Parse debug flag
    options.debug_info = args.flags.contains("debug");
    
    // Parse optimization level
    if (args.options.contains("optimize")) {
        try {
            options.optimization_level = std::stoi(args.options.at("optimize"));
        } catch (const std::exception&) {
            // Use default value
        }
    }
    
    // Parse warnings as errors
    options.warnings_as_errors = args.flags.contains("warnings-as-errors");
    
    // Parse include paths
    for (const auto& [key, value] : args.options) {
        if (key == "include") {
            options.include_paths.emplace_back(value);
        }
    }
    
    // Parse defines
    for (const auto& [key, value] : args.options) {
        if (key == "define") {
            // Parse KEY=VALUE format
            size_t eq_pos = value.find('=');
            if (eq_pos != std::string::npos) {
                std::string define_key = value.substr(0, eq_pos);
                std::string define_value = value.substr(eq_pos + 1);
                options.defines[define_key] = define_value;
            } else {
                options.defines[value] = "1";  // Default value
            }
        }
    }
    
    return options;
}

void CompilerModule::print_compilation_errors(const CompilationResult& result) const {
    for (const auto& error : result.errors) {
        std::cerr << error.format() << std::endl;
    }
}

void CompilerModule::print_compilation_warnings(const CompilationResult& result) const {
    for (const auto& warning : result.warnings) {
        std::cout << "warning: " << warning.format() << std::endl;
    }
}

std::string CompilerModule::read_source_file(const std::filesystem::path& file_path) const {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("Cannot open source file: " + file_path.string());
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool CompilerModule::write_output_file(const std::filesystem::path& output_path, 
                                      const std::string& content) const {
    try {
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_path.parent_path());
        
        std::ofstream file(output_path);
        if (!file) {
            return false;
        }
        
        file << content;
        return file.good();
        
    } catch (const std::exception&) {
        return false;
    }
}

CommandResult CompilerModule::build_to_bytecode(const std::filesystem::path& source_file,
                                                 const std::filesystem::path& output_path,
                                                 bool optimize,
                                                 bool debug_info) {
    try {
        // 1. Read source file
        std::string source = read_source_file(source_file);

        // 2. Parse
        parser::Parser parser;
        std::vector<parser::ast::expression> ast;
        if (!parser.parse_file(source, ast)) {
            std::cerr << source_file.string() << ": error: " << parser.error_message() << std::endl;
            return CommandResult::Error;
        }

        // 3. AST-to-IR lowering
        auto ir_module = std::make_shared<compiler::ir::Module>(source_file.stem().string());
        compiler::ASTToIR lowerer(ir_module);

        // Configure debug info emission (Req 12B)
        if (debug_info) {
            lowerer.set_source_file(source_file.string(),
                                    source_file.has_parent_path()
                                        ? source_file.parent_path().string()
                                        : ".");
            lowerer.set_debug_info_enabled(true);
        }

        auto lower_result = lowerer.transform_program(ast);
        if (!lower_result) {
            std::cerr << source_file.string() << ": error: IR lowering failed: "
                      << lower_result.error().format() << std::endl;
            return CommandResult::Error;
        }

        // 4. Bytecode generation
        compiler::BytecodeGenerator generator;
        auto bc_module = generator.generate(**lower_result);

        // 5. Optional optimization
        if (optimize) {
            compiler::BytecodeOptimizer optimizer;
            optimizer.optimize(bc_module);
        }

        // 6. Serialize and write
        auto bytes = bc_module.serialize();

        // Create output directory if needed
        if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
            std::filesystem::create_directories(output_path.parent_path());
        }

        std::ofstream out(output_path, std::ios::binary);
        if (!out) {
            std::cerr << "error: cannot write output file: " << output_path << std::endl;
            return CommandResult::Error;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        out.close();

        std::cout << "Built " << output_path.string()
                  << " (" << bytes.size() << " bytes";
        if (optimize) std::cout << ", optimized";
        if (debug_info) std::cout << ", debug info";
        std::cout << ")" << std::endl;

        return CommandResult::Success;

    } catch (const std::exception& e) {
        std::cerr << source_file.string() << ": error: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

} // namespace meld::cli