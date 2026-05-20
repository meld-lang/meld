#include "meld/cli/shell_integration_module.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cstdlib>

namespace meld::cli {

ShellIntegrationModule::ShellIntegrationModule(std::shared_ptr<ErrorHandler> error_handler)
    : BaseCommandHandler("shell", "Shell integration features including completion scripts and output formatting")
    , error_handler_(error_handler)
    , verbosity_level_(VerbosityLevel::Normal)
    , output_format_(OutputFormat::Text) {
}

CommandResult ShellIntegrationModule::execute(const CommandArgs& args) {
    if (args.subcommand == "completion") {
        return handle_completion_command(args);
    } else if (args.subcommand == "format") {
        return handle_format_command(args);
    } else {
        error_handler_->report_invalid_arguments(
            "Unknown subcommand: " + args.subcommand,
            get_usage()
        );
        return CommandResult::InvalidArguments;
    }
}

std::string ShellIntegrationModule::get_help() const {
    std::ostringstream oss;
    oss << "Shell Integration Module" << std::endl;
    oss << std::endl;
    oss << "Provides shell integration features including completion scripts," << std::endl;
    oss << "JSON output formatting, and verbosity control." << std::endl;
    oss << std::endl;
    oss << "Subcommands:" << std::endl;
    oss << "  completion <shell>  Generate completion script for specified shell" << std::endl;
    oss << "  format <format>     Set output format (text, json, yaml, table)" << std::endl;
    oss << std::endl;
    oss << "Global flags:" << std::endl;
    oss << "  --json              Output in JSON format" << std::endl;
    oss << "  --quiet             Suppress non-essential output" << std::endl;
    oss << "  --verbose           Provide detailed operation information" << std::endl;
    oss << std::endl;
    oss << "Examples:" << std::endl;
    oss << "  meld completion bash > ~/.bash_completion.d/meld" << std::endl;
    oss << "  meld --json compile file.meld" << std::endl;
    oss << "  meld --verbose build" << std::endl;
    
    return oss.str();
}

std::string ShellIntegrationModule::get_usage() const {
    return "meld shell <subcommand> [options]";
}

std::vector<std::string> ShellIntegrationModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    // Complete subcommands
    std::vector<std::string> subcommands = {"completion", "format"};
    for (const auto& cmd : subcommands) {
        if (cmd.find(partial) == 0) {
            completions.push_back(cmd);
        }
    }
    
    return completions;
}

bool ShellIntegrationModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.subcommand.empty()) {
        error_message = "Subcommand required";
        return false;
    }
    
    if (args.subcommand != "completion" && args.subcommand != "format") {
        error_message = "Invalid subcommand: " + args.subcommand;
        return false;
    }
    
    return true;
}

std::string ShellIntegrationModule::generate_completion_script(ShellType shell_type) const {
    switch (shell_type) {
        case ShellType::Bash:
            return generate_bash_completion();
        case ShellType::Zsh:
            return generate_zsh_completion();
        case ShellType::Fish:
            return generate_fish_completion();
        case ShellType::PowerShell:
            return generate_powershell_completion();
        default:
            return "";
    }
}

