#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace meld::cli {

/**
 * Central command dispatcher and plugin architecture
 */
class CommandDispatcher {
public:
    explicit CommandDispatcher(std::shared_ptr<ErrorHandler> error_handler);
    virtual ~CommandDispatcher() = default;

    /**
     * Register a command handler
     */
    void register_handler(std::unique_ptr<CommandHandler> handler);

    /**
     * Dispatch command to appropriate handler
     */
    virtual CommandResult dispatch(const std::vector<std::string>& args);

    /**
     * Parse command line arguments into structured format
     */
    CommandArgs parse_args(const std::vector<std::string>& args);

    /**
     * Get list of all registered commands
     */
    virtual std::vector<std::string> get_commands() const;

    /**
     * Get help for a specific command
     */
    virtual std::string get_command_help(const std::string& command) const;

    /**
     * Get general help text
     */
    virtual std::string get_general_help() const;

    /**
     * Suggest similar commands for invalid command
     */
    std::vector<std::string> suggest_commands(const std::string& invalid_command) const;

    /**
     * Get command completions for shell integration
     */
    std::vector<std::string> get_completions(const std::vector<std::string>& args) const;

    /**
     * Check if a command exists
     */
    virtual bool has_command(const std::string& command) const;



protected:
    // Helper methods
    virtual CommandHandler* find_handler(const std::string& command) const;
    bool is_flag(const std::string& arg) const;
    bool is_option(const std::string& arg) const;
    std::string extract_flag_name(const std::string& arg) const;
    std::pair<std::string, std::string> extract_option(const std::string& arg) const;
    
    // Protected members for derived classes
    std::shared_ptr<ErrorHandler> error_handler_;

private:
    std::map<std::string, std::unique_ptr<CommandHandler>> handlers_;
};

} // namespace meld::cli