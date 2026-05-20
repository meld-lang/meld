#include "../include/meld/cli/cap_module.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace meld::cli {

CAPModule::CAPModule()
    : BaseCommandHandler("cap", "Compile with Compiler-Agent Protocol (structured JSON output)")
    , cap_(std::make_unique<compiler::cap::CompilerAgentProtocol>()) {
}

CommandResult CAPModule::execute(const CommandArgs& args) {
    try {
        // Configure CAP from command-line arguments
        configure_cap_from_args(args);
        
        // Check for required source file(s)
        if (args.positional.empty()) {
            std::cerr << "Error: No source file specified\n";
            std::cerr << get_usage() << std::endl;
            return CommandResult::InvalidArguments;
        }
        
        // Determine operation mode
        bool batch_mode = args.flags.contains("batch");
        bool incremental_mode = args.flags.contains("incremental");
        bool json_output = args.flags.contains("json");
        bool pretty_json = args.flags.contains("pretty");
        
        if (batch_mode || args.positional.size() > 1) {
            // Batch compilation
            std::vector<std::filesystem::path> files;
            for (const auto& file : args.positional) {
                files.emplace_back(file);
            }
            return compile_batch_files(files, args);
        } else if (incremental_mode) {
            // Incremental compilation
            std::vector<std::filesystem::path> changed_files;
            for (const auto& file : args.positional) {
                changed_files.emplace_back(file);
            }
            return compile_incremental(changed_files, args);
        } else {
            // Single file compilation
            return compile_single_file(args.positional[0], args);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "CAP compilation failed: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

std::string CAPModule::get_help() const {
    std::ostringstream help;
    help << "Compile Meld source code with structured JSON output for AI agents.\n\n";
    help << get_usage() << "\n\n";
    help << "Options:\n";
    help << "  --json              Output results as JSON\n";
    help << "  --pretty            Pretty-print JSON output\n";
    help << "  --batch             Compile multiple files in batch mode\n";
    help << "  --incremental       Incremental compilation mode\n";
    help << "  --no-suggestions    Disable fix suggestions\n";
    help << "  --max-suggestions=N Maximum suggestions per error (default: 3)\n";
    help << "  --confidence=LEVEL  Minimum confidence level (low, medium, high, very_high)\n";
    help << "  --output=FILE       Write JSON output to file\n\n";
    help << "Examples:\n";
    help << "  meld cap source.meld --json --pretty\n";
    help << "  meld cap file1.meld file2.meld --batch --json\n";
    help << "  meld cap changed.meld --incremental --output=result.json\n";
    return help.str();
}

std::string CAPModule::get_usage() const {
    return "meld cap [options] <source-file> [<source-file> ...]";
}

std::vector<std::string> CAPModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    if (partial.starts_with("--")) {
        std::vector<std::string> options = {
            "--json", "--pretty", "--batch", "--incremental",
            "--no-suggestions", "--max-suggestions=", "--confidence=", "--output="
        };
        for (const auto& option : options) {
            if (option.starts_with(partial)) {
                completions.push_back(option);
            }
        }
    } else if (partial.starts_with("--confidence=")) {
        std::vector<std::string> levels = {
            "--confidence=low", "--confidence=medium", 
            "--confidence=high", "--confidence=very_high"
        };
        for (const auto& level : levels) {
            if (level.starts_with(partial)) {
                completions.push_back(level);
            }
        }
    }
    
    return completions;
}

bool CAPModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.positional.empty()) {
        error_message = "No source file specified";
        return false;
    }
    
    // Validate confidence level if specified
    if (args.options.contains("confidence")) {
        std::string level = args.options.at("confidence");
        if (level != "low" && level != "medium" && level != "high" && level != "very_high") {
            error_message = "Invalid confidence level: " + level;
            return false;
        }
    }
    
    // Validate max-suggestions if specified
    if (args.options.contains("max-suggestions")) {
        try {
            int max = std::stoi(args.options.at("max-suggestions"));
            if (max < 0 || max > 10) {
                error_message = "max-suggestions must be between 0 and 10";
                return false;
            }
        } catch (const std::exception&) {
            error_message = "Invalid max-suggestions value";
            return false;
        }
    }
    
    return true;
}