std::string ShellIntegrationModule::generate_bash_completion() const {
    std::ostringstream oss;
    oss << "#!/bin/bash" << std::endl;
    oss << "# Bash completion script for meld CLI" << std::endl;
    oss << std::endl;
    oss << "_meld_completion() {" << std::endl;
    oss << "    local cur prev opts" << std::endl;
    oss << "    COMPREPLY=()" << std::endl;
    oss << "    cur=\"${COMP_WORDS[COMP_CWORD]}\"" << std::endl;
    oss << "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"" << std::endl;
    oss << std::endl;
    oss << "    # Main commands" << std::endl;
    oss << "    if [[ ${COMP_CWORD} == 1 ]]; then" << std::endl;
    oss << "        opts=\"" << generate_command_list_for_completion() << "\"" << std::endl;
    oss << "        COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "        return 0" << std::endl;
    oss << "    fi" << std::endl;
    oss << std::endl;
    oss << "    # Global options" << std::endl;
    oss << "    case \"${prev}\" in" << std::endl;
    oss << "        --output)" << std::endl;
    oss << "            COMPREPLY=( $(compgen -f -- ${cur}) )" << std::endl;
    oss << "            return 0" << std::endl;
    oss << "            ;;" << std::endl;
    oss << "        --target)" << std::endl;
    oss << "            opts=\"jvm go cpp wasm\"" << std::endl;
    oss << "            COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "            return 0" << std::endl;
    oss << "            ;;" << std::endl;
    oss << "    esac" << std::endl;
    oss << std::endl;
    oss << "    # Subcommand-specific completions" << std::endl;
    oss << "    case \"${COMP_WORDS[1]}\" in" << std::endl;
    oss << "        vm)" << std::endl;
    oss << "            if [[ ${COMP_CWORD} == 2 ]]; then" << std::endl;
    oss << "                opts=\"status start stop restart shell prune logs\"" << std::endl;
    oss << "                COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "                return 0" << std::endl;
    oss << "            fi" << std::endl;
    oss << "            case \"${COMP_WORDS[2]}\" in" << std::endl;
    oss << "                logs)" << std::endl;
    oss << "                    opts=\"--no-follow --json\"" << std::endl;
    oss << "                    COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "                    return 0" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "                status)" << std::endl;
    oss << "                    opts=\"--json\"" << std::endl;
    oss << "                    COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "                    return 0" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "            esac" << std::endl;
    oss << "            ;;" << std::endl;
    oss << "        run)" << std::endl;
    oss << "            if [[ ${cur} == --isolation=* ]]; then" << std::endl;
    oss << "                local prefix=\"--isolation=\"" << std::endl;
    oss << "                local val=\"${cur#${prefix}}\"" << std::endl;
    oss << "                opts=\"srt finch microvm\"" << std::endl;
    oss << "                COMPREPLY=( $(compgen -P \"${prefix}\" -W \"${opts}\" -- ${val}) )" << std::endl;
    oss << "                return 0" << std::endl;
    oss << "            fi" << std::endl;
    oss << "            if [[ ${cur} == -* ]]; then" << std::endl;
    oss << "                opts=\"--watch --isolation=\"" << std::endl;
    oss << "                COMPREPLY=( $(compgen -W \"${opts}\" -- ${cur}) )" << std::endl;
    oss << "                return 0" << std::endl;
    oss << "            fi" << std::endl;
    oss << "            COMPREPLY=( $(compgen -f -X '!*.meld' -- ${cur}) )" << std::endl;
    oss << "            return 0" << std::endl;
    oss << "            ;;" << std::endl;
    oss << "    esac" << std::endl;
    oss << std::endl;
    oss << "    # File completion for .meld files" << std::endl;
    oss << "    if [[ ${cur} == *.meld || ${cur} == \"\" ]]; then" << std::endl;
    oss << "        COMPREPLY=( $(compgen -f -X '!*.meld' -- ${cur}) )" << std::endl;
    oss << "        return 0" << std::endl;
    oss << "    fi" << std::endl;
    oss << std::endl;
    oss << "    # Default to file completion" << std::endl;
    oss << "    COMPREPLY=( $(compgen -f -- ${cur}) )" << std::endl;
    oss << "}" << std::endl;
    oss << std::endl;
    oss << "complete -F _meld_completion meld" << std::endl;
    
    return oss.str();
}

