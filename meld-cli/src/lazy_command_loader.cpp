#include "meld/cli/lazy_command_loader.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>

namespace meld::cli {

// LazyCommandLoader implementation

void LazyCommandLoader::register_factory(const std::string& command, CommandHandlerFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_[command] = std::move(factory);
}

CommandHandler* LazyCommandLoader::get_handler(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already loaded
    auto loaded_it = loaded_handlers_.find(command);
    if (loaded_it != loaded_handlers_.end()) {
        return loaded_it->second.get();
    }
    
    // Load the handler
    return load_handler(command);
}

bool LazyCommandLoader::has_command(const std::string& command) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return factories_.find(command) != factories_.end() || 
           loaded_handlers_.find(command) != loaded_handlers_.end();
}

std::vector<std::string> LazyCommandLoader::get_commands() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> commands;
    
    // Add factory commands
    for (const auto& [command, factory] : factories_) {
        commands.push_back(command);
    }
    
    // Add loaded commands (in case some were loaded directly)
    for (const auto& [command, handler] : loaded_handlers_) {
        if (std::find(commands.begin(), commands.end(), command) == commands.end()) {
            commands.push_back(command);
        }
    }
    
    std::sort(commands.begin(), commands.end());
    return commands;
}

void LazyCommandLoader::preload_commands(const std::vector<std::string>& commands) {
    for (const auto& command : commands) {
        get_handler(command); // This will load it if not already loaded
    }
}

void LazyCommandLoader::preload_all_commands() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [command, factory] : factories_) {
        if (loaded_handlers_.find(command) == loaded_handlers_.end()) {
            load_handler(command);
        }
    }
}

LazyCommandLoader::LoadingStats LazyCommandLoader::get_loading_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LoadingStats stats;
    stats.total_commands = factories_.size();
    stats.loaded_commands = loaded_handlers_.size();
    stats.total_load_time = total_load_time_;
    
    if (stats.loaded_commands > 0) {
        stats.average_load_time = std::chrono::milliseconds(
            total_load_time_.count() / stats.loaded_commands
        );
    }
    
    return stats;
}

CommandHandler* LazyCommandLoader::load_handler(const std::string& command) {
    // This method assumes mutex is already locked
    
    auto factory_it = factories_.find(command);
    if (factory_it == factories_.end()) {
        return nullptr;
    }
    
    // Measure loading time
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        auto handler = factory_it->second();
        if (!handler) {
            return nullptr;
        }
        
        CommandHandler* handler_ptr = handler.get();
        loaded_handlers_[command] = std::move(handler);
        
        // Update statistics
        auto end_time = std::chrono::steady_clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        total_load_time_ += load_time;
        
        // Cache hit for performance manager
        if (performance_manager_) {
            performance_manager_->record_cache_miss(); // First load is a miss
        }
        
        return handler_ptr;
    } catch (const std::exception&) {
        // Loading failed
        return nullptr;
    }
}

// LazyCommandDispatcher implementation

void LazyCommandDispatcher::register_lazy_handler(const std::string& command, CommandHandlerFactory factory) {
    lazy_loader_.register_factory(command, std::move(factory));
}