CommandResult CAPModule::compile_single_file(const std::filesystem::path& file_path,
                                            const CommandArgs& args) {
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Error: File does not exist: " << file_path << std::endl;
        return CommandResult::Error;
    }
    
    // Compile the file
    auto result = cap_->compile_file(file_path);
    
    // Output results
    bool json_output = args.flags.contains("json");
    bool pretty_json = args.flags.contains("pretty");
    
    if (json_output) {
        if (args.options.contains("output")) {
            // Write to file
            std::string json_str = compiler::cap::CompilerAgentProtocol::to_json_string(result, pretty_json);
            std::ofstream output_file(args.options.at("output"));
            if (!output_file) {
                std::cerr << "Error: Cannot write to output file: " << args.options.at("output") << std::endl;
                return CommandResult::Error;
            }
            output_file << json_str;
            std::cout << "JSON output written to: " << args.options.at("output") << std::endl;
        } else {
            // Write to stdout
            output_json(result, pretty_json);
        }
    } else {
        // Human-readable summary
        output_summary(result);
    }
    
    return result.success ? CommandResult::Success : CommandResult::Error;
}

CommandResult CAPModule::compile_batch_files(const std::vector<std::filesystem::path>& files,
                                            const CommandArgs& args) {
    // Compile all files
    auto batch_result = cap_->compile_batch(files);
    
    // Output results
    bool json_output = args.flags.contains("json");
    bool pretty_json = args.flags.contains("pretty");
    
    if (json_output) {
        if (args.options.contains("output")) {
            // Write to file
            std::string json_str = compiler::cap::CompilerAgentProtocol::to_json_string(batch_result, pretty_json);
            std::ofstream output_file(args.options.at("output"));
            if (!output_file) {
                std::cerr << "Error: Cannot write to output file: " << args.options.at("output") << std::endl;
                return CommandResult::Error;
            }
            output_file << json_str;
            std::cout << "JSON output written to: " << args.options.at("output") << std::endl;
        } else {
            // Write to stdout
            output_json(batch_result, pretty_json);
        }
    } else {
        // Human-readable summary
        output_summary(batch_result);
    }
    
    return batch_result.overall_success ? CommandResult::Success : CommandResult::Error;
}

CommandResult CAPModule::compile_incremental(const std::vector<std::filesystem::path>& changed_files,
                                            const CommandArgs& args) {
    // Create incremental changes
    std::vector<compiler::cap::IncrementalChange> changes;
    for (const auto& file : changed_files) {
        changes.emplace_back(
            compiler::cap::IncrementalChange::ChangeType::FILE_MODIFIED,
            file.string()
        );
    }
    
    // Compile incrementally
    auto batch_result = cap_->compile_incremental(changes);
    
    // Output results
    bool json_output = args.flags.contains("json");
    bool pretty_json = args.flags.contains("pretty");
    
    if (json_output) {
        if (args.options.contains("output")) {
            // Write to file
            std::string json_str = compiler::cap::CompilerAgentProtocol::to_json_string(batch_result, pretty_json);
            std::ofstream output_file(args.options.at("output"));
            if (!output_file) {
                std::cerr << "Error: Cannot write to output file: " << args.options.at("output") << std::endl;
                return CommandResult::Error;
            }
            output_file << json_str;
            std::cout << "JSON output written to: " << args.options.at("output") << std::endl;
        } else {
            // Write to stdout
            output_json(batch_result, pretty_json);
        }
    } else {
        // Human-readable summary
        output_summary(batch_result);
    }
    
    return batch_result.overall_success ? CommandResult::Success : CommandResult::Error;
}

void CAPModule::output_json(const compiler::cap::CompilationResult& result, bool pretty) const {
    std::string json_str = compiler::cap::CompilerAgentProtocol::to_json_string(result, pretty);
    std::cout << json_str << std::endl;
}

