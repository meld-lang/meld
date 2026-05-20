#include "meld/cli/command_dispatcher.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace meld::cli {

BaseCommandHandler::BaseCommandHandler(const std::string& name, const std::string& description)
    : name_(name), description_(description) {
}

std::vector<std::string> BaseCommandHandler::get_completions(const std::string& /*partial*/) const {
    // Default implementation - no completions
    return {};
}

bool BaseCommandHandler::validate_args(const CommandArgs& /*args*/, std::string& /*error_message*/) const {
    // Default implementation - always valid
    return true;
}

CommandDispatcher::CommandDispatcher(std::shared_ptr<ErrorHandler> error_handler)
    : error_handler_(error_handler) {
}

void CommandDispatcher::register_handler(std::unique_ptr<CommandHandler> handler) {
    std::string name = handler->get_name();
    handlers_[name] = std::move(handler);
}

CommandResult CommandDispatcher::dispatch(const std::vector<std::string>& args) {
    if (args.empty()) {
        error_handler_->report_invalid_arguments("No command specified", get_general_help());
        return CommandResult::InvalidArguments;
    }

    CommandArgs parsed_args = parse_args(args);
    
    // Handle special built-in commands
    if (parsed_args.command == "help") {
        if (parsed_args.positional.empty()) {
            std::cout << get_general_help() << std::endl;
        } else {
            std::string help = get_command_help(parsed_args.positional[0]);
            if (help.empty()) {
                error_handler_->report_command_not_found(parsed_args.positional[0], suggest_commands(parsed_args.positional[0]));
                return CommandResult::NotFound;
            }
            std::cout << help << std::endl;
        }
        return CommandResult::Success;
    }

    CommandHandler* handler = find_handler(parsed_args.command);
    if (!handler) {
        error_handler_->report_command_not_found(parsed_args.command, suggest_commands(parsed_args.command));
        return CommandResult::NotFound;
    }

    // Handle per-command --help flag (e.g. `meld run --help`)
    if (parsed_args.flags.count("help") > 0 || parsed_args.flags.count("h") > 0 ||
        parsed_args.options.count("help") > 0) {
        // Route through HelpModule so built-in help text is included
        CommandHandler* help_handler = find_handler("help");
        if (help_handler) {
            CommandArgs help_args;
            help_args.command = "help";
            help_args.positional.push_back(parsed_args.command);
            return help_handler->execute(help_args);
        }
        // Fallback: use handler's own help
        std::cout << handler->get_help() << std::endl;
        return CommandResult::Success;
    }

    // Validate arguments
    std::string validation_error;
    if (!handler->validate_args(parsed_args, validation_error)) {
        error_handler_->report_invalid_arguments(validation_error, handler->get_usage());
        return CommandResult::InvalidArguments;
    }

    // Execute command
    try {
        return handler->execute(parsed_args);
    } catch (const std::exception& e) {
        error_handler_->report_internal_error("Command execution failed: " + std::string(e.what()));
        return CommandResult::Error;
    }
}

CommandArgs CommandDispatcher::parse_args(const std::vector<std::string>& args) {
    CommandArgs parsed;
    
    if (args.empty()) {
        return parsed;
    }
    
    parsed.command = args[0];
    
    bool found_subcommand = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (is_flag(arg)) {
            std::string flag_name = extract_flag_name(arg);
            parsed.flags[flag_name] = "true";
        } else if (is_option(arg)) {
            auto [key, value] = extract_option(arg);
            parsed.options[key] = value;
        } else {
            if (!found_subcommand && handlers_.find(parsed.command + " " + arg) != handlers_.end()) {
                parsed.subcommand = arg;
                found_subcommand = true;
            } else {
                parsed.positional.push_back(arg);
            }
        }
    }
    
    return parsed;
}

std::vector<std::string> CommandDispatcher::get_commands() const {
    std::vector<std::string> commands;
    for (const auto& [name, handler] : handlers_) {
        commands.push_back(name);
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

std::string CommandDispatcher::get_command_help(const std::string& command) const {
    CommandHandler* handler = find_handler(command);
    if (!handler) {
        return "";
    }
    return handler->get_help();
}

std::string CommandDispatcher::get_general_help() const {
    std::ostringstream oss;
    oss << "Meld CLI - Unified command-line interface for the Meld programming language" << std::endl;
    oss << std::endl;
    oss << "Usage: meld <command> [options] [arguments]" << std::endl;
    oss << std::endl;
    oss << "Available commands:" << std::endl;
    
    for (const auto& [name, handler] : handlers_) {
        oss << "  " << std::left << std::setw(15) << name << handler->get_description() << std::endl;
    }
    
    oss << std::endl;
    oss << "Use 'meld help <command>' for detailed help on a specific command." << std::endl;
    
    return oss.str();
}

std::vector<std::string> CommandDispatcher::suggest_commands(const std::string& invalid_command) const {
    std::vector<std::string> valid_commands = get_commands();
    return error_handler_->generate_command_suggestions(invalid_command, valid_commands);
}

std::vector<std::string> CommandDispatcher::get_completions(const std::vector<std::string>& args) const {
    if (args.empty()) {
        return get_commands();
    }
    
    if (args.size() == 1) {
        // Complete command names
        std::vector<std::string> completions;
        std::string partial = args[0];
        for (const auto& command : get_commands()) {
            if (command.find(partial) == 0) {
                completions.push_back(command);
            }
        }
        return completions;
    }
    
    // Delegate to specific command handler
    CommandHandler* handler = find_handler(args[0]);
    if (handler) {
        std::string partial = args.back();
        return handler->get_completions(partial);
    }
    
    return {};
}

bool CommandDispatcher::has_command(const std::string& command) const {
    return handlers_.find(command) != handlers_.end();
}

CommandHandler* CommandDispatcher::find_handler(const std::string& command) const {
    auto it = handlers_.find(command);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

bool CommandDispatcher::is_flag(const std::string& arg) const {
    return arg.length() >= 2 && arg[0] == '-' && arg[1] != '-' && arg.find('=') == std::string::npos;
}

bool CommandDispatcher::is_option(const std::string& arg) const {
    return (arg.length() >= 3 && arg.substr(0, 2) == "--") || arg.find('=') != std::string::npos;
}

std::string CommandDispatcher::extract_flag_name(const std::string& arg) const {
    if (arg.length() >= 2 && arg[0] == '-') {
        return arg.substr(1);
    }
    return arg;
}

std::pair<std::string, std::string> CommandDispatcher::extract_option(const std::string& arg) const {
    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos) {
        std::string key = arg.substr(0, eq_pos);
        std::string value = arg.substr(eq_pos + 1);
        
        // Remove leading dashes from key
        if (key.length() >= 2 && key.substr(0, 2) == "--") {
            key = key.substr(2);
        } else if (key.length() >= 1 && key[0] == '-') {
            key = key.substr(1);
        }
        
        return {key, value};
    }
    
    // Handle --key value format (would need next argument)
    std::string key = arg;
    if (key.length() >= 2 && key.substr(0, 2) == "--") {
        key = key.substr(2);
    } else if (key.length() >= 1 && key[0] == '-') {
        key = key.substr(1);
    }
    
    return {key, ""};
}

} // namespace meld::cli