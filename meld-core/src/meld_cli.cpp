#include "meld/compiler/compiler.hpp"
#include "meld/compiler/codegen.hpp"
#include "meld/macro/blueprint.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <format>
#include <filesystem>

using namespace meld;

struct CLIOptions {
    std::vector<std::string> input_files;
    std::string target = "compile";  // compile, mcp, jvm
    std::string output_dir = "./output";
    bool verbose = false;
    bool help = false;
    
    // Trust level enforcement options
    std::optional<double> trust_level;  // Minimum trust level required (0.0 to 1.0)
    
    // MCP-specific options
    std::string mcp_server_name = "meld-mcp-server";
    std::string mcp_server_version = "1.0.0";
    bool include_private_functions = false;
    bool include_test_functions = false;
    std::vector<std::string> include_namespaces;
    std::vector<std::string> exclude_functions;
    
    // JVM-specific options
    std::string jvm_package_name = "meld.generated";
    bool jvm_use_modern_java = true;
    int jvm_java_version = 17;
    
    // Effects annotation strict mode
    bool strict_no_keywords = false;  // Reject keyword-based effect syntax
};

void print_help() {
    std::cout << "Meld Compiler with Multi-Target Support\n\n";
    std::cout << "Usage: meld [options] <input-files>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --target=<target>        Compilation target (compile, mcp, jvm) [default: compile]\n";
    std::cout << "  --output=<dir>           Output directory [default: ./output]\n";
    std::cout << "  --trust-level=<level>    Minimum trust level required (0.0-1.0)\n";
    std::cout << "  --verbose                Enable verbose output\n";
    std::cout << "  --help                   Show this help message\n\n";
    std::cout << "Trust Level Values:\n";
    std::cout << "  1.0                      VERIFIED - Human-verified code only\n";
    std::cout << "  0.9                      HIGH_CONFIDENCE - High-confidence AI code and above\n";
    std::cout << "  0.7                      MEDIUM_CONFIDENCE - Medium-confidence AI code and above\n";
    std::cout << "  0.5                      LOW_CONFIDENCE - Low-confidence AI code and above\n";
    std::cout << "  0.0                      UNTRUSTED - All code (no trust enforcement)\n\n";
    std::cout << "MCP Generation Options:\n";
    std::cout << "  --mcp-name=<name>        MCP server name [default: meld-mcp-server]\n";
    std::cout << "  --mcp-version=<version>  MCP server version [default: 1.0.0]\n";
    std::cout << "  --include-private        Include private functions in MCP\n";
    std::cout << "  --include-tests          Include test functions in MCP\n";
    std::cout << "  --include-ns=<namespace> Include specific namespace (can be repeated)\n";
    std::cout << "  --exclude-fn=<function>  Exclude specific function (can be repeated)\n\n";
    std::cout << "JVM Generation Options:\n";
    std::cout << "  --jvm-package=<package>  Java package name [default: meld.generated]\n";
    std::cout << "  --jvm-version=<version>  Java version (8, 11, 17, 21) [default: 17]\n";
    std::cout << "  --jvm-legacy             Use legacy Java features (no records/modern syntax)\n\n";
    std::cout << "Effects Options:\n";
    std::cout << "  --strict-no-keywords     Reject keyword-based effect syntax (effect, imposes, etc.)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  meld --target=compile src/main.meld\n";
    std::cout << "  meld --target=mcp --output=./mcp-server src/*.meld\n";
    std::cout << "  meld --target=jvm --jvm-package=com.example --output=./java-src src/*.meld\n";
    std::cout << "  meld --trust-level=0.9 --target=compile src/*.meld  # High-confidence code only\n";
    std::cout << "  meld --trust-level=1.0 --target=compile src/*.meld  # Verified code only\n";
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--include-private") {
            options.include_private_functions = true;
        } else if (arg == "--include-tests") {
            options.include_test_functions = true;
        } else if (arg == "--jvm-legacy") {
            options.jvm_use_modern_java = false;
        } else if (arg == "--strict-no-keywords") {
            options.strict_no_keywords = true;
        } else if (arg.starts_with("--jvm-package=")) {
            options.jvm_package_name = arg.substr(14);
        } else if (arg.starts_with("--jvm-version=")) {
            options.jvm_java_version = std::stoi(arg.substr(14));
        } else if (arg.starts_with("--target=")) {
            options.target = arg.substr(9);
        } else if (arg.starts_with("--output=")) {
            options.output_dir = arg.substr(9);
        } else if (arg.starts_with("--mcp-name=")) {
            options.mcp_server_name = arg.substr(11);
        } else if (arg.starts_with("--mcp-version=")) {
            options.mcp_server_version = arg.substr(14);
        } else if (arg.starts_with("--include-ns=")) {
            options.include_namespaces.push_back(arg.substr(13));
        } else if (arg.starts_with("--exclude-fn=")) {
            options.exclude_functions.push_back(arg.substr(13));
        } else if (arg.starts_with("--trust-level=")) {
            try {
                double level = std::stod(arg.substr(14));
                if (level < 0.0 || level > 1.0) {
                    std::cerr << "Error: Trust level must be between 0.0 and 1.0, got: " << level << std::endl;
                    options.help = true;
                } else {
                    options.trust_level = level;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid trust level value: " << arg.substr(14) << std::endl;
                options.help = true;
            }
        } else if (!arg.starts_with("--")) {
            // Input file or directory
            if (std::filesystem::is_directory(arg)) {
                // Add all .meld files in directory
                for (const auto& entry : std::filesystem::recursive_directory_iterator(arg)) {
                    if (entry.path().extension() == ".meld") {
                        options.input_files.push_back(entry.path().string());
                    }
                }
            } else {
                options.input_files.push_back(arg);
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            options.help = true;
        }
    }
    
    return options;
}

