#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace meld::cli {

using json = nlohmann::json;

/**
 * Output verbosity level
 */
enum class VerbosityLevel {
    Quiet,    // Minimal output, only essential information
    Normal,   // Standard output level
    Verbose   // Detailed operation information
};

/**
 * Shell completion script type
 */
enum class ShellType {
    Bash,
    Zsh,
    Fish,
    PowerShell
};

/**
 * Output format options
 */
enum class OutputFormat {
    Text,     // Human-readable text
    JSON,     // Machine-readable JSON
    YAML,     // YAML format
    Table     // Formatted table
};

/**
 * Shell integration module for completion scripts, JSON output, and verbosity control
 */
class ShellIntegrationModule : public BaseCommandHandler {
public:
    explicit ShellIntegrationModule(std::shared_ptr<ErrorHandler> error_handler);
    ~ShellIntegrationModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

    // Completion script generation
    std::string generate_completion_script(ShellType shell_type) const;
    std::string generate_bash_completion() const;
    std::string generate_zsh_completion() const;
    std::string generate_fish_completion() const;
    std::string generate_powershell_completion() const;

    // Tab completion functionality
    std::vector<std::string> get_command_completions(const std::string& partial) const;
    std::vector<std::string> get_file_completions(const std::string& partial, const std::string& extension = "") const;
    std::vector<std::string> get_option_completions(const std::string& command, const std::string& partial) const;

    // JSON output formatting
    json format_as_json(const std::string& command, const std::map<std::string, std::string>& data) const;
    json format_error_as_json(const std::string& error_message, const std::string& error_code = "") const;
    json format_list_as_json(const std::vector<std::string>& items) const;
    json format_result_as_json(bool success, const std::string& message, const json& data = json::object()) const;

    // Output verbosity control
    void set_verbosity(VerbosityLevel level);
    VerbosityLevel get_verbosity() const;
    bool should_output_message(VerbosityLevel message_level) const;
    
    // Output formatting
    void output_message(const std::string& message, VerbosityLevel level = VerbosityLevel::Normal) const;
    void output_verbose(const std::string& message) const;
    void output_quiet(const std::string& message) const;
    void output_json(const json& data) const;

    // Format conversion
    std::string format_output(const std::map<std::string, std::string>& data, OutputFormat format) const;
    std::string format_table(const std::vector<std::map<std::string, std::string>>& rows, 
                            const std::vector<std::string>& columns) const;

    // Shell detection
    static std::optional<ShellType> detect_current_shell();
    static std::string shell_type_to_string(ShellType shell_type);
    static std::optional<ShellType> string_to_shell_type(const std::string& shell_name);

private:
    std::shared_ptr<ErrorHandler> error_handler_;
    VerbosityLevel verbosity_level_;
    OutputFormat output_format_;

    // Command handlers
    CommandResult handle_completion_command(const CommandArgs& args);

    friend class CompletionModule;
    CommandResult handle_format_command(const CommandArgs& args);

    // Helper methods for completion generation
    std::string generate_command_list_for_completion() const;
    std::string generate_option_list_for_completion(const std::string& command) const;
    std::vector<std::string> get_all_commands() const;
    std::vector<std::string> get_command_options(const std::string& command) const;
    
    // JSON schema generation
    json generate_command_schema(const std::string& command) const;
    json generate_error_schema() const;
    
    // Output helpers
    std::string escape_for_shell(const std::string& str, ShellType shell_type) const;
    std::string quote_for_shell(const std::string& str, ShellType shell_type) const;
    
    // Verbosity helpers
    std::string verbosity_to_string(VerbosityLevel level) const;
    std::optional<VerbosityLevel> string_to_verbosity(const std::string& level_str) const;
};

/**
 * Completion command handler
 */
class CompletionModule : public BaseCommandHandler {
public:
    explicit CompletionModule(ShellIntegrationModule* shell_integration);
    ~CompletionModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;

private:
    ShellIntegrationModule* shell_integration_;
};

} // namespace meld::cli