CommandResult LazyCommandDispatcher::dispatch(const std::vector<std::string>& args) {
    if (args.empty()) {
        error_handler_->report_invalid_arguments("No command specified", get_general_help());
        return CommandResult::InvalidArguments;
    }

    CommandArgs parsed_args = parse_args(args);
    
    // Handle special built-in commands
    if (parsed_args.command == "help") {
        // Route through the HelpModule handler so built-in help text
        // for unregistered commands (dev, init, audit, config, etc.) works.
        CommandHandler* help_handler = find_handler("help");
        if (help_handler) {
            CommandArgs help_args = parsed_args;
            help_args.command = "help";
            return help_handler->execute(help_args);
        }
        // Fallback if HelpModule not registered
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

std::vector<std::string> LazyCommandDispatcher::get_commands() const {
    // Combine regular commands and lazy commands
    auto regular_commands = CommandDispatcher::get_commands();
    auto lazy_commands = lazy_loader_.get_commands();
    
    std::vector<std::string> all_commands;
    all_commands.reserve(regular_commands.size() + lazy_commands.size());
    
    all_commands.insert(all_commands.end(), regular_commands.begin(), regular_commands.end());
    all_commands.insert(all_commands.end(), lazy_commands.begin(), lazy_commands.end());
    
    // Remove duplicates and sort
    std::sort(all_commands.begin(), all_commands.end());
    all_commands.erase(std::unique(all_commands.begin(), all_commands.end()), all_commands.end());
    
    return all_commands;
}

std::string LazyCommandDispatcher::get_command_help(const std::string& command) const {
    // Try regular commands first
    std::string help = CommandDispatcher::get_command_help(command);
    if (!help.empty()) {
        return help;
    }
    
    // Try lazy commands
    CommandHandler* handler = find_handler(command);
    if (handler) {
        return handler->get_help();
    }
    
    return "";
}

bool LazyCommandDispatcher::has_command(const std::string& command) const {
    return CommandDispatcher::has_command(command) || lazy_loader_.has_command(command);
}

void LazyCommandDispatcher::preload_common_commands() {
    // Preload commonly used commands for better performance
    std::vector<std::string> common_commands = {
        "help", "version", "compile", "run", "build", "test"
    };
    
    lazy_loader_.preload_commands(common_commands);
}

std::string LazyCommandDispatcher::get_general_help() const {
    return R"(Meld CLI - Unified command-line interface for the Meld programming language

Usage: meld <command> [options] [arguments]

Debugging:
  debug        Launch or attach a debugger to a compiled Meld binary
  profile      Profile CPU, memory, and call traces for a Meld program

Editor Integration:
  lsp          Start the Language Server Protocol daemon
  mcp          Start the Model Context Protocol server

Execution:
  build        Compile a project to a native binary via LLVM AOT
  dev          Start the ORC JIT hot-reload development server
  run          Execute a Meld program or start the interactive REPL

Infrastructure:
  daemon       Manage the meldd daemon lifecycle (start, stop, status, logs)
  vm           Manage the background Lima VM on macOS (start, stop, shell)

Project Management:
  audit        Display the effect permission tree for all dependencies
  fmt          Format .meld source files to the canonical style
  init         Initialize a Meld project in the current directory
  module       Manage packages and dependencies (install, list, update, fetch)
  new          Create a new Meld project from a template
  test         Discover and run @test functions in the project

Security & Signing:
  sign         Sign, verify, or bundle Meld binaries via Sigstore

Shell & Help:
  completion   Generate shell completion scripts (bash, zsh, fish)
  config       Read and write CLI configuration values
  help         Show help for a command or topic
  update       Update the meld toolchain to the latest version
  version      Display version, LLVM version, and build metadata

Use 'meld help <command>' for detailed help on a specific command.
Use 'meld help examples' to see usage examples.)";
}

CommandHandler* LazyCommandDispatcher::find_handler(const std::string& command) const {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto cache_it = handler_cache_.find(command);
        if (cache_it != handler_cache_.end()) {
            return cache_it->second;
        }
    }
    
    // Try regular handlers first
    CommandHandler* handler = CommandDispatcher::find_handler(command);
    if (handler) {
        // Cache the result
        std::lock_guard<std::mutex> lock(cache_mutex_);
        handler_cache_[command] = handler;
        return handler;
    }
    
    // Try lazy handlers
    handler = lazy_loader_.get_handler(command);
    if (handler) {
        // Cache the result
        std::lock_guard<std::mutex> lock(cache_mutex_);
        handler_cache_[command] = handler;
    }
    
    return handler;
}

} // namespace meld::cli