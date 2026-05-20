#pragma once

#include "command_handler.hpp"
#include "command_dispatcher.hpp"
#include "error_handler.hpp"
#include <memory>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Help and documentation system for the Meld CLI
 */
class HelpModule : public BaseCommandHandler {
public:
    explicit HelpModule(std::shared_ptr<CommandDispatcher> dispatcher, 
                       std::shared_ptr<ErrorHandler> error_handler);
    ~HelpModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

    /**
     * Get version information
     */
    std::string get_version_info() const;

    /**
     * Get general help text
     */
    std::string get_general_help() const;

    /**
     * Get help for a specific command
     */
    std::string get_command_help(const std::string& command) const;

    /**
     * Generate command suggestions for invalid commands
     */
    std::vector<std::string> suggest_commands(const std::string& invalid_command) const;

    /**
     * Format error message with helpful suggestions
     */
    std::string format_helpful_error(const std::string& error_message, 
                                   const std::vector<std::string>& suggestions = {}) const;

    /**
     * Set version information
     */
    void set_version(const std::string& version, const std::string& build_info = "");

private:
    std::shared_ptr<CommandDispatcher> dispatcher_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::string version_;
    std::string build_info_;

    // Helper methods
    std::string format_command_list() const;
    std::string format_usage_examples() const;
    bool is_valid_command(const std::string& command) const;
};

/**
 * Version command handler
 */
class VersionModule : public BaseCommandHandler {
public:
    explicit VersionModule(HelpModule* help_module);
    ~VersionModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;

private:
    HelpModule* help_module_;
};

} // namespace meld::cli