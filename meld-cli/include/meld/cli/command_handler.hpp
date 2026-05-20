#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace meld::cli {

/**
 * Command line arguments structure
 */
struct CommandArgs {
    std::string command;
    std::string subcommand;
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;
    std::map<std::string, std::string> options;
};

/**
 * Command execution result
 */
enum class CommandResult {
    Success,
    Error,
    InvalidArguments,
    NotFound
};

/**
 * Abstract base class for command handlers
 */
class CommandHandler {
public:
    virtual ~CommandHandler() = default;

    /**
     * Execute the command with given arguments
     */
    virtual CommandResult execute(const CommandArgs& args) = 0;

    /**
     * Get help text for this command
     */
    virtual std::string get_help() const = 0;

    /**
     * Get command name
     */
    virtual std::string get_name() const = 0;

    /**
     * Get command description
     */
    virtual std::string get_description() const = 0;

    /**
     * Get command usage string
     */
    virtual std::string get_usage() const = 0;

    /**
     * Get command completion suggestions for partial input
     */
    virtual std::vector<std::string> get_completions(const std::string& partial) const = 0;

    /**
     * Validate command arguments
     */
    virtual bool validate_args(const CommandArgs& args, std::string& error_message) const = 0;
};

/**
 * Base implementation of CommandHandler with common functionality
 */
class BaseCommandHandler : public CommandHandler {
public:
    explicit BaseCommandHandler(const std::string& name, const std::string& description);
    virtual ~BaseCommandHandler() = default;

    std::string get_name() const override { return name_; }
    std::string get_description() const override { return description_; }
    
    // Default implementations that can be overridden
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

protected:
    std::string name_;
    std::string description_;
};

} // namespace meld::cli