std::string ShellIntegrationModule::generate_zsh_completion() const {
    std::ostringstream oss;
    oss << "#compdef meld" << std::endl;
    oss << "# Zsh completion script for meld CLI" << std::endl;
    oss << std::endl;
    oss << "_meld() {" << std::endl;
    oss << "    local context state line" << std::endl;
    oss << "    typeset -A opt_args" << std::endl;
    oss << std::endl;
    oss << "    _arguments -C \\" << std::endl;
    oss << "        '(--help -h)'{--help,-h}'[Show help information]' \\" << std::endl;
    oss << "        '(--version -v)'{--version,-v}'[Show version information]' \\" << std::endl;
    oss << "        '(--json)--json[Output in JSON format]' \\" << std::endl;
    oss << "        '(--quiet -q)'{--quiet,-q}'[Suppress non-essential output]' \\" << std::endl;
    oss << "        '(--verbose)--verbose[Provide detailed operation information]' \\" << std::endl;
    oss << "        '1: :_meld_commands' \\" << std::endl;
    oss << "        '*::arg:->args'" << std::endl;
    oss << std::endl;
    oss << "    case $state in" << std::endl;
    oss << "        args)" << std::endl;
    oss << "            case $words[1] in" << std::endl;
    oss << "                compile)" << std::endl;
    oss << "                    _arguments \\" << std::endl;
    oss << "                        '--target[Compilation target]:target:(jvm go cpp wasm)' \\" << std::endl;
    oss << "                        '--output[Output file]:file:_files' \\" << std::endl;
    oss << "                        '*:file:_files -g \"*.meld\"'" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "                run)" << std::endl;
    oss << "                    _arguments \\" << std::endl;
    oss << "                        '--watch[Watch for file changes]' \\" << std::endl;
    oss << "                        '--isolation=[Isolation backend]:backend:(srt finch microvm)' \\" << std::endl;
    oss << "                        '*:file:_files -g \"*.meld\"'" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "                vm)" << std::endl;
    oss << "                    local -a vm_commands; vm_commands=(" << std::endl;
    oss << "                        'status:Show VM status'" << std::endl;
    oss << "                        'start:Start the Lima VM'" << std::endl;
    oss << "                        'stop:Stop the Lima VM'" << std::endl;
    oss << "                        'restart:Restart the Lima VM'" << std::endl;
    oss << "                        'shell:Open a shell in the VM'" << std::endl;
    oss << "                        'prune:Remove unused VM data'" << std::endl;
    oss << "                        'logs:Show VM logs'" << std::endl;
    oss << "                    )" << std::endl;
    oss << "                    _describe 'vm commands' vm_commands" << std::endl;
    oss << "                    case $words[2] in" << std::endl;
    oss << "                        logs)" << std::endl;
    oss << "                            _arguments \\" << std::endl;
    oss << "                                '--no-follow[Do not follow log output]' \\" << std::endl;
    oss << "                                '--json[Output in JSON format]'" << std::endl;
    oss << "                            ;;" << std::endl;
    oss << "                        status)" << std::endl;
    oss << "                            _arguments \\" << std::endl;
    oss << "                                '--json[Output in JSON format]'" << std::endl;
    oss << "                            ;;" << std::endl;
    oss << "                    esac" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "                completion)" << std::endl;
    oss << "                    _arguments \\" << std::endl;
    oss << "                        '1:shell:(bash zsh fish powershell)'" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "                *)" << std::endl;
    oss << "                    _files" << std::endl;
    oss << "                    ;;" << std::endl;
    oss << "            esac" << std::endl;
    oss << "            ;;" << std::endl;
    oss << "    esac" << std::endl;
    oss << "}" << std::endl;
    oss << std::endl;
    oss << "_meld_commands() {" << std::endl;
    oss << "    local commands; commands=(" << std::endl;
    
    // Add commands with descriptions
    std::vector<std::string> commands = get_all_commands();
    for (const auto& cmd : commands) {
        oss << "        '" << cmd << ":Description for " << cmd << "'" << std::endl;
    }
    
    oss << "    )" << std::endl;
    oss << "    _describe 'commands' commands" << std::endl;
    oss << "}" << std::endl;
    oss << std::endl;
    oss << "_meld \"$@\"" << std::endl;
    
    return oss.str();
}