bool compile_files(const CLIOptions& cli_options) {
    compiler::Compiler compiler;
    
    // Set up compilation options
    compiler::CompilationOptions options;
    options.output_path = cli_options.output_dir;
    
    // Set trust level enforcement if specified
    if (cli_options.trust_level) {
        options.minimum_trust_level = *cli_options.trust_level;
    }
    
    // Set strict-no-keywords mode
    options.strict_no_keywords = cli_options.strict_no_keywords;
    
    if (cli_options.target == "mcp") {
        options.generate_mcp = true;
        options.mcp_options.server_name = cli_options.mcp_server_name;
        options.mcp_options.server_version = cli_options.mcp_server_version;
        options.mcp_options.output_directory = cli_options.output_dir;
        options.mcp_options.include_private_functions = cli_options.include_private_functions;
        options.mcp_options.include_test_functions = cli_options.include_test_functions;
        options.mcp_options.include_namespaces = cli_options.include_namespaces;
        options.mcp_options.exclude_functions = cli_options.exclude_functions;
    } else if (cli_options.target == "jvm") {
        options.generate_jvm = true;
        options.target = compiler::CodeGenTarget::JVM;
        options.jvm_package_name = cli_options.jvm_package_name;
        options.jvm_output_directory = cli_options.output_dir;
        options.jvm_use_modern_java = cli_options.jvm_use_modern_java;
        options.jvm_java_version = cli_options.jvm_java_version;
    }
    
    // Compile files
    auto result = compiler.compile_files(cli_options.input_files, options);
    
    // Print results
    if (cli_options.verbose || !result.success) {
        std::cout << std::format("Compilation completed in {:.2f}ms\n", result.compilation_time_ms);
        std::cout << std::format("Functions compiled: {}\n", result.total_functions_compiled);
        
        if (result.total_tests_executed > 0) {
            std::cout << std::format("Tests executed: {} (failures: {})\n", 
                                   result.total_tests_executed, result.total_test_failures);
        }
        
        if (result.mcp_generated) {
            std::cout << std::format("MCP tools generated: {} in {}\n", 
                                   result.mcp_tools_generated, result.mcp_output_path);
        }
    }
    
    // Print diagnostics
    for (const auto& diagnostic : result.diagnostics) {
        std::string level_str;
        switch (diagnostic.level) {
            case compiler::DiagnosticLevel::INFO: level_str = "INFO"; break;
            case compiler::DiagnosticLevel::WARNING: level_str = "WARNING"; break;
            case compiler::DiagnosticLevel::ERROR: level_str = "ERROR"; break;
        }
        
        if (!diagnostic.file_path.empty()) {
            std::cout << std::format("{}:{}:{}: {}: {}\n", 
                                   diagnostic.file_path, diagnostic.line, diagnostic.column,
                                   level_str, diagnostic.message);
        } else {
            std::cout << std::format("{}: {}\n", level_str, diagnostic.message);
        }
    }
    
    return result.success;
}

int main(int argc, char* argv[]) {
    auto options = parse_arguments(argc, argv);
    
    if (options.help || options.input_files.empty()) {
        print_help();
        return options.help ? 0 : 1;
    }
    
    // Validate target
    if (options.target != "compile" && options.target != "mcp" && options.target != "jvm") {
        std::cerr << "Error: Invalid target '" << options.target << "'. Must be 'compile', 'mcp', or 'jvm'.\n";
        return 1;
    }
    
    // Create output directory
    try {
        std::filesystem::create_directories(options.output_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error: Could not create output directory: " << e.what() << std::endl;
        return 1;
    }
    
    if (options.verbose) {
        std::cout << std::format("Target: {}\n", options.target);
        std::cout << std::format("Output directory: {}\n", options.output_dir);
        std::cout << std::format("Input files: {}\n", options.input_files.size());
        
        if (options.trust_level) {
            std::cout << std::format("Trust level enforcement: {:.1f}\n", *options.trust_level);
        }
        
        if (options.target == "mcp") {
            std::cout << std::format("MCP server name: {}\n", options.mcp_server_name);
            std::cout << std::format("MCP server version: {}\n", options.mcp_server_version);
            std::cout << std::format("Include private functions: {}\n", options.include_private_functions);
            std::cout << std::format("Include test functions: {}\n", options.include_test_functions);
        } else if (options.target == "jvm") {
            std::cout << std::format("JVM package name: {}\n", options.jvm_package_name);
            std::cout << std::format("Java version: {}\n", options.jvm_java_version);
            std::cout << std::format("Use modern Java: {}\n", options.jvm_use_modern_java);
        }
    }
    
    bool success = compile_files(options);
    
    if (success) {
        if (options.target == "mcp") {
            std::cout << "MCP server generated successfully!\n";
            std::cout << std::format("Run 'cd {} && npm install && npm start' to start the server.\n", 
                                   options.output_dir);
        } else if (options.target == "jvm") {
            std::cout << "Java source files generated successfully!\n";
            std::cout << std::format("Java files are in: {}\n", options.output_dir);
            std::cout << "Compile with: javac -d build *.java\n";
        } else {
            std::cout << "Compilation completed successfully!\n";
        }
    } else {
        std::cerr << "Compilation failed.\n";
    }
    
    return success ? 0 : 1;
}