void CAPModule::output_json(const compiler::cap::BatchCompilationResult& result, bool pretty) const {
    std::string json_str = compiler::cap::CompilerAgentProtocol::to_json_string(result, pretty);
    std::cout << json_str << std::endl;
}

void CAPModule::output_summary(const compiler::cap::CompilationResult& result) const {
    std::cout << "=== Compilation Result ===" << std::endl;
    std::cout << "File: " << result.file_path << std::endl;
    std::cout << "Status: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Lines of code: " << result.lines_of_code << std::endl;
    std::cout << "AST nodes: " << result.ast_node_count << std::endl;
    std::cout << "Compilation time: " << result.compilation_time.count() << "ms" << std::endl;
    
    if (result.has_errors()) {
        std::cout << "\nErrors (" << result.get_errors().size() << "):" << std::endl;
        for (const auto& error : result.get_errors()) {
            std::cout << "  [" << error.code << "] " << error.message << std::endl;
            std::cout << "    at " << error.location.file << ":" 
                     << error.location.line << ":" << error.location.column << std::endl;
            
            if (!error.suggestions.empty()) {
                std::cout << "    Suggestions:" << std::endl;
                for (const auto& suggestion : error.suggestions) {
                    std::cout << "      - " << suggestion.description 
                             << " (confidence: " << static_cast<int>(suggestion.confidence) << ")" << std::endl;
                }
            }
        }
    }
    
    if (result.has_warnings()) {
        std::cout << "\nWarnings (" << result.get_warnings().size() << "):" << std::endl;
        for (const auto& warning : result.get_warnings()) {
            std::cout << "  [" << warning.code << "] " << warning.message << std::endl;
        }
    }
}

void CAPModule::output_summary(const compiler::cap::BatchCompilationResult& result) const {
    std::cout << "=== Batch Compilation Result ===" << std::endl;
    std::cout << "Total files: " << result.total_files << std::endl;
    std::cout << "Successful: " << result.successful_files << std::endl;
    std::cout << "Failed: " << (result.total_files - result.successful_files) << std::endl;
    std::cout << "Total errors: " << result.total_errors << std::endl;
    std::cout << "Total warnings: " << result.total_warnings << std::endl;
    std::cout << "Total time: " << result.total_time.count() << "ms" << std::endl;
    std::cout << "Overall status: " << (result.overall_success ? "SUCCESS" : "FAILED") << std::endl;
    
    std::cout << "\nPer-file results:" << std::endl;
    for (const auto& file_result : result.results) {
        std::cout << "  " << file_result.file_path << ": " 
                 << (file_result.success ? "SUCCESS" : "FAILED");
        if (file_result.has_errors()) {
            std::cout << " (" << file_result.get_errors().size() << " errors)";
        }
        std::cout << std::endl;
    }
}

void CAPModule::configure_cap_from_args(const CommandArgs& args) {
    // Configure suggestions
    if (args.flags.contains("no-suggestions")) {
        cap_->set_include_suggestions(false);
    } else {
        cap_->set_include_suggestions(true);
    }
    
    // Configure max suggestions
    if (args.options.contains("max-suggestions")) {
        try {
            int max = std::stoi(args.options.at("max-suggestions"));
            cap_->set_max_suggestions_per_error(max);
        } catch (const std::exception&) {
            // Use default
        }
    }
    
    // Configure confidence threshold
    if (args.options.contains("confidence")) {
        std::string level = args.options.at("confidence");
        compiler::cap::ConfidenceLevel confidence;
        
        if (level == "low") {
            confidence = compiler::cap::ConfidenceLevel::LOW;
        } else if (level == "medium") {
            confidence = compiler::cap::ConfidenceLevel::MEDIUM;
        } else if (level == "high") {
            confidence = compiler::cap::ConfidenceLevel::HIGH;
        } else if (level == "very_high") {
            confidence = compiler::cap::ConfidenceLevel::VERY_HIGH;
        } else {
            confidence = compiler::cap::ConfidenceLevel::MEDIUM; // Default
        }
        
        cap_->set_confidence_threshold(confidence);
    }
}

} // namespace meld::cli