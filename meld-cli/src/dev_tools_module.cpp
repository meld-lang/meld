#include "meld/cli/dev_tools_module.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>

namespace meld::cli {

namespace {
// Helper: collect .meld files from a path (used by CodeFormatter and CodeLinter)
std::vector<std::filesystem::path> collect_meld_files(const std::filesystem::path& path, bool recursive) {
    std::vector<std::filesystem::path> files;
    auto is_meld = [](const std::filesystem::path& p) { return p.extension() == ".meld"; };
    if (std::filesystem::is_regular_file(path) && is_meld(path)) {
        files.push_back(path);
    } else if (std::filesystem::is_directory(path)) {
        try {
            if (recursive) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file() && is_meld(entry.path())) files.push_back(entry.path());
                }
            } else {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    if (entry.is_regular_file() && is_meld(entry.path())) files.push_back(entry.path());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "Error accessing directory: " << e.what() << std::endl;
        }
    }
    return files;
}
} // anonymous namespace

// LintIssue implementation
std::string LintIssue::format() const {
    std::ostringstream oss;
    oss << file << ":" << line << ":" << column << ": " 
        << severity_string() << ": " << message;
    if (!rule_id.empty()) {
        oss << " [" << rule_id << "]";
    }
    if (!suggestions.empty()) {
        oss << "\n  Suggestions:";
        for (const auto& suggestion : suggestions) {
            oss << "\n    - " << suggestion;
        }
    }
    return oss.str();
}

std::string LintIssue::severity_string() const {
    switch (severity) {
        case LintSeverity::Error: return "error";
        case LintSeverity::Warning: return "warning";
        case LintSeverity::Info: return "info";
        case LintSeverity::Hint: return "hint";
        default: return "unknown";
    }
}

// LintResult implementation
bool LintResult::has_errors() const {
    return std::any_of(issues.begin(), issues.end(),
        [](const LintIssue& issue) { return issue.severity == LintSeverity::Error; });
}

bool LintResult::has_warnings() const {
    return std::any_of(issues.begin(), issues.end(),
        [](const LintIssue& issue) { return issue.severity == LintSeverity::Warning; });
}

size_t LintResult::error_count() const {
    return std::count_if(issues.begin(), issues.end(),
        [](const LintIssue& issue) { return issue.severity == LintSeverity::Error; });
}

size_t LintResult::warning_count() const {
    return std::count_if(issues.begin(), issues.end(),
        [](const LintIssue& issue) { return issue.severity == LintSeverity::Warning; });
}

// CodeFormatter implementation
FormatResult CodeFormatter::format_code(const std::string& source_code, const FormatOptions& options) {
    FormatResult result;
    
    try {
        std::string formatted = source_code;
        
        // Apply formatting transformations
        formatted = normalize_whitespace(formatted, options);
        formatted = apply_indentation(formatted, options);
        formatted = apply_line_length_limits(formatted, options);
        
        result.formatted_code = formatted;
        result.needs_formatting = (formatted != source_code);
        result.success = true;
        result.files_processed = 1;
        if (result.needs_formatting && !options.check_only) {
            result.files_changed = 1;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("Formatting error: ") + e.what());
    }
    
    return result;
}
FormatResult CodeFormatter::format_file(const std::filesystem::path& file_path, const FormatOptions& options) {
    FormatResult result;
    
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            result.success = false;
            result.errors.push_back("Cannot open file: " + file_path.string());
            return result;
        }
        
        std::string source_code((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        file.close();
        
        result = format_code(source_code, options);
        
        // Write back to file if not in check mode and formatting is needed
        if (result.success && !options.check_only && result.needs_formatting) {
            std::ofstream out_file(file_path);
            if (out_file.is_open()) {
                out_file << result.formatted_code;
                out_file.close();
            } else {
                result.success = false;
                result.errors.push_back("Cannot write to file: " + file_path.string());
            }
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("File processing error: ") + e.what());
    }
    
    return result;
}

FormatResult CodeFormatter::format_directory(const std::filesystem::path& dir_path, const FormatOptions& options) {
    FormatResult combined_result;
    combined_result.success = true;
    
    auto files = collect_meld_files(dir_path, options.recursive);
    
    for (const auto& file : files) {
        auto file_result = format_file(file, options);
        
        combined_result.files_processed += file_result.files_processed;
        combined_result.files_changed += file_result.files_changed;
        
        if (!file_result.success) {
            combined_result.success = false;
            combined_result.errors.insert(combined_result.errors.end(),
                                        file_result.errors.begin(),
                                        file_result.errors.end());
        }
    }
    
    return combined_result;
}

bool CodeFormatter::needs_formatting(const std::string& source_code, const FormatOptions& options) {
    auto result = format_code(source_code, options);
    return result.needs_formatting;
}