std::string ShellIntegrationModule::generate_fish_completion() const {
    std::ostringstream oss;
    oss << "# Fish completion script for meld CLI" << std::endl;
    oss << std::endl;
    oss << "# Main commands" << std::endl;
    
    std::vector<std::string> commands = get_all_commands();
    for (const auto& cmd : commands) {
        oss << "complete -c meld -n '__fish_use_subcommand' -a '" << cmd << "' -d 'Description for " << cmd << "'" << std::endl;
    }
    
    oss << std::endl;
    oss << "# Global options" << std::endl;
    oss << "complete -c meld -l help -s h -d 'Show help information'" << std::endl;
    oss << "complete -c meld -l version -s v -d 'Show version information'" << std::endl;
    oss << "complete -c meld -l json -d 'Output in JSON format'" << std::endl;
    oss << "complete -c meld -l quiet -s q -d 'Suppress non-essential output'" << std::endl;
    oss << "complete -c meld -l verbose -d 'Provide detailed operation information'" << std::endl;
    oss << std::endl;
    oss << "# Command-specific options" << std::endl;
    oss << "complete -c meld -n '__fish_seen_subcommand_from compile' -l target -a 'jvm go cpp wasm' -d 'Compilation target'" << std::endl;
    oss << "complete -c meld -n '__fish_seen_subcommand_from compile' -l output -F -d 'Output file'" << std::endl;
    oss << "complete -c meld -n '__fish_seen_subcommand_from run' -l watch -d 'Watch for file changes'" << std::endl;
    oss << "complete -c meld -n '__fish_seen_subcommand_from completion' -a 'bash zsh fish powershell' -d 'Shell type'" << std::endl;
    oss << std::endl;
    oss << "# File completions" << std::endl;
    oss << "complete -c meld -n '__fish_seen_subcommand_from compile run' -a '(__fish_complete_suffix .meld)' -d 'Meld source file'" << std::endl;
    
    return oss.str();
}

std::string ShellIntegrationModule::generate_powershell_completion() const {
    std::ostringstream oss;
    oss << "# PowerShell completion script for meld CLI" << std::endl;
    oss << std::endl;
    oss << "Register-ArgumentCompleter -Native -CommandName meld -ScriptBlock {" << std::endl;
    oss << "    param($commandName, $wordToComplete, $cursorPosition)" << std::endl;
    oss << std::endl;
    oss << "    $commands = @(" << std::endl;
    
    std::vector<std::string> commands = get_all_commands();
    for (size_t i = 0; i < commands.size(); ++i) {
        oss << "        '" << commands[i] << "'";
        if (i < commands.size() - 1) {
            oss << ",";
        }
        oss << std::endl;
    }
    
    oss << "    )" << std::endl;
    oss << std::endl;
    oss << "    $globalOptions = @(" << std::endl;
    oss << "        '--help', '-h'," << std::endl;
    oss << "        '--version', '-v'," << std::endl;
    oss << "        '--json'," << std::endl;
    oss << "        '--quiet', '-q'," << std::endl;
    oss << "        '--verbose'" << std::endl;
    oss << "    )" << std::endl;
    oss << std::endl;
    oss << "    if ($wordToComplete -like '--*' -or $wordToComplete -like '-*') {" << std::endl;
    oss << "        $globalOptions | Where-Object { $_ -like \"$wordToComplete*\" }" << std::endl;
    oss << "    } else {" << std::endl;
    oss << "        $commands | Where-Object { $_ -like \"$wordToComplete*\" }" << std::endl;
    oss << "    }" << std::endl;
    oss << "}" << std::endl;
    
    return oss.str();
}

json ShellIntegrationModule::format_as_json(const std::string& command, const std::map<std::string, std::string>& data) const {
    json result;
    result["command"] = command;
    result["timestamp"] = std::time(nullptr);
    result["data"] = json::object();
    
    for (const auto& [key, value] : data) {
        result["data"][key] = value;
    }
    
    return result;
}

json ShellIntegrationModule::format_error_as_json(const std::string& error_message, const std::string& error_code) const {
    json result;
    result["success"] = false;
    result["error"] = {
        {"message", error_message},
        {"code", error_code.empty() ? "GENERIC_ERROR" : error_code},
        {"timestamp", std::time(nullptr)}
    };
    
    return result;
}

json ShellIntegrationModule::format_list_as_json(const std::vector<std::string>& items) const {
    json result;
    result["items"] = items;
    result["count"] = items.size();
    result["timestamp"] = std::time(nullptr);
    
    return result;
}

