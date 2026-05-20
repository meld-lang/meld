#include "meld/cli/check_module.hpp"
#include "meld/compiler/compiler.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace meld::cli {

using namespace meld::compiler;

CheckModule::CheckModule()
    : BaseCommandHandler("check", "Check a file for errors without executing") {}

CommandResult CheckModule::execute(const CommandArgs& args) {
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    bool warnings_ok = args.flags.count("warnings-ok") > 0 || args.options.count("warnings-ok") > 0;

    std::string file_path;
    for (const auto& arg : args.positional) {
        if (arg == "--text") { text = true; continue; }
        if (arg == "--warnings-ok") { warnings_ok = true; continue; }
        if (arg.substr(0, 2) != "--" && file_path.empty()) file_path = arg;
    }

    if (file_path.empty()) {
        std::cerr << "Usage: meld check <file.meld> [--text] [--warnings-ok]\n";
        return CommandResult::InvalidArguments;
    }

    if (!std::filesystem::exists(file_path)) {
        if (text) {
            std::cerr << "File not found: " << file_path << "\n";
        } else {
            std::cout << "{\"ok\":false,\"file\":\"" << file_path << "\",\"diagnostics\":[{\"code\":\"E001\",\"message\":\"File not found\"}]}\n";
        }
        return CommandResult::Error;
    }

    // Read source
    std::ifstream f(file_path);
    std::string source(std::istreambuf_iterator<char>(f), {});

    // Compile (parse + type check + ownership/borrow) without executing
    Compiler compiler;
    CompilationOptions opts;
    opts.enable_ownership_checking = true;
    opts.enable_borrow_checking = true;
    opts.enable_property_tests = false;
    opts.enable_provenance_tracking = false;
    opts.enable_flow_validation = false;
    opts.enable_ide_integration = false;
    opts.persist_annotations_on_save = false;
    opts.update_annotations_on_change = false;
    opts.enable_provenance_mismatch_detection = false;
    auto result = compiler.compile_source(source, file_path, opts);

    // Check for safety violations
    bool has_violations = result.ownership_violations_found > 0 || result.borrow_violations_found > 0;
    
    // Check for error-level diagnostics (includes @uses violations)
    bool has_errors = false;
    for (const auto& d : result.diagnostics) {
        if (d.level == DiagnosticLevel::ERROR) { has_errors = true; break; }
    }
    
    bool ok = result.success && !has_errors && (!has_violations || warnings_ok);

    if (text) {
        // Human-readable output
        if (ok) {
            std::cout << file_path << ": OK";
            if (result.total_functions_compiled > 0)
                std::cout << " (" << result.total_functions_compiled << " functions)";
            std::cout << "\n";
        } else {
            std::cout << file_path << ": FAILED\n";
            for (const auto& d : result.diagnostics) {
                const char* level = "info";
                if (d.level == DiagnosticLevel::ERROR) level = "error";
                if (d.level == DiagnosticLevel::WARNING) level = "warning";
                std::cout << "  " << level << ": " << d.message << "\n";
            }
        }
    } else {
        // JSON output (default)
        std::cout << "{";
        std::cout << "\"ok\":" << (ok ? "true" : "false") << ",";
        std::cout << "\"file\":\"" << file_path << "\",";
        std::cout << "\"functions\":" << result.total_functions_compiled << ",";
        std::cout << "\"diagnostics\":[";
        bool first = true;
        for (const auto& d : result.diagnostics) {
            if (d.level != DiagnosticLevel::ERROR && d.level != DiagnosticLevel::WARNING) continue;
            if (!first) std::cout << ",";
            first = false;
            std::cout << "{\"level\":\"" << (d.level == DiagnosticLevel::ERROR ? "error" : "warning") << "\",";
            std::cout << "\"message\":\"" << d.message << "\",";
            std::cout << "\"line\":" << d.line << "}";
        }
        std::cout << "]}\n";
    }

    return ok ? CommandResult::Success : CommandResult::Error;
}

std::string CheckModule::get_help() const {
    return R"(Usage: meld check <file.meld> [--text] [--warnings-ok]

Parse and type-check a file without executing it.
Output is JSON by default. Use --text for human-readable.

Flags:
  --text          Human-readable output
  --warnings-ok   Treat borrow/ownership violations as warnings

Examples:
  meld check src/main.meld              JSON diagnostics (default)
  meld check src/main.meld --text       Human-readable
  meld check . --warnings-ok            Check with relaxed safety)";
}

std::string CheckModule::get_usage() const {
    return "meld check <file.meld> [--text] [--warnings-ok]";
}

std::vector<std::string> CheckModule::get_completions(const std::string& partial) const {
    return {"--text", "--warnings-ok"};
}

bool CheckModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    return true;
}

} // namespace meld::cli