std::string CodeFormatter::apply_indentation(const std::string& code, const FormatOptions& options) {
    std::istringstream iss(code);
    std::ostringstream oss;
    std::string line;
    int indent_level = 0;
    
    while (std::getline(iss, line)) {
        // Remove existing indentation
        line = std::regex_replace(line, std::regex("^\\s+"), "");
        
        // Skip empty lines
        if (line.empty()) {
            oss << "\n";
            continue;
        }
        
        // Adjust indent level based on braces
        if (line.find('}') != std::string::npos) {
            indent_level = std::max(0, indent_level - 1);
        }
        
        // Apply indentation
        std::string indent_str;
        if (options.use_tabs) {
            indent_str = std::string(indent_level, '\t');
        } else {
            indent_str = std::string(indent_level * options.indent_size, ' ');
        }
        
        oss << indent_str << line << "\n";
        
        // Increase indent level for opening braces
        if (line.find('{') != std::string::npos) {
            indent_level++;
        }
    }
    
    return oss.str();
}
std::string CodeFormatter::apply_line_length_limits(const std::string& code, const FormatOptions& options) {
    std::istringstream iss(code);
    std::ostringstream oss;
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.length() <= static_cast<size_t>(options.max_line_length)) {
            oss << line << "\n";
        } else {
            // Simple line breaking - could be more sophisticated
            size_t pos = 0;
            while (pos < line.length()) {
                size_t end_pos = std::min(pos + options.max_line_length, line.length());
                
                // Try to break at a space or operator
                if (end_pos < line.length()) {
                    size_t break_pos = line.find_last_of(" \t+-*/=<>", end_pos);
                    if (break_pos != std::string::npos && break_pos > pos) {
                        end_pos = break_pos + 1;
                    }
                }
                
                oss << line.substr(pos, end_pos - pos);
                if (end_pos < line.length()) {
                    oss << "\n";
                }
                pos = end_pos;
            }
            oss << "\n";
        }
    }
    
    return oss.str();
}

std::string CodeFormatter::normalize_whitespace(const std::string& code, const FormatOptions& options) {
    std::string result = code;
    
    // Remove trailing whitespace
    result = std::regex_replace(result, std::regex("[ \t]+$"), "", std::regex_constants::format_default);
    
    // Normalize spaces around operators
    result = std::regex_replace(result, std::regex("\\s*([+\\-*/=<>!])\\s*"), " $1 ");
    
    // Normalize spaces after commas
    result = std::regex_replace(result, std::regex(",\\s*"), ", ");
    
    // Remove multiple consecutive blank lines if not preserving newlines
    if (!options.preserve_newlines) {
        result = std::regex_replace(result, std::regex("\n\n\n+"), "\n\n");
    }
    
    return result;
}