json ShellIntegrationModule::format_result_as_json(bool success, const std::string& message, const json& data) const {
    json result;
    result["success"] = success;
    result["message"] = message;
    result["timestamp"] = std::time(nullptr);
    
    if (!data.is_null()) {
        result["data"] = data;
    }
    
    return result;
}

void ShellIntegrationModule::set_verbosity(VerbosityLevel level) {
    verbosity_level_ = level;
}

VerbosityLevel ShellIntegrationModule::get_verbosity() const {
    return verbosity_level_;
}

bool ShellIntegrationModule::should_output_message(VerbosityLevel message_level) const {
    switch (verbosity_level_) {
        case VerbosityLevel::Quiet:
            return message_level == VerbosityLevel::Quiet;
        case VerbosityLevel::Normal:
            return message_level != VerbosityLevel::Verbose;
        case VerbosityLevel::Verbose:
            return true;
        default:
            return true;
    }
}

void ShellIntegrationModule::output_message(const std::string& message, VerbosityLevel level) const {
    if (should_output_message(level)) {
        std::cout << message << std::endl;
    }
}

void ShellIntegrationModule::output_verbose(const std::string& message) const {
    output_message("[VERBOSE] " + message, VerbosityLevel::Verbose);
}

void ShellIntegrationModule::output_quiet(const std::string& message) const {
    output_message(message, VerbosityLevel::Quiet);
}

void ShellIntegrationModule::output_json(const json& data) const {
    std::cout << data.dump(2) << std::endl;
}

std::string ShellIntegrationModule::format_output(const std::map<std::string, std::string>& data, OutputFormat format) const {
    switch (format) {
        case OutputFormat::JSON: {
            json j;
            for (const auto& [key, value] : data) {
                j[key] = value;
            }
            return j.dump(2);
        }
        case OutputFormat::YAML: {
            std::ostringstream oss;
            for (const auto& [key, value] : data) {
                oss << key << ": " << value << std::endl;
            }
            return oss.str();
        }
        case OutputFormat::Table: {
            std::vector<std::map<std::string, std::string>> rows = {data};
            std::vector<std::string> columns;
            for (const auto& [key, value] : data) {
                columns.push_back(key);
            }
            return format_table(rows, columns);
        }
        case OutputFormat::Text:
        default: {
            std::ostringstream oss;
            for (const auto& [key, value] : data) {
                oss << key << ": " << value << std::endl;
            }
            return oss.str();
        }
    }
}

std::string ShellIntegrationModule::format_table(const std::vector<std::map<std::string, std::string>>& rows, 
                                               const std::vector<std::string>& columns) const {
    if (rows.empty() || columns.empty()) {
        return "";
    }
    
    // Calculate column widths
    std::map<std::string, size_t> widths;
    for (const auto& col : columns) {
        widths[col] = col.length();
    }
    
    for (const auto& row : rows) {
        for (const auto& col : columns) {
            auto it = row.find(col);
            if (it != row.end()) {
                widths[col] = std::max(widths[col], it->second.length());
            }
        }
    }
    
    std::ostringstream oss;
    
    // Header
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << " | ";
        oss << std::left << std::setw(widths[columns[i]]) << columns[i];
    }
    oss << std::endl;
    
    // Separator
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << "-+-";
        oss << std::string(widths[columns[i]], '-');
    }
    oss << std::endl;
    
    // Rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) oss << " | ";
            auto it = row.find(columns[i]);
            std::string value = (it != row.end()) ? it->second : "";
            oss << std::left << std::setw(widths[columns[i]]) << value;
        }
        oss << std::endl;
    }
    
    return oss.str();
}

std::optional<ShellType> ShellIntegrationModule::detect_current_shell() {
    const char* shell_env = std::getenv("SHELL");
    if (!shell_env) {
        return std::nullopt;
    }
    
    std::string shell_path(shell_env);
    std::filesystem::path path(shell_path);
    std::string shell_name = path.filename().string();
    
    return string_to_shell_type(shell_name);
}

std::string ShellIntegrationModule::shell_type_to_string(ShellType shell_type) {
    switch (shell_type) {
        case ShellType::Bash: return "bash";
        case ShellType::Zsh: return "zsh";
        case ShellType::Fish: return "fish";
        case ShellType::PowerShell: return "powershell";
        default: return "unknown";
    }
}

