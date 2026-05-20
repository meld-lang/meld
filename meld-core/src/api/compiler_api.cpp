#include "meld/api/compiler_api.hpp"
#include "meld/compiler/compiler.hpp"
#include <memory>

namespace meld::api {

// Static instance for maintaining state between calls
static std::unique_ptr<compiler::Compiler> g_compiler = nullptr;
static std::vector<std::string> g_last_diagnostics;

static compiler::Compiler& get_compiler() {
    if (!g_compiler) {
        g_compiler = std::make_unique<compiler::Compiler>();
    }
    return *g_compiler;
}

bool CompilerAPI::compile(const std::string& source_code, const std::string& output_path) {
    compiler::CompilationOptions options;
    options.output_path = output_path;
    options.execute_micro_tests = true;  // Always execute micro-tests during compilation
    
    auto result = get_compiler().compile_source(source_code, "<api>", options);
    
    // Convert diagnostics to string format
    g_last_diagnostics.clear();
    for (const auto& diag : result.diagnostics) {
        std::string level_str;
        switch (diag.level) {
            case compiler::DiagnosticLevel::INFO: level_str = "INFO"; break;
            case compiler::DiagnosticLevel::WARNING: level_str = "WARNING"; break;
            case compiler::DiagnosticLevel::ERROR: level_str = "ERROR"; break;
        }
        
        std::string diagnostic_msg = level_str + ": " + diag.message;
        if (!diag.file_path.empty()) {
            diagnostic_msg += " (in " + diag.file_path;
            if (diag.line > 0) {
                diagnostic_msg += ":" + std::to_string(diag.line);
                if (diag.column > 0) {
                    diagnostic_msg += ":" + std::to_string(diag.column);
                }
            }
            diagnostic_msg += ")";
        }
        
        g_last_diagnostics.push_back(diagnostic_msg);
    }
    
    return result.success;
}

bool CompilerAPI::type_check(const std::string& source_code) {
    compiler::CompilationOptions options;
    options.execute_micro_tests = false;  // Only type check, don't run tests
    
    auto result = get_compiler().compile_source(source_code, "<type_check>", options);
    
    // Convert diagnostics to string format (same as compile)
    g_last_diagnostics.clear();
    for (const auto& diag : result.diagnostics) {
        std::string level_str;
        switch (diag.level) {
            case compiler::DiagnosticLevel::INFO: level_str = "INFO"; break;
            case compiler::DiagnosticLevel::WARNING: level_str = "WARNING"; break;
            case compiler::DiagnosticLevel::ERROR: level_str = "ERROR"; break;
        }
        
        g_last_diagnostics.push_back(level_str + ": " + diag.message);
    }
    
    return result.success;
}

std::vector<std::string> CompilerAPI::get_diagnostics() {
    return g_last_diagnostics;
}

} // namespace meld::api