// CodeLinter implementation
LintResult CodeLinter::lint_code(const std::string& source_code, const std::string& file_path, 
                                const LintOptions& options) {
    LintResult result;
    result.success = true;
    result.files_processed = 1;
    
    try {
        // Check different types of issues
        auto syntax_issues = check_syntax_issues(source_code, file_path);
        auto style_issues = check_style_issues(source_code, file_path);
        auto semantic_issues = check_semantic_issues(source_code, file_path);
        
        // Combine all issues
        result.issues.insert(result.issues.end(), syntax_issues.begin(), syntax_issues.end());
        result.issues.insert(result.issues.end(), style_issues.begin(), style_issues.end());
        result.issues.insert(result.issues.end(), semantic_issues.begin(), semantic_issues.end());
        
        // Filter by severity and enabled rules
        result.issues.erase(
            std::remove_if(result.issues.begin(), result.issues.end(),
                [&](const LintIssue& issue) {
                    return issue.severity < options.min_severity ||
                           !is_rule_enabled(issue.rule_id, options);
                }),
            result.issues.end());
        
        // Apply automatic fixes if requested
        if (options.auto_fix) {
            // Count fixable issues
            result.fixes_applied = std::count_if(result.issues.begin(), result.issues.end(),
                [](const LintIssue& issue) { return issue.auto_fixable; });
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        LintIssue error("Linting error: " + std::string(e.what()), file_path, 0, 0, LintSeverity::Error);
        result.issues.push_back(error);
    }
    
    return result;
}
LintResult CodeLinter::lint_file(const std::filesystem::path& file_path, const LintOptions& options) {
    LintResult result;
    
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            result.success = false;
            LintIssue error("Cannot open file: " + file_path.string(), file_path.string(), 0, 0, LintSeverity::Error);
            result.issues.push_back(error);
            return result;
        }
        
        std::string source_code((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        file.close();
        
        result = lint_code(source_code, file_path.string(), options);
        
        // Apply fixes to file if requested
        if (result.success && options.auto_fix && result.fixes_applied > 0) {
            std::string fixed_code = apply_fixes(source_code, result.issues);
            std::ofstream out_file(file_path);
            if (out_file.is_open()) {
                out_file << fixed_code;
                out_file.close();
            }
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        LintIssue error("File processing error: " + std::string(e.what()), file_path.string(), 0, 0, LintSeverity::Error);
        result.issues.push_back(error);
    }
    
    return result;
}

LintResult CodeLinter::lint_directory(const std::filesystem::path& dir_path, const LintOptions& options) {
    LintResult combined_result;
    combined_result.success = true;
    
    auto files = collect_meld_files(dir_path, options.recursive);
    
    for (const auto& file : files) {
        auto file_result = lint_file(file, options);
        
        combined_result.files_processed += file_result.files_processed;
        combined_result.fixes_applied += file_result.fixes_applied;
        combined_result.issues.insert(combined_result.issues.end(),
                                    file_result.issues.begin(),
                                    file_result.issues.end());
        
        if (!file_result.success) {
            combined_result.success = false;
        }
    }
    
    return combined_result;
}

std::string CodeLinter::apply_fixes(const std::string& source_code, const std::vector<LintIssue>& issues) {
    std::string fixed_code = source_code;
    
    // Apply fixes in reverse order to maintain line/column positions
    auto fixable_issues = issues;
    std::sort(fixable_issues.begin(), fixable_issues.end(),
        [](const LintIssue& a, const LintIssue& b) {
            if (a.line != b.line) return a.line > b.line;
            return a.column > b.column;
        });
    
    for (const auto& issue : fixable_issues) {
        if (issue.auto_fixable && !issue.suggestions.empty()) {
            // Simple fix application - in a real implementation this would be more sophisticated
            // For now, just apply basic fixes like removing trailing whitespace
            if (issue.rule_id == "trailing-whitespace") {
                fixed_code = std::regex_replace(fixed_code, std::regex("[ \t]+$"), "");
            }
        }
    }
    
    return fixed_code;
}

std::vector<LintIssue> CodeLinter::check_syntax_issues(const std::string& code, const std::string& file_path) {
    std::vector<LintIssue> issues;
    
    // Basic syntax checks - in a real implementation this would use a proper parser
    std::istringstream iss(code);
    std::string line;
    size_t line_num = 1;
    
    while (std::getline(iss, line)) {
        // Check for unmatched braces (simplified)
        size_t open_braces = std::count(line.begin(), line.end(), '{');
        size_t close_braces = std::count(line.begin(), line.end(), '}');
        
        if (open_braces != close_braces) {
            LintIssue issue("Unmatched braces", file_path, line_num, 0, LintSeverity::Error);
            issue.rule_id = "unmatched-braces";
            issue.suggestions.push_back("Check brace matching");
            issues.push_back(issue);
        }
        
        line_num++;
    }
    
    return issues;
}
std::vector<LintIssue> CodeLinter::check_style_issues(const std::string& code, const std::string& file_path) {
    std::vector<LintIssue> issues;
    
    std::istringstream iss(code);
    std::string line;
    size_t line_num = 1;
    
    while (std::getline(iss, line)) {
        // Check for trailing whitespace
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            LintIssue issue("Trailing whitespace", file_path, line_num, line.length(), LintSeverity::Warning);
            issue.rule_id = "trailing-whitespace";
            issue.auto_fixable = true;
            issue.suggestions.push_back("Remove trailing whitespace");
            issues.push_back(issue);
        }
        
        // Check line length
        if (line.length() > 100) {
            LintIssue issue("Line too long", file_path, line_num, 100, LintSeverity::Info);
            issue.rule_id = "line-length";
            issue.suggestions.push_back("Consider breaking long lines");
            issues.push_back(issue);
        }
        
        line_num++;
    }
    
    return issues;
}

std::vector<LintIssue> CodeLinter::check_semantic_issues(const std::string& code, const std::string& file_path) {
    std::vector<LintIssue> issues;
    
    // Basic semantic checks - in a real implementation this would use semantic analysis
    std::istringstream iss(code);
    std::string line;
    size_t line_num = 1;
    
    while (std::getline(iss, line)) {
        // Check for unused variables (simplified)
        if (line.find("val ") != std::string::npos || line.find("var ") != std::string::npos) {
            // Extract variable name
            std::regex var_regex("(val|var)\\s+(\\w+)");
            std::smatch match;
            if (std::regex_search(line, match, var_regex)) {
                std::string var_name = match[2].str();
                
                // Check if variable is used later (very simplified)
                size_t usage_count = 0;
                std::istringstream check_iss(code);
                std::string check_line;
                while (std::getline(check_iss, check_line)) {
                    if (check_line.find(var_name) != std::string::npos) {
                        usage_count++;
                    }
                }
                
                if (usage_count <= 1) {
                    LintIssue issue("Variable '" + var_name + "' may be unused", file_path, line_num, 0, LintSeverity::Hint);
                    issue.rule_id = "unused-variable";
                    issue.suggestions.push_back("Remove unused variable or use it");
                    issues.push_back(issue);
                }
            }
        }
        
        line_num++;
    }
    
    return issues;
}

bool CodeLinter::is_rule_enabled(const std::string& rule_id, const LintOptions& options) {
    // Check if rule is explicitly disabled
    if (std::find(options.disabled_rules.begin(), options.disabled_rules.end(), rule_id) 
        != options.disabled_rules.end()) {
        return false;
    }
    
    // If enabled_rules is empty, all rules are enabled by default
    if (options.enabled_rules.empty()) {
        return true;
    }
    
    // Check if rule is explicitly enabled
    return std::find(options.enabled_rules.begin(), options.enabled_rules.end(), rule_id) 
           != options.enabled_rules.end();
}

// Debugger implementation
bool Debugger::start_debug_session(const std::filesystem::path& program_path, 
                                  const std::vector<std::string>& args,
                                  const DevToolsDebugOptions& options) {
    if (state_ != DebugState::NotStarted && state_ != DebugState::Stopped) {
        return false;
    }
    
    breakpoints_ = options.breakpoints;
    state_ = DebugState::Running;
    
    // In a real implementation, this would start the debugger
    // For now, just simulate starting
    std::cout << "Debug session started for: " << program_path << std::endl;
    
    return true;
}
void Debugger::stop_debug_session() {
    state_ = DebugState::Stopped;
    breakpoints_.clear();
    std::cout << "Debug session stopped" << std::endl;
}

bool Debugger::add_breakpoint(const Breakpoint& breakpoint) {
    breakpoints_.push_back(breakpoint);
    std::cout << "Breakpoint added at " << breakpoint.file << ":" << breakpoint.line << std::endl;
    return true;
}

bool Debugger::remove_breakpoint(const std::filesystem::path& file, size_t line) {
    auto it = std::remove_if(breakpoints_.begin(), breakpoints_.end(),
        [&](const Breakpoint& bp) { return bp.file == file && bp.line == line; });
    
    if (it != breakpoints_.end()) {
        breakpoints_.erase(it, breakpoints_.end());
        std::cout << "Breakpoint removed from " << file << ":" << line << std::endl;
        return true;
    }
    
    return false;
}

bool Debugger::step_next() {
    if (state_ != DebugState::Paused && state_ != DebugState::Running) {
        return false;
    }
    
    state_ = DebugState::Paused;
    current_line_++;
    std::cout << "Stepped to line " << current_line_ << std::endl;
    return true;
}

bool Debugger::step_into() {
    if (state_ != DebugState::Paused && state_ != DebugState::Running) {
        return false;
    }
    
    state_ = DebugState::Paused;
    std::cout << "Stepped into function" << std::endl;
    return true;
}

bool Debugger::step_out() {
    if (state_ != DebugState::Paused) {
        return false;
    }
    
    std::cout << "Stepped out of function" << std::endl;
    return true;
}

bool Debugger::continue_execution() {
    if (state_ != DebugState::Paused) {
        return false;
    }
    
    state_ = DebugState::Running;
    std::cout << "Continuing execution" << std::endl;
    return true;
}

std::map<std::string, std::string> Debugger::get_variables() {
    std::map<std::string, std::string> variables;
    
    // In a real implementation, this would query the runtime
    variables["x"] = "42";
    variables["name"] = "\"example\"";
    variables["active"] = "true";
    
    return variables;
}

std::string Debugger::evaluate_expression(const std::string& expression) {
    // In a real implementation, this would evaluate the expression in the current context
    return "Result of: " + expression;
}

// Profiler implementation
ProfileResult Profiler::profile_program(const std::filesystem::path& program_path,
                                       const std::vector<std::string>& args,
                                       const ProfileOptions& options) {
    ProfileResult result;
    
    try {
        result.metrics = collect_metrics(program_path, args, options);
        result.report = generate_report(result.metrics);
        result.hotspots = analyze_hotspots(result.metrics);
        result.recommendations = generate_recommendations(result.metrics);
        result.success = true;
        
        // Write profile data to file if specified
        if (!options.output_file.empty()) {
            write_profile_data(result.metrics, options.output_file);
            result.report_file = options.output_file;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.report = "Profiling error: " + std::string(e.what());
    }
    
    return result;
}
std::string Profiler::generate_report(const DevToolsPerformanceMetrics& metrics) {
    std::ostringstream oss;
    
    oss << "Performance Profile Report\n";
    oss << "==========================\n\n";
    
    oss << "Execution Time: " << metrics.execution_time.count() / 1000000.0 << " ms\n";
    oss << "Peak Memory Usage: " << metrics.peak_memory_usage / 1024 << " KB\n";
    oss << "Total Allocations: " << metrics.total_allocations << "\n";
    oss << "Function Calls: " << metrics.function_calls << "\n\n";
    
    if (!metrics.function_times.empty()) {
        oss << "Function Timing:\n";
        for (const auto& [func_name, time] : metrics.function_times) {
            oss << "  " << func_name << ": " << time.count() / 1000000.0 << " ms\n";
        }
        oss << "\n";
    }
    
    if (!metrics.allocation_sites.empty()) {
        oss << "Allocation Hotspots:\n";
        for (const auto& [site, count] : metrics.allocation_sites) {
            oss << "  " << site << ": " << count << " allocations\n";
        }
    }
    
    return oss.str();
}

std::vector<std::string> Profiler::analyze_hotspots(const DevToolsPerformanceMetrics& metrics) {
    std::vector<std::string> hotspots;
    
    // Find functions that take the most time
    std::vector<std::pair<std::string, std::chrono::nanoseconds>> sorted_functions;
    for (const auto& [name, time] : metrics.function_times) {
        sorted_functions.emplace_back(name, time);
    }
    
    std::sort(sorted_functions.begin(), sorted_functions.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Report top 3 time-consuming functions
    for (size_t i = 0; i < std::min(size_t(3), sorted_functions.size()); ++i) {
        hotspots.push_back("Function '" + sorted_functions[i].first + "' consumes " +
                          std::to_string(sorted_functions[i].second.count() / 1000000.0) + " ms");
    }
    
    // Find allocation hotspots
    std::vector<std::pair<std::string, size_t>> sorted_allocations;
    for (const auto& [site, count] : metrics.allocation_sites) {
        sorted_allocations.emplace_back(site, count);
    }
    
    std::sort(sorted_allocations.begin(), sorted_allocations.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Report top allocation sites
    for (size_t i = 0; i < std::min(size_t(2), sorted_allocations.size()); ++i) {
        hotspots.push_back("Allocation site '" + sorted_allocations[i].first + "' has " +
                          std::to_string(sorted_allocations[i].second) + " allocations");
    }
    
    return hotspots;
}

std::vector<std::string> Profiler::generate_recommendations(const DevToolsPerformanceMetrics& metrics) {
    std::vector<std::string> recommendations;
    
    // Memory usage recommendations
    if (metrics.peak_memory_usage > 100 * 1024 * 1024) { // > 100MB
        recommendations.push_back("Consider optimizing memory usage - peak usage is high");
    }
    
    if (metrics.total_allocations > 10000) {
        recommendations.push_back("High allocation count - consider object pooling or reuse");
    }
    
    // Function call recommendations
    if (metrics.function_calls > 100000) {
        recommendations.push_back("High function call count - consider inlining hot functions");
    }
    
    // Execution time recommendations
    if (metrics.execution_time > std::chrono::seconds(1)) {
        recommendations.push_back("Long execution time - profile individual functions for optimization");
    }
    
    return recommendations;
}

DevToolsPerformanceMetrics Profiler::collect_metrics(const std::filesystem::path& program_path,
                                            const std::vector<std::string>& args,
                                            const ProfileOptions& options) {
    DevToolsPerformanceMetrics metrics;
    
    // Simulate program execution and metric collection
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    metrics.execution_time = end_time - start_time;
    
    // Simulate collected metrics
    metrics.peak_memory_usage = 1024 * 1024; // 1MB
    metrics.total_allocations = 500;
    metrics.function_calls = 1000;
    
    // Simulate function timing data
    metrics.function_times["main"] = std::chrono::milliseconds(50);
    metrics.function_times["process_data"] = std::chrono::milliseconds(30);
    metrics.function_times["calculate"] = std::chrono::milliseconds(20);
    
    // Simulate allocation sites
    metrics.allocation_sites["main:42"] = 100;
    metrics.allocation_sites["process_data:15"] = 200;
    metrics.allocation_sites["calculate:8"] = 200;
    
    return metrics;
}
void Profiler::write_profile_data(const DevToolsPerformanceMetrics& metrics, 
                                  const std::filesystem::path& output_file) {
    std::ofstream file(output_file);
    if (file.is_open()) {
        file << generate_report(metrics);
        file.close();
    }
}

// DevToolsModule implementation
DevToolsModule::DevToolsModule() 
    : BaseCommandHandler("dev", "Development tools for formatting, linting, debugging, and profiling"),
      formatter_(std::make_unique<CodeFormatter>()),
      linter_(std::make_unique<CodeLinter>()),
      debugger_(std::make_unique<Debugger>()),
      profiler_(std::make_unique<Profiler>()) {
}

CommandResult DevToolsModule::execute(const CommandArgs& args) {
    // Support "meld fmt" as a top-level alias for "meld dev format"
    if (args.command == "fmt") {
        return handle_format_command(args);
    }

    if (args.subcommand.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    if (args.subcommand == "format") {
        return handle_format_command(args);
    } else if (args.subcommand == "lint") {
        return handle_lint_command(args);
    } else if (args.subcommand == "debug") {
        return handle_debug_command(args);
    } else if (args.subcommand == "profile") {
        return handle_profile_command(args);
    } else {
        std::cout << "Unknown subcommand: " << args.subcommand << std::endl;
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }
}

std::string DevToolsModule::get_help() const {
    return R"(Development Tools Module

USAGE:
    meld dev <subcommand> [options] [files...]

SUBCOMMANDS:
    format      Format Meld source code
    lint        Analyze code for issues
    debug       Debug Meld programs
    profile     Profile program performance

FORMAT OPTIONS:
    --check         Check formatting without modifying files
    --recursive     Process directories recursively
    --indent-size   Number of spaces for indentation (default: 4)
    --max-length    Maximum line length (default: 100)
    --use-tabs      Use tabs instead of spaces

LINT OPTIONS:
    --fix           Apply automatic fixes
    --recursive     Process directories recursively
    --rules         Comma-separated list of rules to enable
    --disable       Comma-separated list of rules to disable
    --severity      Minimum severity level (error|warning|info|hint)

DEBUG OPTIONS:
    --breakpoint    Set breakpoint at file:line
    --step          Start in step mode
    --trace         Enable execution tracing

PROFILE OPTIONS:
    --cpu           Enable CPU profiling (default: true)
    --memory        Enable memory profiling (default: true)
    --duration      Profiling duration in seconds (default: 30)
    --output        Output file for profile data

EXAMPLES:
    meld dev format src/
    meld dev lint --fix --recursive src/
    meld dev debug --breakpoint main.meld:10 program.meld
    meld dev profile --output profile.txt program.meld
)";
}

std::string DevToolsModule::get_usage() const {
    return "meld dev <subcommand> [options] [files...]";
}

std::vector<std::string> DevToolsModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {"format", "lint", "debug", "profile"};
    
    std::vector<std::string> matches;
    for (const auto& completion : completions) {
        if (completion.find(partial) == 0) {
            matches.push_back(completion);
        }
    }
    
    return matches;
}

bool DevToolsModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.subcommand.empty()) {
        error_message = "Subcommand required";
        return false;
    }
    
    std::vector<std::string> valid_subcommands = {"format", "lint", "debug", "profile"};
    if (std::find(valid_subcommands.begin(), valid_subcommands.end(), args.subcommand) 
        == valid_subcommands.end()) {
        error_message = "Invalid subcommand: " + args.subcommand;
        return false;
    }
    
    return true;
}
CommandResult DevToolsModule::handle_format_command(const CommandArgs& args) {
    auto options = parse_format_options(args);
    bool stdout_mode = args.flags.count("stdout") > 0;
    
    if (args.positional.empty()) {
        std::cerr << "No files specified for formatting" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    FormatResult combined_result;
    combined_result.success = true;
    
    for (const auto& path_str : args.positional) {
        std::filesystem::path path(path_str);

        // File-not-found check
        if (!std::filesystem::exists(path)) {
            std::cerr << "error: file not found: " << path_str << std::endl;
            return CommandResult::Error;
        }

        // Non-.meld extension warning
        if (std::filesystem::is_regular_file(path) && path.extension() != ".meld") {
            std::cerr << "warning: file does not have .meld extension: " << path_str << std::endl;
        }

        if (stdout_mode && std::filesystem::is_regular_file(path)) {
            // --stdout mode: format and print to stdout, don't modify file
            std::ifstream file(path);
            if (!file.is_open()) {
                std::cerr << "error: cannot open file: " << path_str << std::endl;
                combined_result.success = false;
                continue;
            }
            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();

            auto result = formatter_->format_code(source, options);
            if (result.success) {
                std::cout << result.formatted_code;
                combined_result.files_processed++;
            } else {
                combined_result.success = false;
                combined_result.errors.insert(combined_result.errors.end(),
                                            result.errors.begin(), result.errors.end());
            }
        } else if (options.check_only) {
            // --check mode: check formatting, exit 1 if any file needs formatting
            FormatResult result;
            if (std::filesystem::is_directory(path)) {
                result = formatter_->format_directory(path, options);
            } else {
                // Read file and check without modifying
                std::ifstream file(path);
                if (!file.is_open()) {
                    std::cerr << "error: cannot open file: " << path_str << std::endl;
                    combined_result.success = false;
                    continue;
                }
                std::string source((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                result.success = true;
                result.files_processed = 1;
                result.needs_formatting = formatter_->needs_formatting(source, options);
            }

            combined_result.files_processed += result.files_processed;
            if (result.needs_formatting) {
                combined_result.needs_formatting = true;
            }
            if (!result.success) {
                combined_result.success = false;
                combined_result.errors.insert(combined_result.errors.end(),
                                            result.errors.begin(), result.errors.end());
            }
        } else {
            // Default mode: format and write back to file
            FormatResult result;
            if (std::filesystem::is_directory(path)) {
                result = formatter_->format_directory(path, options);
            } else {
                result = formatter_->format_file(path, options);
            }

            combined_result.files_processed += result.files_processed;
            combined_result.files_changed += result.files_changed;

            if (!result.success) {
                combined_result.success = false;
                combined_result.errors.insert(combined_result.errors.end(),
                                            result.errors.begin(), result.errors.end());
            }
        }
    }
    
    if (options.check_only) {
        if (combined_result.needs_formatting) {
            std::cout << "Some files need formatting" << std::endl;
            return CommandResult::Error; // exit code 1
        }
        std::cout << "All files are properly formatted" << std::endl;
        return CommandResult::Success; // exit code 0
    }

    if (!stdout_mode) {
        print_format_result(combined_result);
    }
    return combined_result.success ? CommandResult::Success : CommandResult::Error;
}

CommandResult DevToolsModule::handle_lint_command(const CommandArgs& args) {
    auto options = parse_lint_options(args);
    
    if (args.positional.empty()) {
        std::cout << "No files specified for linting" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    LintResult combined_result;
    combined_result.success = true;
    
    for (const auto& path_str : args.positional) {
        std::filesystem::path path(path_str);
        LintResult result;
        
        if (std::filesystem::is_directory(path)) {
            result = linter_->lint_directory(path, options);
        } else if (std::filesystem::is_regular_file(path)) {
            result = linter_->lint_file(path, options);
        } else {
            std::cout << "Invalid path: " << path_str << std::endl;
            continue;
        }
        
        combined_result.files_processed += result.files_processed;
        combined_result.fixes_applied += result.fixes_applied;
        combined_result.issues.insert(combined_result.issues.end(),
                                    result.issues.begin(), result.issues.end());
        
        if (!result.success) {
            combined_result.success = false;
        }
    }
    
    print_lint_result(combined_result);
    return combined_result.success && !combined_result.has_errors() ? 
           CommandResult::Success : CommandResult::Error;
}

CommandResult DevToolsModule::handle_debug_command(const CommandArgs& args) {
    auto options = parse_debug_options(args);
    
    if (args.positional.empty()) {
        std::cout << "No program specified for debugging" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::filesystem::path program_path(args.positional[0]);
    std::vector<std::string> program_args(args.positional.begin() + 1, args.positional.end());
    
    if (debugger_->start_debug_session(program_path, program_args, options)) {
        std::cout << "Debug session started. Use debug commands to control execution." << std::endl;
        return CommandResult::Success;
    } else {
        std::cout << "Failed to start debug session" << std::endl;
        return CommandResult::Error;
    }
}

CommandResult DevToolsModule::handle_profile_command(const CommandArgs& args) {
    auto options = parse_profile_options(args);
    
    if (args.positional.empty()) {
        std::cout << "No program specified for profiling" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::filesystem::path program_path(args.positional[0]);
    std::vector<std::string> program_args(args.positional.begin() + 1, args.positional.end());
    
    auto result = profiler_->profile_program(program_path, program_args, options);
    
    print_profile_result(result);
    return result.success ? CommandResult::Success : CommandResult::Error;
}
FormatOptions DevToolsModule::parse_format_options(const CommandArgs& args) const {
    FormatOptions options;
    
    if (args.flags.count("check")) {
        options.check_only = true;
    }
    
    if (args.flags.count("recursive")) {
        options.recursive = true;
    }
    
    if (args.flags.count("use-tabs")) {
        options.use_tabs = true;
    }
    
    if (args.options.count("indent-size")) {
        try {
            options.indent_size = std::stoi(args.options.at("indent-size"));
        } catch (const std::exception&) {
            // Use default
        }
    }
    
    if (args.options.count("max-length")) {
        try {
            options.max_line_length = std::stoi(args.options.at("max-length"));
        } catch (const std::exception&) {
            // Use default
        }
    }
    
    return options;
}

LintOptions DevToolsModule::parse_lint_options(const CommandArgs& args) const {
    LintOptions options;
    
    if (args.flags.count("fix")) {
        options.auto_fix = true;
    }
    
    if (args.flags.count("recursive")) {
        options.recursive = true;
    }
    
    if (args.options.count("rules")) {
        std::string rules_str = args.options.at("rules");
        std::istringstream iss(rules_str);
        std::string rule;
        while (std::getline(iss, rule, ',')) {
            options.enabled_rules.push_back(rule);
        }
    }
    
    if (args.options.count("disable")) {
        std::string disable_str = args.options.at("disable");
        std::istringstream iss(disable_str);
        std::string rule;
        while (std::getline(iss, rule, ',')) {
            options.disabled_rules.push_back(rule);
        }
    }
    
    if (args.options.count("severity")) {
        std::string severity_str = args.options.at("severity");
        if (severity_str == "error") {
            options.min_severity = LintSeverity::Error;
        } else if (severity_str == "warning") {
            options.min_severity = LintSeverity::Warning;
        } else if (severity_str == "info") {
            options.min_severity = LintSeverity::Info;
        } else if (severity_str == "hint") {
            options.min_severity = LintSeverity::Hint;
        }
    }
    
    return options;
}

DevToolsDebugOptions DevToolsModule::parse_debug_options(const CommandArgs& args) const {
    DevToolsDebugOptions options;
    
    if (args.flags.count("step")) {
        options.step_mode = true;
    }
    
    if (args.flags.count("trace")) {
        options.trace_execution = true;
    }
    
    if (args.options.count("breakpoint")) {
        std::string bp_str = args.options.at("breakpoint");
        size_t colon_pos = bp_str.find(':');
        if (colon_pos != std::string::npos) {
            std::string file = bp_str.substr(0, colon_pos);
            std::string line_str = bp_str.substr(colon_pos + 1);
            try {
                size_t line = std::stoul(line_str);
                options.breakpoints.emplace_back(file, line);
            } catch (const std::exception&) {
                // Invalid breakpoint format, ignore
            }
        }
    }
    
    return options;
}

ProfileOptions DevToolsModule::parse_profile_options(const CommandArgs& args) const {
    ProfileOptions options;
    
    if (args.flags.count("cpu")) {
        options.cpu_profiling = true;
    }
    
    if (args.flags.count("memory")) {
        options.memory_profiling = true;
    }
    
    if (args.flags.count("trace")) {
        options.trace_calls = true;
    }
    
    if (args.options.count("duration")) {
        try {
            int duration_secs = std::stoi(args.options.at("duration"));
            options.duration = std::chrono::seconds(duration_secs);
        } catch (const std::exception&) {
            // Use default
        }
    }
    
    if (args.options.count("output")) {
        options.output_file = args.options.at("output");
    }
    
    return options;
}
void DevToolsModule::print_format_result(const FormatResult& result) const {
    if (result.success) {
        std::cout << "Formatting completed successfully" << std::endl;
        std::cout << "Files processed: " << result.files_processed << std::endl;
        std::cout << "Files changed: " << result.files_changed << std::endl;
    } else {
        std::cout << "Formatting failed with errors:" << std::endl;
        for (const auto& error : result.errors) {
            std::cout << "  " << error << std::endl;
        }
    }
}

void DevToolsModule::print_lint_result(const LintResult& result) const {
    if (result.success) {
        std::cout << "Linting completed" << std::endl;
        std::cout << "Files processed: " << result.files_processed << std::endl;
        
        if (result.issues.empty()) {
            std::cout << "No issues found" << std::endl;
        } else {
            std::cout << "Issues found: " << result.issues.size() << std::endl;
            std::cout << "Errors: " << result.error_count() << std::endl;
            std::cout << "Warnings: " << result.warning_count() << std::endl;
            
            for (const auto& issue : result.issues) {
                std::cout << issue.format() << std::endl;
            }
        }
        
        if (result.fixes_applied > 0) {
            std::cout << "Fixes applied: " << result.fixes_applied << std::endl;
        }
    } else {
        std::cout << "Linting failed" << std::endl;
    }
}

void DevToolsModule::print_profile_result(const ProfileResult& result) const {
    if (result.success) {
        std::cout << result.report << std::endl;
        
        if (!result.hotspots.empty()) {
            std::cout << "Performance Hotspots:" << std::endl;
            for (const auto& hotspot : result.hotspots) {
                std::cout << "  " << hotspot << std::endl;
            }
            std::cout << std::endl;
        }
        
        if (!result.recommendations.empty()) {
            std::cout << "Optimization Recommendations:" << std::endl;
            for (const auto& recommendation : result.recommendations) {
                std::cout << "  " << recommendation << std::endl;
            }
        }
        
        if (!result.report_file.empty()) {
            std::cout << "Detailed report written to: " << result.report_file << std::endl;
        }
    } else {
        std::cout << "Profiling failed: " << result.report << std::endl;
    }
}

std::vector<std::filesystem::path> DevToolsModule::collect_meld_files(const std::filesystem::path& path, 
                                                                     bool recursive) const {
    std::vector<std::filesystem::path> files;
    
    if (std::filesystem::is_regular_file(path) && is_meld_file(path)) {
        files.push_back(path);
    } else if (std::filesystem::is_directory(path)) {
        try {
            if (recursive) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file() && is_meld_file(entry.path())) {
                        files.push_back(entry.path());
                    }
                }
            } else {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    if (entry.is_regular_file() && is_meld_file(entry.path())) {
                        files.push_back(entry.path());
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "Error accessing directory: " << e.what() << std::endl;
        }
    }
    
    return files;
}

bool DevToolsModule::is_meld_file(const std::filesystem::path& file) const {
    return file.extension() == ".meld";
}

} // namespace meld::cli