std::optional<ShellType> ShellIntegrationModule::string_to_shell_type(const std::string& shell_name) {
    if (shell_name == "bash") return ShellType::Bash;
    if (shell_name == "zsh") return ShellType::Zsh;
    if (shell_name == "fish") return ShellType::Fish;
    if (shell_name == "powershell" || shell_name == "pwsh") return ShellType::PowerShell;
    return std::nullopt;
}

CommandResult ShellIntegrationModule::handle_completion_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        error_handler_->report_invalid_arguments(
            "Shell type required for completion command",
            "Usage: meld completion <shell>"
        );
        return CommandResult::InvalidArguments;
    }
    
    std::string shell_name = args.positional[0];
    auto shell_type = string_to_shell_type(shell_name);
    
    if (!shell_type) {
        error_handler_->report_invalid_arguments(
            "Unsupported shell: " + shell_name,
            "Supported shells: bash, zsh, fish, powershell"
        );
        return CommandResult::InvalidArguments;
    }
    
    std::string completion_script = generate_completion_script(*shell_type);
    
    if (args.flags.count("json")) {
        json result = format_result_as_json(true, "Completion script generated", {
            {"shell", shell_name},
            {"script", completion_script}
        });
        output_json(result);
    } else {
        std::cout << completion_script;
    }
    
    return CommandResult::Success;
}

CommandResult ShellIntegrationModule::handle_format_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        error_handler_->report_invalid_arguments(
            "Format type required",
            "Usage: meld format <format>"
        );
        return CommandResult::InvalidArguments;
    }
    
    std::string format_name = args.positional[0];
    OutputFormat format;
    
    if (format_name == "text") {
        format = OutputFormat::Text;
    } else if (format_name == "json") {
        format = OutputFormat::JSON;
    } else if (format_name == "yaml") {
        format = OutputFormat::YAML;
    } else if (format_name == "table") {
        format = OutputFormat::Table;
    } else {
        error_handler_->report_invalid_arguments(
            "Unsupported format: " + format_name,
            "Supported formats: text, json, yaml, table"
        );
        return CommandResult::InvalidArguments;
    }
    
    output_format_ = format;
    
    if (args.flags.count("json")) {
        json result = format_result_as_json(true, "Output format set", {
            {"format", format_name}
        });
        output_json(result);
    } else {
        output_message("Output format set to: " + format_name);
    }
    
    return CommandResult::Success;
}

std::string ShellIntegrationModule::generate_command_list_for_completion() const {
    std::vector<std::string> commands = get_all_commands();
    std::ostringstream oss;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) oss << " ";
        oss << commands[i];
    }
    return oss.str();
}

std::vector<std::string> ShellIntegrationModule::get_all_commands() const {
    return {
        "audit", "build", "completion", "config", "daemon", "debug",
        "dev", "fmt", "help", "init", "lsp", "mcp", "module", "new",
        "profile", "run", "sign", "test", "update", "version", "vm"
    };
}

std::vector<std::string> ShellIntegrationModule::get_command_options(const std::string& command) const {
    // This would typically be populated from the actual command handlers
    // For now, return common options
    return {"--help", "--json", "--quiet", "--verbose"};
}

// CompletionModule implementation
CompletionModule::CompletionModule(ShellIntegrationModule* shell_integration)
    : BaseCommandHandler("completion", "Generate shell completion scripts")
    , shell_integration_(shell_integration) {
}

CommandResult CompletionModule::execute(const CommandArgs& args) {
    return shell_integration_->handle_completion_command(args);
}

std::string CompletionModule::get_help() const {
    return "Generate shell completion scripts for the meld CLI.\n\n"
           "Usage: meld completion <shell>\n\n"
           "Supported shells: bash, zsh, fish, powershell\n\n"
           "Examples:\n"
           "  meld completion bash > ~/.bash_completion.d/meld\n"
           "  meld completion zsh > ~/.zsh/completions/_meld";
}

std::string CompletionModule::get_usage() const {
    return "meld completion <shell>";
}

} // namespace meld::cli